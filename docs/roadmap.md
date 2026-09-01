# Synology Drive Dolphin Extension (Unofficial) Roadmap

This roadmap is the source of truth for accepted project work. Task identifiers are permanent and are never reused.

Move a task when its status changes. Never mark more than one task active.

Status legend:

- 🎯 **Active** — implementation or required review is in progress
- ⏭ **Planned next** — accepted and ordered for near-term delivery
- ◻ **Planned later** — accepted but not yet scheduled
- ⚠️ **Accepted residual risk** — bounded work is complete, but an accepted limitation remains
- ❌ **Declined** — considered and intentionally not supported
- ✅ **Completed** — implemented and validated

The v1.0 target supports x86_64 systems, KDE 6, and Synology Drive 8.x. GitHub releases will provide DEB and RPM packages.

Ubuntu 26.04 LTS and Fedora KDE 44 are the automated package targets. The RPM uses a community-packaged Synology Drive client.

## Active

No task is active.

## Planned next

- **SDD-004 — Produce DEB and RPM packages.**
  - Use the CPack DEB and RPM generators.
  - Publish one `synodrive-dolphin` package for each format on x86_64.
  - Use generated shared-library dependencies.
  - Do not include Synology software or require it from a distribution repository.

- **SDD-005 — Test the package lifecycle.**
  - Install and remove each package in disposable Ubuntu and Fedora containers.
  - Validate the executable, plugin, license, metadata, permissions, and shared-library resolution.
  - Make sure that package removal leaves no owned files.
  - Add an upgrade test after the first packaged release exists.

- **SDD-007 — Prove context-menu integration.**
  - Build a bounded prototype against the installed private helper.
  - List and invoke “Get link” and “Browse previous versions” from a separate process.
  - Support one local selection and return within 250 ms.
  - Never load Synology code into Dolphin.
  - Record the private ABI. Discard the prototype if either action is unreliable.

## Planned later

- **SDD-008 — Ship safe context-menu actions.**
  - Start this task only if SDD-007 succeeds.
  - Add a native KF6 `KAbstractFileItemActionPlugin` in the `kf6/kfileitemaction` namespace.
  - Show “Get link” and “Browse previous versions” for one eligible local file or directory.
  - Show no action after a timeout, unsupported selection, or helper error.
  - Include the plugin in both packages.

- **SDD-009 — Complete the v1.0 gate.**
  - Document the support matrix and package commands in the README, installation guide, and usage guide.
  - Add a focused bug-report form and `SECURITY.md`.
  - Request Fedora runtime reports and encourage fixes through pull requests.
  - Complete a real Ubuntu desktop smoke test for overlays, live transitions, Dolphin restart, context actions, and removal.
  - Complete the final correctness and documentation reviews before the `v1.0.0` tag.

- **SDD-010 — Add protected GitHub CI after v1.0.**
  - Start this task only after the `v1.0.0` release.
  - Run the local distribution gate for pushes and pull requests without path filters.
  - Give the workflow one unique required status check.
  - Require that check on `main` through branch protection.

- **SDD-006 — Automate tagged releases after v1.0.**
  - Start this task only after the `v1.0.0` release and SDD-010.
  - For a `vX.Y.Z` tag, require the tag and CMake project version to match.
  - Run all tests and build both packages.
  - Generate `SHA256SUMS` and GitHub provenance attestations.
  - Attach the packages, checksums, and attestations to the GitHub release.
  - Use minimum workflow permissions.

## Accepted residual risk

- Synology does not provide a supported Dolphin API. A private helper change can break this extension.
- Containers prove builds and package lifecycle. They do not prove live overlays or account-backed actions.
- Synology officially supports Ubuntu Linux. Fedora runtime support depends on community packaging and contributor reports.
- A failed context-menu prototype does not block v1.0. Move SDD-008 to Declined and document the result.

## Declined

The project does not plan these items for v1.0:

- Hosted APT or DNF repositories
- Project GPG key management
- ARM packages
- Selective-sync, lock, unlock, pause, or resume menu actions
- Static service menus that appear for unrelated files
- Loading Synology libraries directly into Dolphin
- A new diagnostics command

Use `synodrive-status <absolute-path>` for command-line troubleshooting.

## Completed

- **SDD-001 — Publish the v0.1.0 proof of concept.**
  - The MIT-licensed release provides isolated Dolphin status overlays for Synology Drive `8.0.2-17889`.

- **SDD-002 — Add local distribution CI.**
  - Provide one `./ci/run` command that builds and runs all tests in pinned `ubuntu:26.04` and `fedora:44` Docker containers.
  - Use the existing fake helper. Never download Synology software in CI.
  - Keep all GitHub Actions and branch protection work after `v1.0.0`.

- **SDD-003 — Support Synology Drive 8.x.**
  - Read the installed client's internal `INFO` metadata instead of the Debian package version.
  - Accept internal major 4, which corresponds to public Synology Drive 8.x.
  - Continue to require icon-overlay ABI 15, the expected library, symbols, structure, and status range.
  - Fail closed for malformed metadata, other majors, ABI changes, missing symbols, and unknown statuses.

Production readiness is complete when SDD-002 through SDD-005, SDD-007, and SDD-009 are complete.

SDD-008 must be complete or declined with evidence from SDD-007. Each non-trivial task gets an ExecPlan when it becomes active.
