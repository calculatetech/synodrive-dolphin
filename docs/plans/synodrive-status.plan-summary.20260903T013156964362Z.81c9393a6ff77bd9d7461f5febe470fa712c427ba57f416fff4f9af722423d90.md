<proposed_plan>
# SDD-015 — Opt-in tray left-click patch for 0.4.0

## Summary

Add a reversible, user-controlled patch that makes a normal tray-icon left click call `SysTray::showStyledMenu()`. Fold the unreleased 0.3.0 work into an unreleased 0.4.0 candidate.

Implement on `task/SDD-015-tray-patch` from synchronized `main`. Make SDD-015 the sole active roadmap task and create its ExecPlan before source changes. Do not push, tag, or release until field testing succeeds and publication is requested.

## Public interfaces

- Install `/usr/bin/synodrive-tray-patch` with:
  - `status`: print `patched\n` or `unpatched\n`.
  - `apply`: explicitly opt in; patch once or succeed unchanged if already patched.
  - `restore`: reverse the patch or succeed unchanged if already unpatched.
- Successful commands exit 0. Invalid syntax prints `usage: synodrive-tray-patch status|apply|restore` to stderr and exits 2. Unsupported targets and operational failures use the `synodrive-tray-patch:` prefix and exit 1.
- After an actual mutation, print one stderr advisory:
  - Apply: `synodrive-tray-patch: restart Synology Drive Client to load the tray patch`
  - Restore: `synodrive-tray-patch: restart Synology Drive Client to remove the tray patch`
- Add `SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL`, a CMake option that defaults to `OFF`.
  - When enabled, capture the configuring user’s absolute `HOME`.
  - At the end of source installation, run the same production `apply` path against that captured home, including when installation uses `sudo`.
  - Reject unset, empty, relative, or staged `DESTDIR` use before patching.
- DEB and RPM installation remains passive. Package managers get no patch flag, prompt, trigger, or maintainer script; users run `synodrive-tray-patch apply` afterward.

## Implementation changes

- Reuse the existing internal-major-4 compatibility parser. Resolve only:
  - `$HOME/.SynologyDrive/SynologyDrive.app/INFO`
  - `$HOME/.SynologyDrive/SynologyDrive.app/bin/cloud-drive-ui`
- Accept only a regular, non-symlink, little-endian x86-64 ELF `ET_DYN` file with the unique expected symbols and recognized 96-byte `iconActivated()` template.
- Apply the proven seven-byte transformation:
  - Route the Trigger terminal block to `leave; jmp rel32 SysTray::showStyledMenu()`.
  - Move MiddleClick to the retained original epilogue.
  - Verify every other instruction byte, symbol location, section bound, and relative-jump range.
- Restore the exact inverse seven bytes. Do not create backups, persistent state, automatic reapply logic, watchers, or process-control code.
- Write through a same-directory exclusive temporary file. Preserve ownership, group, and mode; sync the file; atomically rename it; then sync the directory.
  - Before rename, every failure leaves the target unchanged.
  - Rename is the commit point.
  - If the final directory sync fails, exit 1 and report that the target changed but durability was not confirmed; direct the user to run `status`.
- Record the official major-4 compatibility evidence in the reverse-engineering report. Do not commit or package Synology binaries.
- Change all source, package, filename, validation, documentation, and release-note identity from unreleased 0.3.0 to unreleased 0.4.0. Both packages gain the patch command as the sixth project-owned file.
- Update README, installation, usage, release notes, roadmap, and research documentation. Explain opt-in risk, restart requirements, source-install behavior, update replacement, manual reapplication, and restore-before-uninstall.

## Test and delivery gates

- Exercise the actual release executable with temporary `HOME` directories and generated ELF fixtures. Permit a test-only seam only for deterministic write and post-commit synchronization failures.
- Cover exact output and exit contracts, supported and unsupported INFO data, ELF validation, symbol/template corruption, partial patches, seven-byte-only mutation, exact restoration, idempotence, symlinks, preservation of metadata, and unchanged hashes for all pre-commit failures.
- Test source installation with:
  - Default option against both patched and unpatched seeds.
  - Enabled option with configure-time `HOME=A` and install-time `HOME=B`; only A changes.
  - Invalid configure-time homes and `DESTDIR`; neither home changes.
- Update DEB/RPM validators for 0.4.0 and the exact six-file inventory. Test install and removal against paired patched and unpatched user targets. Inspect package metadata to prove the absence of Debian maintainer files and RPM scripts or triggers.
- Run focused CTest, the complete host build and CTest suite, `./ci/run`, `./ci/package`, `git diff --check`, boundary-trace closure, documentation maintenance, and correctness review.
- Make field testing an SDD-015 completion gate:
  1. The user explicitly applies the patch and restarts Drive.
  2. Static verification confirms Trigger targets the exact `showStyledMenu()` symbol.
  3. One left click visibly opens the styled menu.
  4. Restore produces the original bytes; after restart, the added dispatch is absent.
- Keep SDD-015 active if field testing is pending. After successful field testing, record the evidence, complete the roadmap task, run the final delta review, and commit. Do not push, tag, or release.

## Assumptions and exclusions

- Supported scope remains x86-64 Linux, KDE 6, Synology Drive 8.x/internal major 4, and exact recognized binary layouts.
- Invocation of `apply` or enabling the source-install option is sufficient consent; no second confirmation prompt is added.
- A Synology update can remove the patch. The user must inspect and reapply it manually.
- Wayland popup placement, GUI management, arbitrary target paths, automatic restart, updater migration, uninstall restoration, ARM, hosted CI, publication, and changes to existing overlay or context-menu behavior are excluded.

## Prior plan reconciliation

- Record: `.agent/plan-history/plan-summary.20260902T053027990715Z.455303d1f1a965144aa3e84aac88b9517fc5d07047146d702e4ed915255e5f87.md`
- Status: superseded
- Scope: unreleased 0.3.0 release identity only
- Reason: the user selected 0.4.0 for SDD-015, and 0.3.0 was never tagged.
- Replacement: fold the unreleased packaging and context-action work into 0.4.0 with the tray patch.
- The byte-identical later duplicate receives the same resolution. All other applicable plan-history decisions are carried, including local-only CI, major-4 compatibility, helper isolation, package lifecycle validation, and no Synology redistribution.

<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-002-local-ci.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/plans/SDD-004-packaging.md -->
<!-- cpk-plan-spec: docs/plans/SDD-005-package-lifecycle.md -->
<!-- cpk-plan-spec: docs/plans/SDD-008-context-menu-actions.md -->
<!-- cpk-plan-spec: docs/plans/SDD-015-tray-patch.md -->
</proposed_plan>