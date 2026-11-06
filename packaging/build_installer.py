#!/usr/bin/env python3
"""Build a Spectra installer from staged plugin binaries."""

from __future__ import annotations

import argparse
import os
import platform
import shutil
import stat
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent


def detect_os_arch(value: str | None) -> str:
    if value:
        return value
    system = platform.system().lower()
    machine = platform.machine().lower()
    arch = "arm64" if machine in {"arm64", "aarch64"} else "x64"
    if system == "windows" or system.startswith("cygwin"):
        return f"windows-{arch}"
    if system == "darwin":
        return f"macos-{arch}"
    return f"linux-{arch}"


def first_match(root: Path, patterns: list[str]) -> Path | None:
    for pattern in patterns:
        matches = sorted(path for path in root.glob(pattern) if path.exists())
        if matches:
            return matches[0]
    return None


def find_standalone(plugin_root: Path) -> Path | None:
    return first_match(
        plugin_root,
        [
            "Standalone/Spectra.app",
            "Standalone/Spectra.exe",
            "Standalone/Spectra",
            "Spectra.app",
            "Spectra.exe",
            "Spectra",
            "Standalone/Profiler.app",
            "Standalone/Profiler.exe",
            "Standalone/Profiler",
            "Profiler.app",
            "Profiler.exe",
            "Profiler",
        ],
    )


def find_vst3(plugin_root: Path) -> Path | None:
    return first_match(
        plugin_root,
        [
            "lib/VST3/Spectra.vst3",
            "VST3/Spectra.vst3",
            "Spectra.vst3",
            "lib/VST3/Profiler.vst3",
            "VST3/Profiler.vst3",
            "Profiler.vst3",
        ],
    )


def find_au(plugin_root: Path) -> Path | None:
    return first_match(
        plugin_root,
        [
            "lib/AU/Spectra.component",
            "AU/Spectra.component",
            "Spectra.component",
            "lib/AU/Profiler.component",
            "AU/Profiler.component",
            "Profiler.component",
        ],
    )


def standalone_dir(standalone: Path) -> Path:
    if standalone.suffix.lower() in {".exe", ".app"} or standalone.name in {"Spectra", "Profiler"}:
        return standalone.parent
    return standalone


def copytree(src: Path, dest: Path) -> None:
    if dest.exists():
        shutil.rmtree(dest)
    if src.is_file():
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dest)
        return
    shutil.copytree(src, dest, symlinks=True)


def run(command: list[str], **kwargs) -> None:
    print("+", " ".join(command), file=sys.stderr)
    subprocess.run(command, check=True, **kwargs)


def find_iscc() -> Path:
    env = os.environ.get("ISCC")
    candidates = []
    if env:
        candidates.append(Path(env))
    which = shutil.which("iscc") or shutil.which("ISCC")
    if which:
        candidates.append(Path(which))
    candidates.extend(
        [
            Path(r"C:\Program Files (x86)\Inno Setup 6\ISCC.exe"),
            Path(r"C:\Program Files\Inno Setup 6\ISCC.exe"),
        ]
    )
    for path in candidates:
        if path.is_file():
            return path
    raise FileNotFoundError("Inno Setup compiler (ISCC) not found")


def build_windows(args: argparse.Namespace, standalone: Path, vst3: Path) -> Path:
    output = args.output_dir / f"Spectra-{args.os_arch}.exe"
    iss = SCRIPT_DIR / "windows" / "Spectra.iss"
    iscc = find_iscc()
    defines = [
        f"/DAppVersion={args.version}",
        f"/DArch={args.os_arch.split('-', 1)[-1]}",
        f"/DStandaloneDir={standalone_dir(standalone)}",
        f"/DVst3Dir={vst3}",
        f"/DOutputDir={args.output_dir}",
        f"/DOutputName={output.stem}",
        f"/DLicenseFile={REPO_ROOT / 'LICENSE'}",
    ]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    run([str(iscc), *defines, str(iss)])
    if not output.is_file():
        raise FileNotFoundError(f"Installer was not produced: {output}")
    return output


def build_macos(args: argparse.Namespace, standalone: Path | None, vst3: Path | None, au: Path | None) -> Path:
    output = args.output_dir / f"Spectra-{args.os_arch}.pkg"
    script = SCRIPT_DIR / "macos" / "build-pkg.sh"
    command = ["bash", str(script), "--output", str(output), "--version", args.version]
    if standalone:
        command.extend(["--standalone", str(standalone)])
    if vst3:
        command.extend(["--vst3", str(vst3)])
    if au:
        command.extend(["--au", str(au)])
    args.output_dir.mkdir(parents=True, exist_ok=True)
    run(command)
    return output


def stage_linux_payload(standalone: Path | None, vst3: Path | None) -> Path:
    payload = args_temp_payload()
    install_src = SCRIPT_DIR / "linux" / "install.sh"
    shutil.copy2(install_src, payload / "install.sh")
    (payload / "install.sh").chmod((payload / "install.sh").stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    icon = REPO_ROOT / "Source" / "Assets" / "png" / "SpectraIcon.png"
    if icon.is_file():
        shutil.copy2(icon, payload / "spectra.png")
    if standalone:
        copytree(standalone_dir(standalone), payload / "standalone")
    if vst3:
        copytree(vst3, payload / "vst3" / vst3.name)
    return payload


def args_temp_payload() -> Path:
    payload = Path(os.environ.get("RUNNER_TEMP", "/tmp")) / "spectra-linux-payload"
    if payload.exists():
        shutil.rmtree(payload)
    payload.mkdir(parents=True)
    return payload


def build_linux(args: argparse.Namespace, standalone: Path | None, vst3: Path | None) -> Path:
    output = args.output_dir / f"Spectra-{args.os_arch}.run"
    payload = stage_linux_payload(standalone, vst3)
    script = SCRIPT_DIR / "linux" / "build-run.sh"
    args.output_dir.mkdir(parents=True, exist_ok=True)
    run(["bash", str(script), "--payload", str(payload), "--output", str(output), "--label", "Spectra installer"])
    output.chmod(output.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)
    return output


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--plugin-root", type=Path, default=REPO_ROOT / "build" / "bin")
    parser.add_argument("--os-arch", default=None)
    parser.add_argument("--output-dir", type=Path, default=REPO_ROOT / "dist")
    parser.add_argument("--version", default="0.0.0")
    args = parser.parse_args(argv)
    args.plugin_root = args.plugin_root.resolve()
    args.output_dir = args.output_dir.resolve()
    args.os_arch = detect_os_arch(args.os_arch)
    return args


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    plugin_root = args.plugin_root
    standalone = find_standalone(plugin_root)
    vst3 = find_vst3(plugin_root)
    au = find_au(plugin_root)
    family = args.os_arch.split("-", 1)[0]

    print(f"os-arch={args.os_arch}", file=sys.stderr)
    print(f"standalone={standalone}", file=sys.stderr)
    print(f"vst3={vst3}", file=sys.stderr)
    print(f"au={au}", file=sys.stderr)

    if family == "windows":
        if standalone is None or vst3 is None:
            print("Windows installer needs Standalone and VST3 under --plugin-root", file=sys.stderr)
            return 1
        output = build_windows(args, standalone, vst3)
    elif family == "macos":
        if standalone is None and vst3 is None and au is None:
            print("macOS installer needs Standalone, VST3, or AU under --plugin-root", file=sys.stderr)
            return 1
        output = build_macos(args, standalone, vst3, au)
    elif family == "linux":
        if standalone is None and vst3 is None:
            print("Linux installer needs Standalone or VST3 under --plugin-root", file=sys.stderr)
            return 1
        output = build_linux(args, standalone, vst3)
    else:
        print(f"Unsupported os-arch: {args.os_arch}", file=sys.stderr)
        return 1

    print(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
