# Release version 1.0 from the version 0.4 runtime

This ExecPlan is a living specification until review closure. Maintain it in accordance with `/home/mbeutler/.codex/PLANS.md`.

## Purpose / Big Picture

Version 1.0 gives users a documented and validated stable release without changing the proven version 0.4 runtime. Users can install one Ubuntu DEB or Fedora RPM, upgrade from version 0.4, use overlays and file actions, and report defects or security problems through documented channels.

## Decision Log

- Decision: Use commit `b055b80cd9f131de5cef86427d0f7d26cfeffec6` as the runtime base.
  Rationale: It is the published version 0.4.0 tag, and its runtime passed the accepted performance and field gates.
- Decision: Decline SDD-012 for version 1.0.
  Rationale: Its persistent worker remained stale for 120 seconds after synchronization completed.
- Decision: Complete SDD-010 inside one combined draft pull request.
  Rationale: The user moved protected GitHub CI before version 1.0 and requested a fast-tracked release.
- Decision: Add `./ci/package --upgrade-from DIR`.
  Rationale: It gives the published-version upgrade obligation one repeatable command without adding network access to package scripts.
- Decision: Retire the inherited legal-review requirement.
  Rationale: The user explicitly superseded that requirement and retained the MIT and no-Synology-payload boundaries.
- Decision: Publish only the DEB and RPM.
  Rationale: Checksums and automated release assets remain owned by SDD-006.

## Outcomes & Retrospective

Version 1.0 retains the version 0.4 overlay, action, compatibility, performance, and optional tray-patch behavior. The release adds a protected delivery path, a native package-upgrade proof, complete user documentation, and focused support channels.

## Context and Orientation

`CMakeLists.txt` owns the project and CPack version. `ci/package`, `ci/validate-package`, and `ci/validate-package-lifecycle` own package creation and validation. `tests/test_package_command.py` proves their command boundaries with fake native tools.

The README, `docs/INSTALL.md`, `docs/USAGE.md`, `docs/RELEASE_NOTES.md`, and `docs/SYNODRIVE_DOLPHIN_RECON.md` own user and technical facts. `SECURITY.md` and `.github/ISSUE_TEMPLATE/bug_report.yml` provide focused report paths. `docs/roadmap.md` owns task state. SDD-010 in `docs/plans/SDD-010-protected-github-ci.md` owns GitHub protection and CI.

The supported system is x86_64 Linux with KDE and Qt 6, Synology Drive public version 8.x, internal major 4, and overlay ABI 15. Ubuntu 26.04 is the supported live target. Fedora 44 package behavior is validated, while client runtime availability depends on community packaging.

## Product Boundary

The product boundary follows `/home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md` and `/home/mbeutler/.agents/skills/design-preflight/references/supported-model.md`.

Version identity, package upgrade validation, release documentation, the Ubuntu desktop smoke, roadmap closure, tag, and GitHub release compose with this task. The version 0.4 runtime is opaque. Synology software and its private implementation are external and remain outside every package.

The task excludes persistent-worker code, runtime behavior changes, extra platforms or actions, project signing, attestations, hosted package repositories, ARM packages, checksum assets, and tag-triggered publication.

The accepted plan is `.agent/plan-history/plan-summary.20260904T030553255401Z.890479e1a919a22ebb44fbae67c784f36e2c4699a5489d33f0b470aabb1f0208.md`.

That plan supersedes the earlier post-version-1.0 CI timing, the pending SDD-012 direction, and the initial legal-review gate. It carries all published runtime, compatibility, isolation, package, context-action, performance, and tray-patch decisions.

## Scenario Proof

The candidate must have no runtime source change from version 0.4.0. Current version owners must say 1.0.0; retained 0.4.0 text must be release history or the explicit upgrade baseline.

`./ci/package --upgrade-from DIR` must verify the published v0.4.0 DEB and RPM digests. For each format, it must complete both a clean candidate lifecycle and a native v0.4.0-to-v1.0.0 upgrade. The upgraded package must have the exact six-file inventory, load without missing or Synology libraries, preserve patched and unpatched tray fixtures, pass native integrity, and remove all owned paths.

On a real Ubuntu desktop, the exact v1 DEB must show an overlay, show a syncing-to-stable transition, work after Dolphin restarts, expose both user-invoked context actions, and remove cleanly. Synology-owned windows are not automated.

README, installation, and usage documents must agree on support and package commands. Private security reports must use GitHub private vulnerability reporting. The bug form must request enough compatibility and reproduction data while warning users to redact private paths, account names, and links.

## Plan of Work

Change the CMake project version and every current package identity owner to 1.0.0. Preserve older release notes and use 0.4.0 only as the upgrade baseline.

Extend `ci/package` with optional `--upgrade-from DIR`. Require the exact v0.4.0 DEB and RPM names and recorded SHA-256 digests. Mount the matching baseline package read-only into each runtime container. Extend `ci/validate-package-lifecycle` with an optional third baseline argument. In that mode, install the baseline, upgrade to the candidate, and then run the existing complete candidate checks. The normal no-argument package command retains its clean-install behavior. Extend `tests/test_package_command.py` for accepted arguments, missing or altered baselines, both upgrade mounts, native upgrade commands, and both terminal results.

Update user and technical documentation in Simple English. Add version 1.0 package download, install, upgrade, verify, and removal commands. State the support matrix and Fedora limitation consistently. Add release notes, the security policy, the bug form, and contribution instructions. Remove the retired legal-review sentence from the current technical report but preserve historical plan records.

Keep every file under `src/` unchanged. Do not change behavioral tests except the package-command owner.

## Concrete Steps

Run commands from `/home/mbeutler/Projects/dolphin-drive`.

Download both published v0.4.0 assets into one temporary directory with `gh release download v0.4.0`. Run:

    ./ci/package --upgrade-from /absolute/path/to/the/directory

Expect exactly these candidate paths after both clean and upgrade lifecycles pass:

    build/packages/ubuntu-26.04/synodrive-dolphin_1.0.0-1_amd64.deb
    build/packages/fedora-44/synodrive-dolphin-1.0.0-1.x86_64.rpm

Run focused host checks before review:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build --output-on-failure

Install the exact DEB on the Ubuntu desktop. Let the user invoke the two context actions. Record every required observation in `.agent/test-results/SDD-009.md`. Remove the package, verify all owned files are absent, then reinstall the candidate so the desktop remains usable.

Complete the read-only reviews and protected workflow in SDD-010. Reviewers run no commands. The coordinator runs `./ci/run` only after every other publication gate and current-head review is green.

After the final pull-request and `main` results pass, create annotated tag `v1.0.0` on that exact commit. Publish a normal GitHub release titled `Synology Drive Dolphin Extension (Unofficial) 1.0.0` with exactly the validated DEB and RPM.

## Validation and Acceptance

All five host CTest tests must pass. Both package formats must pass archive, clean lifecycle, upgrade lifecycle, integrity, loading, passive-tray, and removal checks. The exact v1 DEB must pass the real desktop sequence.

Reviewers must report no applicable finding on the exact candidate and existing evidence. The local, pull-request, and merged-main distribution gates must each pass in their required order. The tag, CMake version, reviewed tree, package identities, asset digests, and release state must agree.

## Idempotence and Recovery

Builds, package checks, and temporary downloads are repeatable. A missing or altered baseline package fails before Docker starts. A project-controlled CI failure returns the pull request to draft and invalidates old review and check evidence. An external GitHub failure stops publication without weakening protection.

Do not move or reuse an existing tag. If `v1.0.0` already exists, stop. If merged `main` differs from the reviewed tree, repeat the affected gates before tagging.

## Interfaces and Dependencies

The new package interface is `./ci/package --upgrade-from DIR`. The directory contains `synodrive-dolphin_0.4.0-1_amd64.deb` and `synodrive-dolphin-0.4.0-1.x86_64.rpm`. The command uses no network and accepts no other option.

The lifecycle validator accepts `validate-package-lifecycle DEB|RPM ARTIFACT [BASELINE]`. Without `BASELINE`, it performs the existing clean install. With it, it installs the baseline and then upgrades to `ARTIFACT` before the existing candidate checks.

No new runtime or third-party dependency is added. GitHub release downloads occur only in coordinator preflight.

This revision links the accepted release plan to its durable SDD-009 and SDD-010 implementation specifications.
