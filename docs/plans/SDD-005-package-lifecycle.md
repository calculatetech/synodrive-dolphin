# Validate the DEB and RPM package lifecycle

This ExecPlan is the durable specification for SDD-005. Maintain it according to `/home/mbeutler/.codex/PLANS.md` until review closure.

The task uses branch `task/SDD-004-packaging`. It starts at commit `f42d323c176e7ec833b7e82159ac7b8db8663a1b`, which contains the completed SDD-004 package candidates. The isolated worktree is `/home/mbeutler/Projects/dolphin-drive-sdd004`.

## Purpose / Big Picture

After this change, `./ci/package` proves that both release candidates work as native packages. The command installs, inspects, runs, verifies, and removes each package in a clean disposable container.

The command keeps its no-argument interface. It prints the two package paths only after the Ubuntu and Fedora lifecycle checks pass.

## Surprises & Discoveries

- Observation: The SDD-004 build images do not contain the full runtime dependency set.
  Evidence: A DEB install in the Ubuntu build image failed on missing KDE runtime packages. An RPM install without dependencies did not provide a valid lifecycle check.
- Observation: The Fedora Cisco OpenH264 repository is not required for Dolphin.
  Evidence: The repository failed during a runtime-image probe. Fedora installed Dolphin from the normal Fedora repositories when the runtime image disabled only `fedora-cisco-openh264`.
- Observation: Package-native permissions differ for the plugin.
  Evidence: The DEB stores the plugin as `0644`. The RPM stores it as `0755`. The user accepted both native results.

## Decision Log

- Decision: Extend `./ci/package` instead of adding a second public command.
  Rationale: One command already owns official package generation and archive checks.
- Decision: Add named `build` and `runtime` Docker targets.
  Rationale: The build tools and runtime dependencies have different purposes. Separate targets keep `ci/run` unchanged.
- Decision: Use native package tools for installation, inventory, verification, and removal.
  Rationale: These tools define the supported package lifecycle on each target distribution.
- Decision: Add one internal lifecycle validator and reuse the existing package-command test.
  Rationale: One real validator must own both the real container checks and the adversarial fixtures.
- Decision: Require exact native permissions.
  Rationale: The accepted DEB modes are `0755`, `0644`, and `0644`. The accepted RPM modes are `0755`, `0755`, and `0644`.
- Decision: Keep upgrade checks deferred.
  Rationale: No packaged release exists as a valid upgrade source.
- Decision: Push the SDD-004 and SDD-005 commits together on the task branch.
  Rationale: The user wants one validated branch push before any pull request or release work.

## Outcomes & Retrospective

After completion, record only the observable package-lifecycle result and design lessons here.

## Context and Orientation

`ci/package` builds the Ubuntu DEB first and the Fedora RPM second. It calls `ci/validate-package` inside each build container to check the archive name, metadata, payload, and dependencies.

`ci/docker/ubuntu-26.04.Dockerfile` and `ci/docker/fedora-44.Dockerfile` define the distribution environments. `ci/run` uses them for the existing build and test gate.

`tests/test_package_command.py` runs the real `ci/package` script through a fake Docker executable. The fake external commands let the test exercise wrong package results without nested containers.

The package installs these files:

- `/usr/bin/synodrive-status`
- The target distribution's `synodrive-overlay.so` Qt plugin path
- `/usr/share/doc/synodrive-dolphin/copyright`

The version remains unreleased `0.3.0`. The exact candidates are `synodrive-dolphin_0.3.0-1_amd64.deb` and `synodrive-dolphin-0.3.0-1.x86_64.rpm`.

The applicable immutable Plan records include:

- `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`
- `.agent/plan-history/plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- `.agent/plan-history/plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md`
- `.agent/plan-history/plan-summary.20260901T163218079291Z.30d64bb2faf269861f88faf3432e8a0268edc0e84e7653fe1d8a362799077f87.md`
- `.agent/plan-history/plan-summary.20260901T181723354265Z.e5524360a445bc95804a35781f50d2b095f673ff5d690f019e6d493a8279d1fe.md`
- `.agent/plan-history/plan-summary.20260901T182134563661Z.8250eabe2bf83218118a65973b2f84697e7f8200987d04bd15768e6ed0fbb3b0.md`
- `.agent/plan-history/plan-summary.20260901T210430122701Z.2649b14c4cd2877a93d28ca5e07cd6fce0e06766442bdb90f40ed697ab1b75d5.md`
- `.agent/plan-history/plan-summary.20260901T212049866952Z.2649b14c4cd2877a93d28ca5e07cd6fce0e06766442bdb90f40ed697ab1b75d5.md`
- `.agent/plan-history/plan-summary.20260902T032236776229Z.03aa9cd44a8385c3489770bbc8821e3531d52f2d35c9832b603f8ec58ab0b49b.md`

The last record has a byte-identical sibling beside this specification.

## Product Boundary

The applicable product-boundary rule is `/home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md`.

This task composes the package command, archive validator, lifecycle validator, Docker build/runtime images, package-command test, and package documentation. These owners must produce one terminal success or stop on the first wrong result.

Production C++, Synology Drive behavior, live Dolphin behavior, and `ci/run` results are opaque. The task can select an explicit Docker build target, but it cannot change the `ci/run` public interface or accepted result.

Upgrade checks, hosted CI, branch protection, signing, package repositories, ARM, context menus, pull requests, main integration, tags, releases, and asset uploads are deferred.

The supported model is one developer on x86_64 Linux with Docker. The official targets are Ubuntu 26.04 and Fedora 44. Docker image builds can use the network. Package-build and lifecycle containers cannot use the network. No step can download, mount, install, package, or depend on Synology software.

## Prior Plan Reconciliation

All earlier records remain carried for process isolation, package identity, local CI, Drive 8.x compatibility, performance, and version history.

The SDD-004 package plan deferred installation and removal to SDD-005. This task implements that deferred lifecycle. The first upgrade check remains deferred until a packaged release exists.

The user accepted the exact current native permission tuples. No retained Plan record conflicts with this decision.

## Scenario Proof

The applicable proof rules are:

- `/home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md`
- `/home/mbeutler/.agents/skills/design-preflight/references/owner-composition.md`
- `/home/mbeutler/.agents/skills/design-preflight/references/full-set-results.md`

| Boundary | Required result and contrast | Runnable proof |
| --- | --- | --- |
| B1: complete targets | The command validates exactly the Ubuntu DEB and Fedora RPM. A missing or extra lifecycle leg cannot pass. | Assert the exact eight Docker calls and inspect both real artifacts. |
| B2: isolation | Image builds can use the network. Package and lifecycle runs use no network, read-only source and artifacts, and ephemeral build state. | Assert every Docker argument and run the real command. |
| B3: Synology exclusion | No image, package, dependency, mount, or installed path contains Synology software. | Check clean start, archive metadata, installed state, loader results, and post-removal state. |
| B4: terminal behavior | Each leg gets one attempt. The first or later failure stops without final package paths. | Inject Ubuntu and Fedora lifecycle failures through the real helper. |
| B5: installed file set | The complete non-directory inventory contains exactly the command, plugin, and copyright file. | Cover empty, partial, duplicate, duplicate-prefix-plus-extra, symlink, and extra inventories. |
| B6: installed metadata | Every applicable installed database field equals the accepted package identity. | Change each field independently for both native formats. |
| B7: ownership and modes | All files use `root:root`. DEB modes are `0755/0644/0644`; RPM modes are `0755/0755/0644`. | Change each owner, group, and mode outcome independently. |
| B8: command launch | No arguments return 2, write no stdout, and write the exact usage line to stderr. | Change the exit status and both output streams independently. |
| B9: runtime loading | `ldd -r` resolves both binaries and the Nautilus extension runtime. No Synology library resolves. | Fail the command, later plugin, Nautilus lookup, missing-library, and undefined-symbol cases. |
| B10: native integrity | `dpkg --verify` and `rpm --verify` return success. | Return a deterministic integrity mismatch for each format and require immediate failure. |
| B11: clean start and removal | Install starts with no package or project paths. Removal clears the package database, every captured path, and its documentation directory. | Seed the installed package and each later project path before install. Retain each file, the database, and only the empty documentation directory after removal. |
| B12: inherited archive gate | Existing archive names, metadata, dependencies, payload, and stale-output behavior remain required before lifecycle validation. | Preserve all SDD-004 package-command cases. |

The input collection is the exact two target formats. The installed collection is the exact three non-directory paths. Metadata checks cover every applicable field. Loader checks cover both binaries. Removal checks cover every captured path.

## Plan of Work

Change each Dockerfile to define `common`, `build`, and `runtime` stages. Keep the existing build dependencies in `build`. Install only `dolphin` and `libnautilus-extension4` in the Ubuntu runtime stage. Install `dolphin` and `nautilus-extensions` in the Fedora runtime stage. Disable weak dependencies and `fedora-cisco-openh264` only for that Fedora runtime installation.

Make `ci/run` and the package build legs select `--target build`. After each archive check, make `ci/package` build the matching runtime target and start one disposable lifecycle container. Use `--rm`, `--init`, `--network none`, a read-only source mount, and a read-only artifact-directory mount. Do not mount the build tree.

Create executable `ci/validate-package-lifecycle`. It accepts only `DEB ARTIFACT` or `RPM ARTIFACT`. It must reject execution unless `SYNODRIVE_PACKAGE_CONTAINER=1`.

Before install, query the native package database and check every project path. Then install with `dpkg --install` or `rpm --install`. Query every accepted metadata field. Get the complete package-owned path list through `dpkg-query -L` or `rpm -ql`, remove directory entries, and compare the ordered result as a set with the exact three target files.

Use native commands to check file type, bytes, ownership, and modes. Run the installed command and compare its exact process result. Run `ldd -r` on the command and plugin. Prove that `libnautilus-extension.so.4` resolves and that no Synology library or path appears. Run the native package integrity command.

Capture the non-directory path set before removal. Purge the DEB or erase the RPM. Then check the package database, every captured path, the package documentation directory, and the Synology path.

Add a test-only `SYNODRIVE_LIFECYCLE_TEST_ROOT` seam. The helper uses this prefix only when the fake Docker test sets it. The production package command never sets it.

Extend `tests/test_package_command.py`. Keep its current fake-Docker design and archive cases. The fake Docker executable must run the real lifecycle helper. Fake only Docker, CMake, CPack, package-manager, loader, file-stat, and executable process boundaries. Parameterize distinct wrong terminal outcomes. Do not construct a Cartesian product.

Update `README.md`, `INSTALL.md`, `RELEASE_NOTES.md`, and `docs/SYNODRIVE_DOLPHIN_RECON.md`. Document that `./ci/package` includes the lifecycle gate. Add local `apt` and `dnf` installation and removal commands. Keep `0.3.0` unreleased and remove only the SDD-005 blocker statement.

## Concrete Steps

Run all commands from `/home/mbeutler/Projects/dolphin-drive-sdd004`.

    bash -n ci/run ci/package ci/validate-package ci/validate-package-lifecycle
    python3 tests/test_package_command.py
    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build --output-on-failure
    ./ci/run
    ./ci/package
    git diff --check

Store detailed output in `.agent/test-results/SDD-005.md`. Never stage that file.

## Validation and Acceptance

Acceptance requires all fake-Docker success and discrimination cases to pass. Host CTest must report all existing tests as passed. Both `ci/run` distribution legs must pass without a public behavior change.

The real `./ci/package` command must produce only:

    build/packages/ubuntu-26.04/synodrive-dolphin_0.3.0-1_amd64.deb
    build/packages/fedora-44/synodrive-dolphin-0.3.0-1.x86_64.rpm

Each clean runtime container must prove install, exact installed state, command launch, complete shared-library resolution, native integrity, and complete removal. A lifecycle failure must return nonzero and suppress the final path list.

The complete candidate must receive a clean native Codex review with task base `f42d323c176e7ec833b7e82159ac7b8db8663a1b`. Review closure can then move SDD-005 from Active to Completed.

After closure, create one SDD-005 commit on top of the existing SDD-004 commit. Confirm that `.github/workflows` is absent. Push `task/SDD-004-packaging` once without force. The remote branch must equal the frozen local head and contain both commits.

## Idempotence and Recovery

The package command can run repeatedly. Each lifecycle uses a new disposable container. A failed run stops and leaves local artifacts for diagnosis.

Do not retry image builds, package builds, installation, verification, removal, or publication automatically. Do not remove user files, `.vscode`, unrelated build output, Docker images, worktrees, branches, tags, or remote state.

## Artifacts and Notes

The package candidates stay in the existing ignored distribution directories. Docker runtime images remain normal local cache artifacts.

Detailed validation and review evidence stays in ignored `.agent/test-results/SDD-005.md`.

## Interfaces and Dependencies

The public command remains:

    ./ci/package

It accepts no arguments. Unsupported input prints usage and exits 2. Success now means that archive checks and both package lifecycles passed.

The internal helper interface is:

    ci/validate-package-lifecycle DEB ARTIFACT
    ci/validate-package-lifecycle RPM ARTIFACT

The helper requires `SYNODRIVE_PACKAGE_CONTAINER=1`. The test-root seam is internal and unsupported.

The project adds no production library or test dependency. The runtime images use only distribution packages that the installed command and plugin require.

## Plan Deviations

No deviation is accepted at implementation start. If a design-shaping result changes these instructions, update this specification before the related code edit.
