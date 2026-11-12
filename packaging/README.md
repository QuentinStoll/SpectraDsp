# Spectra installers

CI runs `build_installer.py` after each OS build and attaches the result to
GitHub Releases as:

- `Spectra-windows-x64.exe` / `Spectra-windows-arm64.exe` (Inno Setup, Standalone + VST3 checkboxes)
- `Spectra-macos-arm64.pkg` (Installer.app choices for Standalone, VST3, and AU; not notarized)
- `Spectra-linux-x64.run` (makeself or tar stub; zenity/kdialog checkboxes, or `--standalone` / `--vst3`)

```sh
python packaging/build_installer.py --plugin-root build/bin --os-arch linux-x64 --output-dir dist
```

`marketplace-site/` is the drop-in for ProfilerMarketplace so spectradsp.com
`/downloads/{macos,windows,linux}` 302s to those assets.
