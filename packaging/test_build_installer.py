#!/usr/bin/env python3
"""Smoke-test installer discovery and the Linux .run payload."""

from __future__ import annotations

import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

PACKAGING = Path(__file__).resolve().parent
sys.path.insert(0, str(PACKAGING))

import build_installer  # noqa: E402


class FindBinariesTests(unittest.TestCase):
    def test_finds_linux_layout(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            (root / "Standalone").mkdir()
            (root / "Standalone" / "Profiler").write_text("elf\n")
            (root / "lib" / "VST3" / "Profiler.vst3" / "Contents").mkdir(parents=True)
            (root / "lib" / "VST3" / "Profiler.vst3" / "Contents" / "Profiler.so").write_text("so\n")
            self.assertEqual(build_installer.find_standalone(root), root / "Standalone" / "Profiler")
            self.assertEqual(build_installer.find_vst3(root), root / "lib" / "VST3" / "Profiler.vst3")


class LinuxInstallerTests(unittest.TestCase):
    def test_run_installs_selected_components(self) -> None:
        with tempfile.TemporaryDirectory() as raw:
            root = Path(raw)
            plugin_root = root / "bin"
            (plugin_root / "Standalone").mkdir(parents=True)
            binary = plugin_root / "Standalone" / "Profiler"
            binary.write_text("#!/bin/sh\necho spectra\n")
            binary.chmod(binary.stat().st_mode | stat.S_IXUSR)
            vst3 = plugin_root / "lib" / "VST3" / "Profiler.vst3" / "Contents"
            vst3.mkdir(parents=True)
            (vst3 / "Profiler.so").write_text("plugin\n")

            output_dir = root / "dist"
            dest = root / "install"
            env = os.environ.copy()
            env["RUNNER_TEMP"] = str(root / "tmp")
            subprocess.run(
                [
                    sys.executable,
                    str(PACKAGING / "build_installer.py"),
                    "--plugin-root",
                    str(plugin_root),
                    "--os-arch",
                    "linux-x64",
                    "--output-dir",
                    str(output_dir),
                    "--version",
                    "0.0.1",
                ],
                check=True,
                env=env,
            )
            installer = output_dir / "Spectra-linux-x64.run"
            self.assertTrue(installer.is_file(), installer)
            self.assertTrue(os.access(installer, os.X_OK))

            prefix = dest / "opt"
            vst3_dir = dest / "vst3"
            xdg = dest / "xdg"
            env["XDG_DATA_HOME"] = str(xdg)
            subprocess.run(
                [
                    str(installer),
                    "--yes",
                    "--standalone",
                    "--vst3",
                    "--prefix",
                    str(prefix),
                    "--vst3-dir",
                    str(vst3_dir),
                ],
                check=True,
                env=env,
            )
            self.assertTrue((prefix / "Profiler").exists())
            self.assertTrue((vst3_dir / "Profiler.vst3" / "Contents" / "Profiler.so").is_file())
            self.assertTrue((xdg / "applications" / "spectra.desktop").is_file())


if __name__ == "__main__":
    unittest.main()
