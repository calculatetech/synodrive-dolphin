<proposed_plan>
# SDD-004: DEB and RPM Packaging

## Summary

Prepare version 0.3.0 as local release candidates. Add native CPack support and one containerized command for official Ubuntu 26.04 and Fedora 44 artifacts.

SDD-004 will not publish, tag, or upload version 0.3.0. SDD-005 must validate installation and removal before publication.

## Behavior and Interfaces

- **B1 — Official packages:** `./ci/package` accepts no arguments. It produces exactly:
  - `build/packages/ubuntu-26.04/synodrive-dolphin_0.3.0-1_amd64.deb`
  - `build/packages/fedora-44/synodrive-dolphin-0.3.0-1.x86_64.rpm`
- **B2 — Native packaging:** Compatible x86_64 hosts can invoke `cpack --config build/CPackConfig.cmake -G DEB|RPM` without the container wrapper.
- **B3 — Identity:** Both packages use version 0.3.0, release 1, MIT, the official product name, repository homepage, and the selected maintainer identity.
- **B4 — Payload:** Each package contains the executable, its distro-specific Dolphin overlay plugin, and `/usr/share/doc/synodrive-dolphin/copyright`. It contains no tests or Synology software.
- **B5 — Dependencies:** Enable native shared-library dependency generation. Add `dolphin` and the unscannable Nautilus runtime dependency for each format. Never declare Synology Drive as a distro dependency.
- **B6 — Isolation and failure:** Build each target once with read-only source, disabled container networking, and an ephemeral build tree. Stop on the first failure and never report a partial set as successful.
- **B7 — Existing behavior:** Keep production runtime code and `./ci/run` unchanged. Preserve caller-selected prefixes for ordinary source installation.

## Implementation

- Extend CMake with one monolithic CPack configuration:
  - Set the project version to 0.3.0 and the packaging prefix to `/usr`.
  - Use `DEB-DEFAULT` and `RPM-DEFAULT` filenames.
  - Enable `dpkg-shlibdeps` and RPM auto-requires.
  - Add `libnautilus-extension4` for DEB and `libnautilus-extension.so.4()(64bit)` for RPM.
  - Add `dolphin` explicitly because the installed module is a Dolphin plugin.
  - Install the existing MIT license as the package copyright file.
  - Do not add components, maintainer scripts, or a custom packaging framework.
- Add only `dpkg-dev` and `rpm-build` to the existing target images.
- Add a thin `./ci/package` wrapper:
  - Reject arguments with usage and exit 2 before Docker starts.
  - Process Ubuntu before Fedora and print artifact paths only after both succeed.
  - Remove stale matching package candidates but preserve unrelated files.
  - Validate filenames, metadata, architecture, complete payloads, and complete dependencies inside each target container.
  - Return final artifacts to the invoking user’s ownership.
- Create the SDD-004 ExecPlan and update the README, installation guide, technical report, release notes, and roadmap. Mark 0.3.0 as unreleased and defer package-install commands until SDD-005.

## Verification

- Add one dependency-free Python orchestration check covering:
  - argument rejection with zero Docker calls;
  - successful Ubuntu-then-Fedora ordering;
  - immediate stop on an Ubuntu failure;
  - nonzero completion on a later Fedora failure, without retry or success output;
  - read-only source, disabled network, ephemeral builds, and exact output mounts for both runs.
- Seed stale package files and unrelated sentinels before a real run. Require stale candidates to be replaced and sentinels to remain.
- Exercise direct `cpack -G DEB` and `cpack -G RPM` inside their native target environments.
- Inspect every payload and dependency record in both artifacts. Require the exact project files, generated ELF dependencies, explicit Dolphin/Nautilus dependencies, and no Synology content or dependency.
- Stage `cmake --install --prefix <temporary-path>` and require all three source-install files below that prefix.
- Run the host build and CTest suite, `./ci/run`, and `./ci/package`.
- Finish with documentation checks, `git diff --check`, boundary trace closure for B1–B7, and adversarial review.
- Do not install, remove, upgrade, sign, tag, release, or upload the packages in SDD-004.

## Assumptions and Prior Plan Reconciliation

- Official artifacts target Ubuntu 26.04 and Fedora 44 on x86_64. Native packages reflect the compatible host’s paths and package database.
- Partial artifacts may remain after failure, but the command returns nonzero and never presents them as a complete release set.
- Carried: the accepted runtime architecture, local CI contract, Drive 8.x support, performance design, and CPack packaging direction.
- Superseded: the roadmap clause that published packages during SDD-004. SDD-004 now produces candidates; SDD-005 gates publication.
- Historical engineering-task requirements remain superseded by the accepted plans.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-002-local-ci.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/plans/SDD-011-idle-performance.md -->
<!-- cpk-plan-spec: docs/plans/SDD-004-packaging.md -->
</proposed_plan>