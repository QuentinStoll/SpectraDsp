# Security policy

## Project status and scope

SpectraDsp is experimental. Existing version tags do not establish a security-support commitment: this repository does not document a supported-release schedule. Reports should identify the affected tag or commit and whether the issue reproduces on the current development code. Older snapshots do not have a guaranteed backport policy.

This policy covers code maintained in this repository, including model/IR loading, profile import/export, and plugin processing. Vulnerabilities in dependencies may also need to be reported to their upstream maintainers.

## Reporting a vulnerability

The designated private reporting contact is Quentin. The proposed email is **`quentin.stoll@epitech.eu` (PLACEHOLDER - unconfirmed reporting address)**. Confirm this address and its use for security reports before publication or sending sensitive information.

Until the address is confirmed, use **Report a vulnerability** on the repository's [Security page](https://github.com/QuentinStoll/SpectraDsp/security) if that option is available. Otherwise, ask for a private contact without posting vulnerability details. Do not include exploit files, credentials, or personal data in a public issue.

Once a private channel is available, include:

- The affected commit or version, operating system, plugin format, and host version.
- A concise description of the potential impact.
- Minimal reproduction steps and, where necessary, a sanitized sample file.
- Any relevant logs, with personal paths and sensitive data removed.
- Whether a workaround is known.

Do not exploit a vulnerability beyond what is necessary to demonstrate it. Coordinate disclosure with the maintainers so a fix or mitigation can be prepared. No response or resolution deadline is currently guaranteed.

## Maintainer follow-up

- Confirm or replace the placeholder `quentin.stoll@epitech.eu` and verify that reports can be received before removing its placeholder label.
- Define supported versions and a realistic response process before a stable release.
