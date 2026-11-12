"""Fingerprint the toolchain before restoring compiled public dependencies."""
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess


def command(*args):
    return subprocess.check_output(args, text=True, stderr=subprocess.STDOUT).strip()


def fingerprint():
    data = {
        "os": platform.platform(),
        "arch": platform.machine(),
        "image": os.environ.get("ImageVersion", "local"),
        "workspace": str(Path.cwd().resolve()),
        "cmake": command("cmake", "--version"),
        "cc": os.environ.get("CC", ""),
        "cxx": os.environ.get("CXX", ""),
        "cflags": os.environ.get("CFLAGS", ""),
        "cxxflags": os.environ.get("CXXFLAGS", ""),
        "generator": os.environ.get("CMAKE_GENERATOR", ""),
    }
    if os.name == "nt":
        vswhere = Path(os.environ["ProgramFiles(x86)"]) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installation = command(str(vswhere), "-latest", "-products", "*", "-requires",
                               "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath")
        if not installation:
            raise RuntimeError("No MSVC toolchain found")
        root = Path(installation) / "VC/Tools/MSVC"
        data["msvc"] = {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
                        for path in sorted(root.glob("*/bin/Host*/*/cl.exe"))}
        if not data["msvc"]:
            raise RuntimeError("No MSVC compiler found")
    else:
        compiler = os.environ.get("CXX") or shutil.which("c++")
        if not compiler:
            raise RuntimeError("No C++ compiler found")
        data["compiler"] = command(compiler, "--version")
    return hashlib.sha256(json.dumps(data, sort_keys=True).encode()).hexdigest()


if __name__ == "__main__":
    print("fingerprint=" + fingerprint())
