# Roadmap

This is a proposed roadmap based on the repository's current limitations, not a committed release schedule. Maintainers should confirm priorities through the [project board](https://github.com/users/QuentinStoll/projects/5) and [issues](https://github.com/QuentinStoll/SpectraDsp/issues).

## Now

- Validate the documented Windows and Linux setup from clean checkouts.
- Establish private security and conduct reporting channels.
- Verify model loading, IR loading, profile recall, and the Library workflow in supported hosts.
- Keep critical automated checks passing and document reproducible failures.

## Next

- Load the macOS standalone app and Audio Unit in a host and record the result.
- Confirm the Windows and Linux builds still match the preset formats after the CMake format list is shared.
- Document a tested model compatibility matrix and asset-sharing workflow.
- Validate the next release's binaries and document its compatibility and release notes. The repository already has version tags; this roadmap does not reset that history.

## Later

- Harden standalone amplifier and cabinet cloning: the round trip, the training handoff, and the sealed tone that comes back to Library.
- Evaluate audio quality, latency, CPU use, and broader host compatibility.
- Expand usage examples and contributor-facing DSP documentation.

Propose changes through an issue before starting substantial work. Items move between horizons as validation and contributor availability change.
