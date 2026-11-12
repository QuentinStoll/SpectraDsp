# Contributing to VSTProfiler

Contributions to code, tests, documentation, and compatibility testing are welcome. Please follow the [code of conduct](CODE_OF_CONDUCT.md). For suspected vulnerabilities, follow the [security policy](SECURITY.md) instead of opening a public bug report.

## Contents

- [Plan your contribution](#plan-your-contribution)
- [Open an issue](#open-an-issue)
- [Set up the project](#set-up-the-project)
- [Create a branch](#create-a-branch)
- [Make and validate changes](#make-and-validate-changes)
- [Write commit messages](#write-commit-messages)
- [Submit a pull request](#submit-a-pull-request)
- [Review and merge](#review-and-merge)
- [Publish a release](#publish-a-release)

## Plan your contribution

Check existing issues and pull requests to avoid duplicate work. The [project board](https://github.com/users/QuentinStoll/projects/5) tracks ongoing priorities; new proposals are welcome too.

Discuss significant features, breaking changes, and architectural decisions in an issue or an early draft pull request. Keep each contribution focused so it is practical to review and test.

## Open an issue

Use the forms under [Issues](https://github.com/QuentinStoll/VstProfiler/issues/new/choose). Each issue gets a type badge and one area badge.

| Type | Use it for |
| --- | --- |
| Bug | A behavior that is wrong and can be reproduced |
| Feature | A new capability or a change to how the plugin behaves |
| Task | Implementation, documentation, or maintenance with a clear end |
| Question | A question about install, usage, or contributing |

| Area badge | Use it for |
| --- | --- |
| `audio` | Processing, capture, models, and impulse responses |
| `interface` | The plugin window and controls |
| `documentation` | README, changelog, and other docs |
| `build` | Build, tests, and CI |
| `security` | Private reporting, file loading, and trust boundaries |

The area dropdown on the form sets that badge. Do not also add `bug` or `enhancement` on top of the type badge.

A bug report needs the environment, the steps, and both the expected and actual result. A feature or task needs the outcome and how to tell that it is done.

## Set up the project

Follow the [README installation instructions](README.md#installation) for prerequisites and the [usage examples](README.md#usage) to try the application.

From the repository root, configure and build with the script for your platform:

**Windows**

```powershell
.\install.bat all default
```

**Linux and macOS**

```sh
bash ./install.sh all default
```

### Repository structure

| Directory | Contents |
| --- | --- |
| `Source/Include/` | Headers, shared interfaces, and styling |
| `Source/Src/` | Audio processor, editor, views, and UI modules |
| `Source/Assets/` | Images and other interface assets |
| `Tests/` | Automated tests and test support |
| `cmake/` | Dependencies, build presets, and compiler configuration |
| `packaging/` | Windows, macOS, and Linux installer scripts used by CI |
| `.github/` | Workflows and contribution templates |

## Create a branch

`main` is the only long-lived branch. Start from an up-to-date `main`, do the work on a short-lived branch, and open the pull request against `main`. There is no integration branch.

```sh
git switch main
git pull
git switch -c feature/profile-export
```

| Prefix | Purpose |
| --- | --- |
| `feature/` | New features or significant improvements |
| `fix/` | Bug fixes |
| `docs/` | Documentation, research notes, or architecture decisions |
| `chore/` | Maintenance, configuration, or dependency updates |

After the prefix, use only lowercase letters, digits, hyphens, underscores, or dots. Do not use spaces, uppercase letters, accented characters, or other special characters.

The pull request title still uses [Conventional Commits](#write-commit-messages) (`feat:`, `fix:`, `docs:`, `chore:`, `refactor:`). The branch prefix `feature/` and the title prefix `feat:` are the same kind of change.

The [branch-name workflow](.github/workflows/check-branch.yml) rejects a pull request whose base is not `main`. Existing branches named `feat/…` or `poc/…` are still accepted so older work can be merged. New feature work uses `feature/`.

## Make and validate changes

Follow the existing C++ style and the [.clang-format](.clang-format) configuration. CI checks C/C++ files under `Source` and `Tests` with `clang-format --dry-run --Werror`.

Add or update tests for meaningful behavior changes. Update documentation whenever installation or observable behavior changes, and add examples where they help explain a feature.

### Automated tests

**Windows**

```powershell
.\install.bat test
```

**Linux and macOS**

```sh
bash ./install.sh test
```

### Build and manual checks

Verify that affected application and plugin targets still build. Manually check affected profiling/model-loading, UI, and performance behavior. Record the commands, results, and steps needed to reproduce your checks, including relevant platform, host, audio-device, sample-rate, and buffer-size details.

For documentation-only changes, verify links, examples, and claims against the repository. Explain any build or audio checks you did not run and why.

Your contribution must comply with the project's [GPLv3-or-later notice](LICENCE.md), and you must have the right to contribute any included code or assets.

## Write commit messages

Use Conventional Commits with the format `type(scope): description`. The scope is optional and identifies an affected area such as `dsp`, `ui`, or `build`.

| Type | Purpose |
| --- | --- |
| `feat` | New functionality |
| `fix` | Bug fixes |
| `docs` | Documentation changes |
| `style` | Formatting without behavior changes |
| `refactor` | Code restructuring without a feature or bug fix |
| `perf` | Performance improvements |
| `test` | New or corrected tests |
| `chore` | Build, dependency, or CI maintenance |

Use a lowercase type and a space after the colon. Write a concise imperative description without a final period, keeping it under 50 characters where practical.

```text
feat(ui): add profile export controls
fix(dsp): correct noise gate bypass
docs: clarify Windows installation
```

## Submit a pull request

1. Push your branch to your fork or the repository you have access to.
2. Open a pull request against `main`.
3. Use the [pull request template](.github/PULL_REQUEST_TEMPLATE.md) to describe the change and its validation.
4. For work in progress, select **Create draft pull request** from the creation dropdown.
5. Address feedback, then mark the PR **Ready for review** when complete.

### PR title

Use the format `type(scope): description`, with one of the types accepted by CI: **`feat`, `fix`, `docs`, `chore`, or `refactor`**. For example: `docs: clarify Windows setup`.

The [PR-title workflow](.github/workflows/check-commits.yml) checks the PR title, despite its commit-check filename. It does not validate individual commit messages. The broader list above applies to commit messages; PR titles use this narrower list.

### Description and AI-assisted contributions

The PR description must be **human-written**. Explain what changed, why it was needed, and how reviewers can reproduce your validation. State which checks passed and disclose checks that were not run.

AI-assisted contributions are welcome only when a qualified human reviewer verifies, tests, and understands the code. The person submitting the PR must be able to explain its correctness and maintainability.

## Review and merge

At least one founder-team member must approve the change before it can be merged. Maintainers may request revisions or decline changes that do not fit the project's direction or quality expectations.

Missing information may result in a PR being closed. The final decision to merge rests with the maintainers.

## Publish a release

Merging to `main` does not publish a build. The pull request runs formatting, tests, coverage, and the plugin builds. That is the test of the branch.

To publish, run **CI/CD Pipeline** manually from the Actions tab, with the branch set to `main`. The workflow then creates a GitHub Release with one installer per OS (`Spectra-windows-x64.exe`, `Spectra-windows-arm64.exe`, `Spectra-macos-arm64.pkg`, `Spectra-linux-x64.run`) plus the portable zip/tar archives. spectradsp.com fetches those installer names from the latest release. Running the same workflow on a feature branch repeats the checks and does not publish. The macOS package is not notarized.
