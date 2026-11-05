# Changelog

Notable changes for every published release are recorded below, newest first. Release dates are the GitHub publication dates (UTC). Entries summarize the published release notes and changes between the corresponding Git tags; comparison links provide the complete commit history, including merges and maintenance changes.

## Unreleased

### Added

- Right-clicking a chain block offers Clear all blocks, which removes every effect block and leaves the input and output in place.
- The standalone window opens maximized. It can be restored and resized down to 1100×550.
- The Play view signal chain continues onto a second row as one serial path. The same block type can appear more than once, each copy is processed in order, and an occupied slot can be cleared from the block picker.
- macOS build and test jobs for Apple Silicon, producing the standalone app, VST3, and Audio Unit.
- Security policy and code of conduct, with an explicit placeholder for Quentin's private reporting email.
- Full GPLv3 license text alongside the existing GPLv3-or-later project notice.
- Bug, feature, task, and question issue forms. Bug, Feature, and Task set the issue type badge.
- Proposed Now / Next / Later roadmap.

### Changed

- Issue forms add an area badge: audio, interface, documentation, build, or security.
- Limited GTK, WebKitGTK, and libcurl to Linux so macOS can configure with the Xcode SDK alone.
- Plugin formats now follow the selected preset. Audio Unit is requested by the default preset and skipped outside macOS.
- Documented `install.bat` on Windows and `install.sh` on Linux and macOS as the installation entry points, including preset selection and the Windows build-configuration behavior.
- README setup, usage, architecture, testing, limitations, project status, and documentation links.
- Corrected standalone/VST3 output paths to match the project's CMake overrides.
- Clarified contribution conventions and acknowledged existing version tags in the roadmap and security policy.
- Switched contribution to a single `main` branch. Feature branches open pull requests directly against `main`, and a release is published only by running the CI workflow manually.
- Reformatted the project license notice using the provided GPL template structure.
- Replaced the generic email token with `quentin.stoll@epitech.eu`, explicitly marked as an unconfirmed placeholder in both reporting policies.
- Reconstructed the changelog for all seven published releases, from `0.1.0` through `v2026.07.15-112`.

## [v2026.07.15-112](https://github.com/QuentinStoll/VstProfiler/releases/tag/v2026.07.15-112) - 2026-07-15

### Added

- Automated tests for profile management, audio processing, profile/processor integration, logging, notification banners, and UI settings.
- CTest integration, test commands in the installation scripts, and coverage reporting with a default minimum line-coverage threshold of 70%.
- Windows coverage support through OpenCppCoverage and non-Windows coverage tooling through lcov/genhtml.
- Tracy performance profiling, dependency fetching, and preset-controlled instrumentation.
- Amp-loading success and failure notifications in the Clone view.
- Hardware/system information in diagnostic logging.
- Example release assets: `Archetype_amplifier.json` and `V30-4x12-cab.wav`.

### Changed

- Updated CI to run automated tests and coverage alongside platform builds before publishing releases.
- Added build-preset selection and parallel builds to the installation scripts.
- Reworked shared logging initialization and configuration.
- Allowed the `Dev`-to-`Main` promotion branch in the branch-name workflow.
- Published platform archives named `linux-x64.tar.gz`, `windows-x64.zip`, and `windows-arm64.zip`.

### Fixed

- Reused the shared logger across multiple processor instances instead of failing on duplicate initialization.
- Addressed logger naming conflicts and empty log files.
- Added missing CURL/Tracy dependencies for test builds and corrected Linux file handling in tests.
- Updated audio test expectations for mono processing and removed a duplicate GUI function call.

[Full comparison](https://github.com/QuentinStoll/VstProfiler/compare/v2026.07.14-98...v2026.07.15-112)

## [v2026.07.14-98](https://github.com/QuentinStoll/VstProfiler/releases/tag/v2026.07.14-98) - 2026-07-14

### Added

- Responsive output level meter in the Play view.
- Settings view with background-color customization and logging controls.
- VSTProfiler banner and application/plugin icon assets.

### Changed

- Completed the transition to CMake-generated JUCE headers.
- Improved mono amp processing and stereo output handling.

### Fixed

- Corrected noise-gate behavior, including bypass at a zero setting.
- Adjusted level-meter layout and UI formatting.

### Removed

- Legacy `Profiler.jucer` project and checked-in `JuceLibraryCode` generated files.

[Full comparison](https://github.com/QuentinStoll/VstProfiler/compare/v2026.07.10-69...v2026.07.14-98)

## [v2026.07.10-69](https://github.com/QuentinStoll/VstProfiler/releases/tag/v2026.07.10-69) - 2026-07-10

### Changed

- Moved master-volume processing to the end of the signal chain so it controls final output gain.
- Consolidated processor helper methods and updated README badges and contribution links.

### Fixed

- Added null checks for raw parameter access and validation for loaded amp models.
- Protected asynchronous path-selection callbacks with a safe component pointer.
- Initialized custom tabs to a default view.

### Removed

- Legacy amp engine, amp profiling, and sweep-generator code.
- Unused path-selector component and obsolete image asset.

[Full comparison](https://github.com/QuentinStoll/VstProfiler/compare/v2026.07.07-57...v2026.07.10-69)

## [v2026.07.07-57](https://github.com/QuentinStoll/VstProfiler/releases/tag/v2026.07.07-57) - 2026-07-07

### Fixed

- Corrected the case-sensitive README link to `contributing.md`.

This release only changes documentation; there are no audio-processing or UI code changes relative to the previous tag.

[Full comparison](https://github.com/QuentinStoll/VstProfiler/compare/v2026.07.06-56...v2026.07.07-57)

## [v2026.07.06-56](https://github.com/QuentinStoll/VstProfiler/releases/tag/v2026.07.06-56) - 2026-07-06

### Added

- RTNeural-based amplifier inference prototype and amp-model file loading.
- Profile management with local persistence, creation, editing, import, export, selection, and deletion confirmation.
- Export or create a profile from the current Play-view settings.
- File status cards and load/unload controls for amp and IR assets in the Clone view.
- Reusable forms, path fields, modal dialogs, profile cards, notification banners, and custom controls.
- Basic/advanced EQ views and connected gain, noise-gate, master-volume, and five-band EQ controls.
- Functional mute, EQ bypass, and profile-aware reset controls.

### Changed

- Replaced the old screen/view-manager UI with modular Clone, Play, and Profil views and JUCE-based tab navigation.
- Refreshed knob, toggle, button, slider, and profile-grid styling.
- Synchronized loaded amp/IR assets when selecting, editing, or resetting the active profile.
- Switched relevant presets from Debug to Release for processing performance.
- Reorganized and updated greenlight-review documentation.

### Fixed

- Applied EQ processing to both audio channels.
- Corrected profile-file error handling, default-profile behavior, and form values.
- Made UI images available when running as VST3 in a DAW.
- Added locking when unloading an amp model.
- Corrected Linux compilation issues, header casing, and build warnings.

[Full comparison](https://github.com/QuentinStoll/VstProfiler/compare/v2026.04.14-12...v2026.07.06-56)

## [v2026.04.14-12](https://github.com/QuentinStoll/VstProfiler/releases/tag/v2026.04.14-12) - 2026-04-14

### Added

- CMake build configuration, dependency fetching, presets, and Windows/Linux installation scripts.
- Configurable development logging and JSON configuration support.
- Gain-capture processing using reference and amplifier recordings.
- GitHub Actions workflows for formatting, platform builds, packaging, and release publication, plus branch-name and PR-title checks.
- `.clang-format` and `.clang-tidy` configuration.
- GPLv3-or-later project notice, contribution guidelines, and beta-test planning documents.
- Published Linux and Windows platform archives.

### Changed

- Reorganized sources into `Source/Include` and `Source/Src` and refactored the DSP engine.
- Replaced the early LUT distortion approach with a four-parameter dynamic waveshaper.
- Migrated builds away from reliance on Projucer-generated Visual Studio projects.
- Updated the README in English and removed its inconsistent MIT badge.

### Fixed

- Windows CMake compilation, include paths, dependency checks, and source filenames.
- Linux JUCE/system dependency resolution and package-version constraints.
- Logging-directory selection and the MSVC logging macro conflict.
- CI formatting scope and PR-title validation.

### Removed

- Checked-in generated Visual Studio build projects and superseded setup scripts.

[Full comparison](https://github.com/QuentinStoll/VstProfiler/compare/0.1.0...v2026.04.14-12)

## [0.1.0](https://github.com/QuentinStoll/VstProfiler/releases/tag/0.1.0) - 2025-12-11

The release notes describe this initial release as an experimental beta (`v0.1.0-beta`); its actual Git tag is `0.1.0`.

### Added

- Initial JUCE-based VST3 prototype targeting Windows.
- Sweep generation and output from the plugin, with manual capture through an external DAW.
- Impulse-response loading in the real-time audio path.
- Early LUT-based amp simulation and loading of paired DI/amplifier audio files.
- Initial cloning, selection, and playing screens, with rotary controls connected to processing.
- Projucer/Visual Studio project files and platform setup scripts.
- Published `Profiler.vst3` and raw/processed guitar and sweep WAV examples.

### Known limitations at release

- Sweep capture required an external DAW; automatic capture was not implemented.
- Amp processing and sound quality were preliminary. Complete automatic profiling and dynamic amp-behavior modeling remained future work.

[Initial release history](https://github.com/QuentinStoll/VstProfiler/commits/0.1.0)
