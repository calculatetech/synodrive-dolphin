<proposed_plan>
# SDD-005 Package Lifecycle Validation and Combined Push

## Summary

Complete SDD-005 on top of the local SDD-004 packaging commit. `./ci/package` will validate both archives and their real install/remove lifecycles in clean Ubuntu 26.04 and Fedora 44 containers.

Keep version `0.3.0` unreleased. After validation and review, create a separate SDD-005 commit and push both commits together to `task/SDD-004-packaging`. Do not create a PR, merge, tag, release, upload assets, or enable hosted CI.

Preflight is ready. Its three findings are incorporated below: clean-start counterexamples, failed native integrity verification, and a retained documentation directory.

## Implementation Changes

- Convert each packaging Dockerfile to named `common`, `build`, and `runtime` stages. Keep the build stage first, and make `ci/run` and `ci/package` explicitly select `--target build`. This uses Docker’s native [multi-stage and target support](https://docs.docker.com/build/building/multi-stage/).

- Build minimal runtime targets:
  - Ubuntu: `dolphin` and `libnautilus-extension4`.
  - Fedora: `dolphin` and `nautilus-extensions`, with weak dependencies and the unrelated `fedora-cisco-openh264` repository disabled.
  - Permit networking only while building images. Run packaging and lifecycle containers with `--network none`, read-only source/artifact mounts, and ephemeral build state.

- Add internal `ci/validate-package-lifecycle FORMAT ARTIFACT`, guarded by `SYNODRIVE_PACKAGE_CONTAINER=1`. Use native [dpkg install/query/purge operations](https://manpages.debian.org/testing/dpkg/dpkg.1.en.html) and [RPM install/query/verify/erase operations](https://rpm.org/docs/6.0.x/man/rpm.8). Add only a test-root environment seam for the existing fake-Docker test; production orchestration must never set it.

- For each package, require:
  - A clean start: package absent, all project installation paths absent, and `/opt/Synology/SynologyDrive` absent.
  - Exact installed package metadata and exactly three owned non-directory entries: command, plugin, and copyright file. Reject symlinks, duplicates, omissions, and extras.
  - ELF command/plugin files, a copyright file byte-identical to `LICENSE`, and `root:root` ownership.
  - DEB modes: command `0755`, plugin `0644`, copyright `0644`.
  - RPM modes: command `0755`, plugin `0755`, copyright `0644`.
  - No-argument command behavior: exit `2`, empty stdout, and exact stderr `usage: synodrive-status <absolute-path>\n`.
  - Successful `ldd -r` for both binaries, including `libnautilus-extension.so.4` resolution and no missing libraries or undefined symbols. Use the standard [`ldd -r` behavior](https://man7.org/linux/man-pages/man1/ldd.1.html).
  - Successful `dpkg --verify` or `rpm --verify`.
  - After purge/erase: package database absent, every captured path absent, package documentation directory absent, and Synology paths still absent.

- Preserve the public `./ci/package` no-argument interface and final two-path output. Its success meaning changes: it prints those paths only after archive validation and both lifecycle validations pass. Stop after the first failure and never retry.

## Tests and Documentation

- Extend `tests/test_package_command.py`; add no framework or separate test target. Preserve all SDD-004 archive tests and assert the exact eight-call order: Ubuntu build/run/runtime-build/runtime-run, then Fedora equivalents.

- Run the real orchestration and both real validators through fake external seams. Cover:
  - Both successful formats and exact Docker arguments, targets, mounts, network policy, ordering, and output.
  - Package already installed and each pre-existing project path, including the later copyright path; fail before native installation.
  - Every applicable metadata, inventory, owner, mode, type, license, CLI, loader, Nautilus, and Synology violation.
  - Native integrity-verification failure for both formats.
  - Retained package database, each retained owned file, and a documentation directory retained by itself.
  - Ubuntu failure stopping before Fedora and Fedora failure suppressing final success output.
  - Duplicate-prefix inventories followed by a later extra entry, proving complete-set validation rather than prefix validation.

- Validate with shell syntax checks, the Python command test, host configure/build/CTest, `./ci/run` for both distributions, a real `./ci/package`, artifact/checksum inspection, `git diff --check`, trace closure, and a full native Codex review against parent `f42d323`.

- Before implementation, mark SDD-005 Active and create `docs/plans/SDD-005-package-lifecycle.md`. Update:
  - README: packaging now includes real lifecycle validation.
  - INSTALL: local `apt`/`dnf` artifact installation and package removal, distinct from source installation.
  - Release notes: add lifecycle validation, remove the SDD-005 blocker, and retain unreleased `0.3.0`.
  - Technical report: record clean-runtime lifecycle evidence.
  - Roadmap: mark SDD-005 Completed only after review passes.
  - Keep detailed evidence in ignored `.agent/test-results/SDD-005.md`.

## Delivery and Assumptions

- Preserve the user-owned `.vscode/` directory.
- Commit SDD-005 separately atop `f42d323`, using `test: validate package lifecycle`.
- Confirm `.github/workflows` is absent, freeze the reviewed head, then run one non-force push: `git push -u origin task/SDD-004-packaging`.
- Verify the remote branch matches the frozen local head and contains both SDD-004 and SDD-005 commits.
- Upgrade lifecycle testing remains deferred until a packaged release exists. Hosted CI and branch protection remain post-1.0. Signing, ARM, context menus, PR/main integration, tags, releases, and uploads remain out of scope.
- All prior plan records are reconciled. SDD-005 now closes SDD-004’s package-lifecycle deferral without changing production C++, Drive integration, or `ci/run` behavior.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-002-local-ci.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/plans/SDD-011-idle-performance.md -->
<!-- cpk-plan-spec: docs/plans/SDD-004-packaging.md -->
<!-- cpk-plan-spec: docs/plans/SDD-005-package-lifecycle.md -->
</proposed_plan>