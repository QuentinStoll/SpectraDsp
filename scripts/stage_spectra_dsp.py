#!/usr/bin/env python3
"""Copy a fetched SpectraDsp binary next to the built plugin bundles."""

import argparse
import os
import shutil
import sys

BUNDLE_DIR_NAMES = {
    "x86_64-win",
    "arm64-win",
    "x86_64-linux",
    "aarch64-linux",
    "MacOS",
    "Standalone",
}


def stage(library, plugin_root):
    version_file = os.path.join(os.path.dirname(library), "SPECTRA_DSP_VERSION.txt")
    runtime_name = os.path.basename(library)
    copied = 0
    for current, _dirs, files in os.walk(plugin_root):
        if os.path.basename(current) not in BUNDLE_DIR_NAMES:
            continue
        if not any(name.startswith(("Spectra", "Profiler")) for name in files):
            continue
        shutil.copy2(library, os.path.join(current, runtime_name))
        if os.path.isfile(version_file):
            shutil.copy2(version_file, os.path.join(current, "SPECTRA_DSP_VERSION.txt"))
        copied += 1
        print(f"Staged {runtime_name} in {current}", file=sys.stderr)
    if copied == 0:
        print(f"No Spectra bundle found under {plugin_root}", file=sys.stderr)
        return 1
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--library", required=True)
    parser.add_argument("--plugin-root", required=True)
    args = parser.parse_args()
    if not os.path.isfile(args.library):
        print(f"Missing SpectraDsp library: {args.library}", file=sys.stderr)
        return 1
    return stage(args.library, args.plugin_root)


if __name__ == "__main__":
    sys.exit(main())
