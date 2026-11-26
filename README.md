<a id="readme-top"></a>

[![C++20](https://img.shields.io/badge/C++-20-blue)](CMakeLists.txt)
[![JUCE](https://img.shields.io/badge/JUCE-8.0.12-red)](cmake/dependencies.cmake)

<p align="center">
  <img src="docs/logo.png" alt="SpectraDsp" width="280">
</p>

# SpectraDsp

SpectraDSP is free guitar software for PC, Mac, and Linux. The standalone application and the plugin are named **Spectra**. A high-quality WaveNet, adapted for real time, clones an amplifier or a speaker with very high fidelity. One click clones an amplifier and trains it in the cloud. One click clones a speaker, and that impulse response is ready almost at once. It is the easiest guitar software to use. It plays those tones in a DAW or on its own, and loads tones from the [spectradsp.com](https://spectradsp.com) library. It also clones clean amplifiers, and raw amplifiers for raw black and raw death.

[Getting started](#getting-started) · [Usage](#usage) · [Contributing](#contributing) · [Report a bug](https://github.com/QuentinStoll/SpectraDsp/issues) · [Request a feature](https://github.com/QuentinStoll/SpectraDsp/issues)

## Table of contents

- [Why this project](#why-this-project)
- [Features and current limitations](#features-and-current-limitations)
- [Getting started](#getting-started)
- [Usage](#usage)
- [Architecture](#architecture)
- [Tests](#tests)
- [Contributing](#contributing)
- [Security](#security)
- [License](#license)
- [Project status](#project-status)

## Why this project

SpectraDSP is professional guitar software. Play an amp model through a cabinet, and come back to the same tone later. The marketplace at spectradsp.com is where account tones live. One click in the standalone app clones an amplifier in the cloud, or a speaker. The engine is a high-quality WaveNet adapted for real time, with very high fidelity.

## Features and current limitations

### Implemented features

- The amplifier clone is a high-quality WaveNet adapted for real time, with very high fidelity. It is not a GRU, and it is not a lighter stand-in for a WaveNet. Cabinet processing uses convolution.
- One click clones an amplifier and sends the training to the cloud. The sealed tone shows up in Library when it is ready.
- One click clones a speaker. That impulse response is ready almost at once.
- One click clones a distortion pedal with the same capture and cloud training as an amplifier. The sealed pedal loads in the pedal slot, in front of the amp. Screamer, Distortion, and Boost stay available when no clone is loaded.
- Adjust input trim, noise gate, amp gain, mute, master volume, and a five-band EQ from the studio.
- Recall a tone from the profile menu in the top bar. Profiles are `.profilerprofile` files: parameter values plus paths to external amp and IR files.
- Sign in from **Library** and use tones from the spectradsp.com account.
- Build a standalone application and a VST3 plugin on Windows, macOS, and Linux. macOS builds also produce an Audio Unit.

### Current limitations

- Cloning an amplifier or a speaker runs in the standalone application, which owns the audio device, and needs a signed-in spectradsp.com account. The plugin plays the clones; it does not run the capture.
- A SpectraDSP clone is a WaveNet trained in the cloud. An imported amplifier file is a separate path. The file chooser lists `.nam`, `.json`, and `.txt`, but an extension alone does not guarantee the file will load.
- Profiles reference external amp and IR files rather than embedding them. Keep those files available when reopening or sharing a profile.
- The processor currently requires mono input and stereo output. Configure the host's channel layout accordingly.
- CI builds and tests Windows x64, Linux x64, and macOS Apple Silicon, and attaches a per-OS installer to GitHub Releases. A local macOS run in a DAW still needs to be checked on a Mac.
- AAX is requested by some presets but is not built unless the Avid AAX SDK is present. That SDK requires an Avid developer agreement and is not part of the default build.

## Getting started

### Prerequisites

- Git, CMake **3.22 or later**, and a **C++20** compiler.
- Internet access for the first configure step: CMake fetches JUCE **8.0.12**, spdlog, simdjson, RTNeural, and Tracy. See [dependency configuration](cmake/dependencies.cmake) for the selected versions.
- On Windows: Visual Studio 2022 or Build Tools with the **Desktop development with C++** workload and a Windows SDK. Run commands from a developer terminal.
- On macOS: Xcode, or the Xcode Command Line Tools, which include Clang and the macOS SDK. No extra package such as GTK is required. Building VST3, Audio Unit, and the standalone app does not require a paid license. Notarizing a build so other people can open it without a Gatekeeper warning requires the Apple Developer Program.
- On Linux: a C++20-capable GCC or Clang toolchain and the JUCE system dependencies below.
- For live guitar input: an audio interface and a configured audio input/output device. The VST3 build also needs a compatible plugin host. On macOS, the Audio Unit can be loaded by hosts that scan `~/Library/Audio/Plug-Ins/Components`.

For Debian/Ubuntu, the system libraries used by the repository's CI can be installed with:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake git pkg-config \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev \
  libfreetype6-dev libfontconfig1-dev libasound2-dev libcurl4-openssl-dev \
  libssl-dev libwebkit2gtk-4.1-dev libgtk-3-dev libglib2.0-dev \
  mesa-common-dev libjack-jackd2-dev libdbus-1-dev
```

### Installation

Prebuilt installers are attached to [GitHub Releases](https://github.com/QuentinStoll/SpectraDsp/releases). Each installer lets you choose the standalone application and/or the plugin (VST3, and Audio Unit on macOS). [spectradsp.com](https://spectradsp.com) downloads the latest of those release assets.

- Windows: `Spectra-windows-x64.exe` (ARM64: `Spectra-windows-arm64.exe`)
- macOS Apple Silicon: `Spectra-macos-arm64.pkg`. The package is not notarized; Gatekeeper may require Open anyway.
- Linux x64: `Spectra-linux-x64.run`

To build from source, clone the repository and enter its directory:

```sh
git clone https://github.com/QuentinStoll/SpectraDsp.git
cd SpectraDsp
```

GitHub still accepts the previous repository URL and redirects it here.

Configure and build on Windows using `install.bat`:

```powershell
.\install.bat all default
```

Configure and build on Linux or macOS using `install.sh`:

```sh
bash ./install.sh all default
```

The scripts compile from source; they do not install the plugin into your DAW's plugin directory. This project overrides JUCE's default output directories: the standalone application is under `build/bin/Standalone/`, and the VST3 bundle is under `build/bin/lib/VST3/`. On macOS, the Audio Unit is under `build/bin/lib/AU/`. Generated JUCE support files remain under `build/Source/Profiler_artefacts/`.

The `default` preset asks for VST3, Audio Unit, and standalone. Audio Unit is skipped automatically on Windows and Linux. Copy `Spectra.component` into `~/Library/Audio/Plug-Ins/Components` and rescan Audio Units in the host.

### Configuration

No `.env` file or API keys are required for the documented local workflow.

Build settings live in [cmake/presets_config.json](cmake/presets_config.json). Both scripts accept an action followed by an optional preset: `default`, `release`, `all-formats`, `dev`, or `debug`. To change an existing build's preset, run the `config` action first (for example, `.\install.bat config dev` or `bash ./install.sh config dev`), then run the `build` action. Use `.\install.bat --help` or `bash ./install.sh --help` to list the available actions.

The Windows build action does not pass a CMake `--config` option. With Visual Studio, it therefore uses the generator's default build configuration, which is normally Debug, even when the preset sets a Release build type. To explicitly build Release after configuring with the script, run `cmake --build build --config Release --parallel 8`.

The application stores settings and profiles in the JUCE user application-data directory under `Spectra`, with profiles in its `Profiles` subdirectory. On Windows, this is normally `%APPDATA%\Spectra`. On macOS, it is `~/Library/Application Support/Spectra`. External amp and IR files remain at their selected paths.

### Launch

On Windows:

```powershell
& ".\build\bin\Standalone\Spectra.exe"
```

On Linux or macOS:

```sh
./build/bin/Standalone/Spectra
```

For DAW use, copy the entire `build/bin/lib/VST3/Spectra.vst3` bundle to a VST3 location scanned by your host, and rescan plugins. Load **Spectra** on a track configured for mono input and stereo output. Build configurations share these output paths, so building Debug can replace an existing Release binary.

## Usage

### Play through an amp model and cabinet IR

1. Launch the standalone application and select your audio device, or load the plugin in your DAW and route your guitar input to it.
2. In the studio, open the amp block and choose a compatible model. Open the cabinet block and choose an impulse response. The chooser lists WAV, AIFF, AIF, and FLAC files.
3. Adjust input trim, noise gate, gain, EQ, and master volume while monitoring your input.
4. Recall a saved tone from the profile menu in the top bar. Open **Library** to sign in and use tones from the spectradsp.com account.

### Clone a cabinet or an amplifier

One click clones an amplifier. Training runs in the cloud, and the tone appears in Library when it is ready. One click clones a speaker. That impulse response is ready almost at once. One click clones a distortion pedal with the same capture. The pedal loads in the pedal slot, in front of the amp. These actions are in the standalone app, after you sign in from **Library**.

### Profiles

Profiles use the `.profilerprofile` extension and live in the Spectra application-data `Profiles` folder. They store parameter values and the paths of the amp and IR files they use. When sharing a profile, provide those files separately and update their paths on the receiving machine.

## Architecture

| Component | Responsibility |
| --- | --- |
| [PluginProcessor](Source/Src/PluginProcessor.cpp) | Audio processing, host parameters, model/IR loading, and plugin state. |
| [PluginEditor](Source/Src/PluginEditor.cpp), the studio, and [Modules](Source/Src/Modules) | JUCE interface, signal chain, file selection, Library, and Settings. |
| [ProfileManager](Source/Src/ProfileManager.cpp) | Profile validation, local JSON storage, import/export, and parameter recall. |
| Marketplace client | spectradsp.com sign-in, the account library, and sealed tone download. |
| [Source/Include](Source/Include) | Shared interfaces, settings paths, and styling definitions. |
| [cmake](cmake) | Dependency fetching, build presets, and compiler configuration. |
| [Tests](Tests) | Automated checks for audio processing, profiles, settings, logging, marketplace, and UI components. |

Audio flows from the host or standalone audio device through the signal chain: input and noise gate, the amp-model stage, cabinet convolution, EQ, and level, before the output. UI controls and recalled profiles update the processor's parameter state. Settings and profiles are stored locally. Account tones come from spectradsp.com.

## Tests

On Windows:

```powershell
.\install.bat test
```

On Linux or macOS:

```sh
bash ./install.sh test
```

These commands build the `ProfilerTests` executable and run the CTest suite. Tests are enabled by default through `PROFILER_BUILD_TESTS`. The [CI workflow](.github/workflows/CI.yml) defines Windows, Linux, and macOS test jobs, plus a Windows coverage job. A manual workflow dispatch on main publishes installers on the GitHub Release. Commits on main publish the 48 kHz latencies and realtime ratios at https://quentinstoll.github.io/SpectraDsp/dev/bench/.

## Contributing

Contributions to code, tests, documentation, and compatibility testing are welcome.

1. Read [contributing.md](contributing.md) and check existing issues and pull requests.
2. Discuss substantial changes in an issue or an early draft pull request.
3. Make a focused change, update relevant documentation, and run the tests. For audio or UI changes, also verify affected behavior manually.
4. Submit a pull request describing the change and how to reproduce your checks. A founder-team member must approve it before merging.

See the [issue tracker](https://github.com/QuentinStoll/SpectraDsp/issues) for bugs, feature requests, and questions, and the [project board](https://github.com/users/QuentinStoll/projects/5) for ongoing work.

Use the repository's issue and pull request templates when submitting changes or feedback. Participation follows the [code of conduct](CODE_OF_CONDUCT.md).

## Security

See the [security policy](SECURITY.md) for vulnerability-reporting guidance and current support limitations. Quentin's proposed reporting email remains an unconfirmed placeholder; do not post vulnerability details in public issues.

## License

The project's [license notice](LICENCE.md) specifies **GNU GPL version 3 or later**. The complete GPLv3 text is included in [LICENSE](LICENSE). Third-party dependencies retain their own licenses.

## Project status

- **Maturity:** professional guitar software. The engine is a high-quality WaveNet adapted for real time.
- **Maintenance:** developed by SpectraDSP.
- **Support:** use the issue tracker for questions and reproducible reports; no response-time guarantee is documented.
- **Planning and history:** see the proposed [roadmap](ROADMAP.md) and [changelog](CHANGELOG.md).
- **Open setup tasks:** confirm or replace the placeholder reporting email in the security and conduct policies, and check the macOS standalone app and Audio Unit in a host on a Mac.

[Back to top](#readme-top)
