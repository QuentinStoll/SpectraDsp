"""Train a NAM model on a Modal GPU from a Profiler amp capture folder.

The folder is the one written by Settings > Clone amp: input.wav and output.wav,
already aligned. The trained .nam is written back into the same folder.

    build\\nam-venv\\Scripts\\modal.exe run scripts/train_nam_modal.py --capture "<capture folder>"
"""

import base64
import json
import pathlib
import urllib.request

import modal

NAM_VERSION = "0.13.0"

image = (
    modal.Image.debian_slim(python_version="3.12")
    # The trainer imports tkinter and sounddevice even when no GUI runs.
    .apt_install("libportaudio2", "libtk8.6")
    .pip_install(f"neural-amp-modeler=={NAM_VERSION}", "cryptography")
    .env({"MPLBACKEND": "Agg"})
)
app = modal.App("profiler-nam-training", image=image)
models_volume = modal.Volume.from_name("profiler-nam-models", create_if_missing=True)


def _download(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=180) as response:
        return response.read()


def _put(url: str, payload: bytes) -> None:
    import urllib.error

    last_error = None
    for method in ("PUT", "POST"):
        request = urllib.request.Request(
            url,
            data=payload,
            method=method,
            headers={"Content-Type": "application/octet-stream", "x-upsert": "true"},
        )
        try:
            with urllib.request.urlopen(request, timeout=180) as response:
                if response.status < 300:
                    return
                last_error = RuntimeError(f"Upload failed with status {response.status}.")
        except urllib.error.HTTPError as exc:
            last_error = exc
            if exc.code not in (400, 404, 405) or method == "POST":
                raise
    if last_error is not None:
        raise last_error


def _notify(url: str, token: str, status: str, message: str = "", spectra: bytes | None = None) -> None:
    if not url or not token:
        return
    payload = {"status": status, "message": message}
    if spectra is not None:
        payload["spectra"] = base64.b64encode(spectra).decode("ascii")
    body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        method="POST",
        headers={"Content-Type": "application/json", "x-job-token": token},
    )
    with urllib.request.urlopen(request, timeout=60) as response:
        if response.status >= 300:
            raise RuntimeError(f"Could not mark the job {status}.")


def _seal(plain: bytes, key: bytes) -> bytes:
    import os

    from cryptography.hazmat.primitives.ciphers.aead import AESGCM

    nonce = os.urandom(12)
    sealed = AESGCM(key).encrypt(nonce, plain, None)
    body, tag = sealed[:-16], sealed[-16:]
    return b"SPEC" + bytes((1, 0, 0, 0)) + nonce + body + tag


def _use_a2_lite_only():
    """NAM's Colab trainer packs A2-Lite (3ch) and A2-Full (8ch). We only play 3ch."""
    from nam.train import core as nam_core

    packed_config = nam_core._get_packed_model_config

    def a2_lite():
        packed = packed_config()
        lite = next(
            sub
            for sub in packed["net"]["config"]["submodels"]
            if sub["name"] == "channels_3"
        )
        packed["net"] = {"name": "WaveNet", "config": lite["config"]}
        return packed

    nam_core._get_packed_model_config = a2_lite


@app.function(
    gpu="L4",
    cpu=4,
    memory=16384,
    timeout=2 * 60 * 60,
    volumes={"/models": models_volume},
)
def train(
    input_wav: bytes,
    output_wav: bytes,
    epochs: int,
    name: str,
    key_hex: str = "",
    upload_url: str = "",
    complete_url: str = "",
    job_token: str = "",
    input_url: str = "",
    output_url: str = "",
) -> bytes:
    try:
        return _train_capture(
            input_wav,
            output_wav,
            epochs,
            name,
            key_hex,
            upload_url,
            complete_url,
            job_token,
            input_url,
            output_url,
        )
    except Exception as exc:
        _notify(complete_url, job_token, "failed", str(exc)[:180])
        raise


def _train_capture(
    input_wav: bytes,
    output_wav: bytes,
    epochs: int,
    name: str,
    key_hex: str,
    upload_url: str,
    complete_url: str,
    job_token: str,
    input_url: str,
    output_url: str,
) -> bytes:
    import os

    from nam.models.metadata import UserMetadata
    from nam.train.colab import run

    if input_url and output_url:
        input_wav = _download(input_url)
        output_wav = _download(output_url)

    work = pathlib.Path("/tmp/capture")
    work.mkdir(parents=True, exist_ok=True)
    (work / "input.wav").write_bytes(input_wav)
    (work / "output.wav").write_bytes(output_wav)
    os.chdir(work)

    _use_a2_lite_only()
    run(epochs=epochs, user_metadata=UserMetadata(name=name))
    exported = sorted(work.glob("exported_models/*/*.nam"))
    if not exported:
        raise RuntimeError("NAM did not export a model. Read the data checks above.")

    nam_bytes = exported[-1].read_bytes()
    payload = _seal(nam_bytes, bytes.fromhex(key_hex)) if key_hex else nam_bytes
    pathlib.Path("/models", f"{name}.spectra" if key_hex else f"{name}.nam").write_bytes(payload)
    models_volume.commit()
    if upload_url:
        try:
            _put(upload_url, payload)
        except Exception:
            pass
    if complete_url:
        _notify(complete_url, job_token, "ready", spectra=payload)
        return b"ok"
    return payload


web_image = modal.Image.debian_slim(python_version="3.12").pip_install("fastapi")


@app.function(image=web_image, timeout=60)
@modal.fastapi_endpoint(method="POST", label="profiler-nam-train", requires_proxy_auth=True)
def start(payload: dict):
    """Spawn GPU training. Only profiler.audio calls this, with Modal proxy auth."""
    required = ("name", "keyHex", "inputUrl", "outputUrl", "uploadUrl", "completeUrl", "jobToken")
    if not isinstance(payload, dict) or any(not payload.get(key) for key in required):
        return {"error": "Missing training fields."}
    train.spawn(
        b"",
        b"",
        int(payload.get("epochs") or 400),
        str(payload["name"])[:80],
        str(payload["keyHex"]),
        str(payload["uploadUrl"]),
        str(payload["completeUrl"]),
        str(payload["jobToken"]),
        str(payload["inputUrl"]),
        str(payload["outputUrl"]),
    )
    return {"started": True}


@app.local_entrypoint()
def main(
    capture: str = "",
    epochs: int = 400,
    name: str = "",
    input_url: str = "",
    output_url: str = "",
    upload_url: str = "",
    key_hex: str = "",
    complete_url: str = "",
    job_token: str = "",
):
    if input_url and output_url:
        input_wav = b""
        output_wav = b""
        name = name or "Amp"
    else:
        folder = pathlib.Path(capture)
        for required in ("input.wav", "output.wav"):
            if not (folder / required).is_file():
                raise SystemExit(f"{folder / required} is missing.")
        input_wav = (folder / "input.wav").read_bytes()
        output_wav = (folder / "output.wav").read_bytes()
        name = name or folder.name

    try:
        model = train.remote(
            input_wav,
            output_wav,
            epochs,
            name,
            key_hex,
            upload_url,
            complete_url,
            job_token,
            input_url,
            output_url,
        )
    except Exception as exc:
        if complete_url and job_token:
            try:
                _notify(complete_url, job_token, "failed", str(exc)[:180])
            except Exception:
                pass
        if not upload_url:
            print(f"Client lost the RPC ({exc}). Reading the Volume instead...")
            models_volume.reload()
            suffix = "spectra" if key_hex else "nam"
            model = b"".join(models_volume.read_file(f"{name}.{suffix}"))
        else:
            raise

    if upload_url:
        print("Uploaded sealed model.")
        return

    folder = pathlib.Path(capture)
    target = folder / (f"{name}.spectra" if key_hex else f"{name}.nam")
    target.write_bytes(model)
    if key_hex:
        print(f"Saved {target}")
        return
    parsed = json.loads(model)
    training = parsed.get("metadata", {}).get("training", {})
    print(f"Saved {target}")
    print(f"Architecture: {parsed.get('architecture')}")
    print(f"Validation ESR: {training.get('validation_esr')}")
