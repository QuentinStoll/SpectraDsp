<a id="readme-top"></a>

[![Epitech](https://img.shields.io/badge/Epitech-Paris-blue)](https://www.epitech.eu/fr/)
[![C++20](https://img.shields.io/badge/C++-20-blue)](CMakeLists.txt)
[![JUCE](https://img.shields.io/badge/JUCE-8.0.12-red)](cmake/dependencies.cmake)

![VSTProfiler banner](Source/Assets/png/ProfilerBanner.png)

# VSTProfiler - Guitar Amplifier Simulator

VSTProfiler is an open-source audio plugin for guitarists who want to shape and save amplifier tones using neural amp models, cabinet impulse responses, and real-time controls, in a DAW or a standalone application.

[Getting started](#getting-started) · [Usage](#usage) · [Contributing](#contributing) · [Report a bug](https://github.com/QuentinStoll/VstProfiler/issues) · [Request a feature](https://github.com/QuentinStoll/VstProfiler/issues)

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

**EIP 2025-2026 - Technical Track.** This final-year engineering project was started in fourth year by four students at Epitech Paris.

The goal is to give guitarists a free, customizable way to explore amplifier and cabinet sounds. Musicians can combine external models and impulse responses, adjust their tone, and reuse saved profiles. Developers and audio enthusiasts can inspect and extend the C++ processing chain and JUCE interface.

Reproducing the tone and dynamic behavior of real amplifiers is the project's longer-term ambition; the current implementation and its limits are described below.

## Features and current limitations

### Implemented features

- Load cabinet impulse responses through the **Clone** view and process audio with JUCE convolution.
- Load compatible neural amplifier models through RTNeural.
- Adjust gain, noise gate, master volume, mute, and a five-band EQ through the **Play** view.
- Arrange amp, cabinet, EQ, and pedal blocks on a two-row signal chain. The path stays serial, a type can be used more than once, and a block can be removed from the picker.
- Create, edit, import, export, and delete `.profilerprofile` profiles containing parameter values and external asset paths.
- Use a JUCE interface with **Clone**, **Play**, **Profil**, and settings views.
- Enlarge text and controls through **Settings > General > Interface scale** (100%–200%). The choice is saved locally; scrollbars keep the interface reachable in smaller windows.
- Enable **Settings > General > High contrast** for white text and outlines with yellow selection and focus indicators on black backgrounds. This preference is saved locally and updates open editors.
- Enable **Settings > General > Action status messages** for a persistent, selectable message at the bottom of the window. It reports navigation, completed control adjustments, signal-chain edits, settings, file and profile operations, and capture/marketplace results. Messages wrap and remain available until the next action or **Clear**; the preference is saved locally.
- Build a standalone application and a VST3 plugin on Windows, macOS, and Linux. macOS builds also produce an Audio Unit.

### Current limitations

- This is an experimental project. Audio quality, performance, and host compatibility still need validation for each use case.
- The **Clone** view loads existing files; it does not currently provide an amplifier capture or model-training workflow.
- Amp files must contain a model that the RTNeural JSON parser accepts, with one input and at least one output. The file chooser lists `.nam`, `.json`, and `.txt`, but an extension alone does not guarantee compatibility.
- Profiles reference external amp and IR files rather than embedding them. Keep those files available when reopening or sharing a profile.
- The processor currently requires mono input and stereo output. Configure the host's channel layout accordingly.
- CI builds and tests Windows x64, Linux x64, and macOS Apple Silicon. A local macOS run in a DAW still needs to be checked on a Mac.
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

Clone the repository and enter its directory:

```sh
git clone https://github.com/QuentinStoll/VstProfiler.git
cd VstProfiler
```

Configure and build on Windows using `install.bat`:

```powershell
.\install.bat all default
```

Configure and build on Linux or macOS using `install.sh`:

```sh
bash ./install.sh all default
```

The scripts compile from source; they do not install the plugin into your DAW's plugin directory. This project overrides JUCE's default output directories: the standalone application is under `build/bin/Standalone/`, and the VST3 bundle is under `build/bin/lib/VST3/`. On macOS, the Audio Unit is under `build/bin/lib/AU/`. Generated JUCE support files remain under `build/Source/Profiler_artefacts/`.

The `default` preset asks for VST3, Audio Unit, and standalone. Audio Unit is skipped automatically on Windows and Linux. Copy `Profiler.component` into `~/Library/Audio/Plug-Ins/Components` and rescan Audio Units in the host.

### Configuration

No `.env` file or API keys are required for the documented local workflow.

Build settings live in [cmake/presets_config.json](cmake/presets_config.json). Both scripts accept an action followed by an optional preset: `default`, `release`, `all-formats`, `dev`, or `debug`. To change an existing build's preset, run the `config` action first (for example, `.\install.bat config dev` or `bash ./install.sh config dev`), then run the `build` action. Use `.\install.bat --help` or `bash ./install.sh --help` to list the available actions.

The Windows build action does not pass a CMake `--config` option. With Visual Studio, it therefore uses the generator's default build configuration, which is normally Debug, even when the preset sets a Release build type. To explicitly build Release after configuring with the script, run `cmake --build build --config Release --parallel 8`.

The application stores settings and profiles in the JUCE user application-data directory under `Profiler`, with profiles in its `Profiles` subdirectory. On Windows, this is normally `%APPDATA%\Profiler`. On macOS, it is `~/Library/Application Support/Profiler`. External amp and IR files remain at their selected paths.

### Launch

On Windows:

```powershell
& ".\build\bin\Standalone\Profiler.exe"
```

On Linux or macOS:

```sh
./build/bin/Standalone/Profiler
```

For DAW use, copy the entire `build/bin/lib/VST3/Profiler.vst3` bundle to a VST3 location scanned by your host, and rescan plugins. Load **Profiler** on a track configured for mono input and stereo output. Build configurations share these output paths, so building Debug can replace an existing Release binary.

## Usage

### Play through an amp model and cabinet IR

1. Launch the standalone application and select your audio device, or load the plugin in your DAW and route your guitar input to it.
2. Open **Clone**, choose **Load Amp**, and select a compatible RTNeural model file.
3. Choose **Load IR** to select a cabinet impulse response. The chooser lists WAV, AIFF, AIF, and FLAC files.
4. Open **Play** and adjust gain, noise gate, EQ, and master volume while monitoring your input.
5. Check the file status cards if a model or IR does not load, and verify that the selected file still exists and is compatible.

### Save and reuse a tone

Use the export controls in **Play** to save the current settings as a profile or export a `.profilerprofile` file. Open **Profil** to create, import, edit, or delete profiles, and use the profile selector in **Play** to recall one. When sharing a profile, provide its referenced assets separately and update their paths on the receiving machine.

## Architecture

| Component | Responsibility |
| --- | --- |
| [PluginProcessor](Source/Src/PluginProcessor.cpp) | Audio processing, host parameters, model/IR loading, and plugin state. |
| [PluginEditor](Source/Src/PluginEditor.cpp), [Views](Source/Src/Views), and [Modules](Source/Src/Modules) | JUCE interface, controls, file selection, and profile workflows. |
| [ProfileManager](Source/Src/ProfileManager.cpp) | Profile validation, local JSON storage, import/export, and parameter recall. |
| [Source/Include](Source/Include) | Shared interfaces, settings paths, and styling definitions. |
| [cmake](cmake) | Dependency fetching, build presets, and compiler configuration. |
| [Tests](Tests) | Automated checks for audio processing, profiles, settings, logging, and UI components. |

Audio flows from the host or standalone audio device through gain, noise gate, and EQ processing, then the amp-model stage and cabinet convolution, before reaching the output. UI controls and recalled profiles update the processor's parameter state. Settings and profiles are stored locally; this workflow has no backend or database service.

## Tests

On Windows:

```powershell
.\install.bat test
```

On Linux or macOS:

```sh
bash ./install.sh test
```

These commands build `ProfilerTests` and run the CTest suite. Tests are enabled by default through `PROFILER_BUILD_TESTS`. The [CI workflow](.github/workflows/CI.yml) defines Windows, Linux, and macOS test jobs, plus a Windows coverage job.

## Contributing

Contributions to code, tests, documentation, and compatibility testing are welcome.

1. Read [contributing.md](contributing.md) and check existing issues and pull requests.
2. Discuss substantial changes in an issue or an early draft pull request.
3. Make a focused change, update relevant documentation, and run the tests. For audio or UI changes, also verify affected behavior manually.
4. Submit a pull request describing the change and how to reproduce your checks. A founder-team member must approve it before merging.

See the [issue tracker](https://github.com/QuentinStoll/VstProfiler/issues) for bugs, feature requests, and questions, and the [project board](https://github.com/users/QuentinStoll/projects/5) for ongoing work.

Use the repository's issue and pull request templates when submitting changes or feedback. Participation follows the [code of conduct](CODE_OF_CONDUCT.md).

## Security

See the [security policy](SECURITY.md) for vulnerability-reporting guidance and current support limitations. Quentin's proposed reporting email remains an unconfirmed placeholder; do not post vulnerability details in public issues.

## License

The project's [license notice](LICENCE.md) specifies **GNU GPL version 3 or later**. The complete GPLv3 text is included in [LICENSE](LICENSE). Third-party dependencies retain their own licenses.

## Project status

- **Maturity:** experimental; the CMake project version is currently `0.0.0`.
- **Maintenance:** an Epitech Paris student project developed by the VSTProfiler team.
- **Support:** use the issue tracker for questions and reproducible reports; no response-time guarantee is documented.
- **Planning and history:** see the proposed [roadmap](ROADMAP.md) and [changelog](CHANGELOG.md).
- **Open setup tasks:** confirm or replace the placeholder reporting email in the security and conduct policies, and check the macOS standalone app and Audio Unit in a host on a Mac.

[Back to top](#readme-top)
