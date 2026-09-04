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

## Planned next

## Planned later

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

## Declined

The project does not plan these items for v1.0:

- **SDD-012 — Prove a persistent GLib-aware status worker.**
  - The worker stayed on `syncing` for 120 seconds after transfer completion.
  - The user selected the released version 0.4.0 runtime for version 1.0.0.
  - The failed experiment and its evidence remain available for future research.

- Hosted APT or DNF repositories
- Project GPG key management
- ARM packages
- Selective-sync, lock, unlock, pause, or resume menu actions
- Static service menus that appear for unrelated files
- Loading Synology libraries directly into Dolphin
- A general diagnostics command. The dedicated SDD-015 patch manager is an explicit exception.

Use `synodrive-status <absolute-path>` for command-line troubleshooting.

## Completed

- **SDD-010 — Add protected GitHub CI for v1.0.**
  - Protected `main` with pull requests, resolved conversations, strict checks, and no bypass actor.
  - Kept task-branch pushes and draft pull requests free of hosted CI runs.
  - Ran the local gate only after clean reviews.
  - Passed the required `distribution-gate` after the reviewed draft became ready.

- **SDD-009 — Complete the v1.0 gate.**
  - Kept the released version 0.4.0 runtime unchanged in version 1.0.0.
  - Passed the documentation, package-upgrade, desktop, review, protected-PR, and initial hosted CI gates.
  - Prepared version 1.0.0 for publication from the reviewed and validated `main` commit.

- **SDD-013 — Inspect Windows Get link behavior.**
  - Current Windows behavior matches Linux. No follow-on action is required.

- **SDD-015 — Add an opt-in tray left-click patch.**
  - Installed a user-invoked command that can inspect, apply, and restore the patch.
  - Made a normal tray-icon left click call Synology's styled menu function.
  - Kept source and package installation passive unless the user selects the source-install option.
  - Supported only recognized x86_64 internal-major-4 client binaries.
  - Completed the live field test before publication.

- **SDD-014 — Normalize local Places paths for overlays.**
  - Removed leading `/..` segments only from the path sent to Synology Drive.
  - Kept Dolphin cache keys and change notifications on the original URL.
  - Covered immediate Places notifications and internal parent segments with a regression test.

- **SDD-008 — Ship safe context-menu actions.**
  - Added a native KF6 `KAbstractFileItemActionPlugin` in the `kf6/kfileitemaction` namespace.
  - Added “Get link” and “Browse previous versions” for one eligible local file or directory.
  - Added fail-closed selection, timeout, helper, and activation handling.
  - Added the plugin and isolated action helper to the DEB and RPM packages.

- **SDD-007 — Prove context-menu integration.**
  - Proved both actions on Synology Drive `8.0.2-17889`, internal version `4.0.2-17889`, and ABI 15.
  - Listed and invoked “Get link” and “Browse previous versions” from a separate process.
  - Completed 20 list runs in 6.106 ms through 9.070 ms.
  - Kept Synology libraries outside Dolphin.
  - Recorded the private action names, handlers, requests, and Synology-owned window behavior.

- **SDD-005 — Test the package lifecycle.**
  - Install and remove each package in disposable Ubuntu and Fedora containers.
  - Validate the executable, plugin, license, metadata, permissions, and shared-library resolution.
  - Make sure that package removal leaves no owned files.
  - Test native upgrades from the published version 0.4.0 packages.

- **SDD-004 — Produce DEB and RPM packages.**
  - Use the CPack DEB and RPM generators.
  - Produce one `synodrive-dolphin` release candidate for each format on x86_64.
  - Use generated shared-library dependencies.
  - Do not include Synology software or require it from a distribution repository.
  - Keep publication behind the SDD-005 package-lifecycle gate.

- **SDD-001 — Publish the v0.1.0 proof of concept.**
  - The MIT-licensed release provides isolated Dolphin status overlays for Synology Drive `8.0.2-17889`.

- **SDD-002 — Add local distribution CI.**
  - Provide one `./ci/run` command that builds and runs all tests in pinned `ubuntu:26.04` and `fedora:44` Docker containers.
  - Use the existing fake helper. Never download Synology software in CI.
  - The later SDD-009 decision moves GitHub Actions and branch protection before `v1.0.0`.

- **SDD-003 — Support Synology Drive 8.x.**
  - Read the installed client's internal `INFO` metadata instead of the Debian package version.
  - Accept internal major 4, which corresponds to public Synology Drive 8.x.
  - Continue to require icon-overlay ABI 15, the expected library, symbols, structure, and status range.
  - Fail closed for malformed metadata, other majors, ABI changes, missing symbols, and unknown statuses.

- **SDD-011 — Eliminate idle overlay polling for v0.2.0.**
  - Query stable paths only when Dolphin requests their overlays.
  - Poll recent syncing paths once per second until they become stable or inactive.
  - Emit overlay notifications only when the visible overlay changes.
  - Preserve the isolated fresh-child status query.

Production readiness is complete when SDD-002 through SDD-005, SDD-007, SDD-009, and SDD-010 are complete.

SDD-008 must be complete or declined with evidence from SDD-007. Each non-trivial task gets an ExecPlan when it becomes active.
