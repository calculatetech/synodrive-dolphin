# Add DEB and RPM release candidates

This ExecPlan is the durable specification for SDD-004. Maintain it according to `/home/mbeutler/.codex/PLANS.md` until review closure.

The task uses branch `task/SDD-004-packaging`, based on `main` commit `988b08cc006b91a5e962f9ca48a0e6d6dae9c516`. The isolated worktree is `/home/mbeutler/Projects/dolphin-drive-sdd004`.

## Purpose / Big Picture

After this change, a developer can build a native DEB or RPM with CPack. A maintainer can run `./ci/package` to produce the official Ubuntu 26.04 and Fedora 44 x86_64 release candidates.

The candidates use version `0.3.0`. SDD-004 does not publish them. SDD-005 must validate installation and removal before publication.

## Progress

- [x] (2026-09-01) Reconciled the accepted Plan records and moved SDD-004 to Active.
- [x] (2026-09-01) Created the isolated task branch and worktree.
- [x] (2026-09-01) Added CPack metadata, dependencies, and the license payload.
- [x] (2026-09-01) Added package tools, the official package command, and its orchestration check.
- [x] (2026-09-01) Updated user and maintenance documentation.
- [x] (2026-09-01) Built and inspected both package candidates.
- [ ] Complete trace closure, correctness review, and roadmap closure.

## Surprises & Discoveries

- Observation: `libnautilus-extension.so.4` is loaded with `dlopen`.
  Evidence: Native ELF dependency scanners cannot infer this runtime dependency. The DEB must add `libnautilus-extension4`. The RPM must add `libnautilus-extension.so.4()(64bit)`.
- Observation: Qt installs plugins below different roots on the target distributions.
  Evidence: Ubuntu reports `/usr/lib/x86_64-linux-gnu/qt6/plugins`. Fedora reports `/usr/lib64/qt6/plugins`.
- Observation: The current images do not contain the native package builders.
  Evidence: Ubuntu lacks `dpkg-shlibdeps`. Fedora lacks `rpmbuild`.
- Observation: CPack checks staged DEB binaries with the `file` command before it runs `dpkg-shlibdeps`.
  Evidence: The first real DEB build stopped with `CPackDeb: file utility is not available`.
- Observation: DEB dependency values contain commas and package names can contain regular-expression operators.
  Evidence: Exact dependency assertions failed when they treated `+` and comma separators as regular-expression syntax. Parsing names and comparing fixed strings corrected the check.
- Observation: CPack does not map `CPACK_PACKAGE_CONTACT` to the RPM `Packager` field.
  Evidence: The first complete identity trace reported `Packager: (none)`. A `Packager:` line in `CPACK_RPM_SPEC_MORE_DEFINE` supplies the selected identity.

## Decision Log

- Decision: Use CPack directly and keep one package per format.
  Rationale: CPack already owns CMake install staging and native dependency generation.
- Decision: Keep host-native CPack available and use containers only for official candidates.
  Rationale: Local builders need packages for their compatible host. Official assets need fixed targets.
- Decision: Use version `0.3.0` and release `1`.
  Rationale: Published versions `0.1.0` and `0.2.0` are consumed. Pre-1.0 feature work increments the minor version.
- Decision: Install the MIT license as `/usr/share/doc/synodrive-dolphin/copyright`.
  Rationale: Package users must receive the project license.
- Decision: Add explicit Dolphin and Nautilus runtime dependencies.
  Rationale: The plugin needs Dolphin, and the dynamic Nautilus load is invisible to native scanners.
- Decision: Assert the full container and validator structure in the orchestration check.
  Rationale: Exact normal artifacts alone cannot distinguish a partial inventory, an extra output, a disabled scanner, or a conflicting container boundary.
- Decision: Keep package inspection in one small executable used by real builds and adversarial fixtures.
  Rationale: The same production path must reject wrong identity, partial or extra payloads, and missing or forbidden dependencies.
- Decision: Do not publish during SDD-004.
  Rationale: The user moved publication behind the SDD-005 install and removal gate.

## Outcomes & Retrospective

Complete this section after validation and final review.

## Context and Orientation

`CMakeLists.txt` builds `synodrive-status` and `synodrive-overlay.so`. Its install rules place the command in `bin` and the module below the target Qt 6 plugin root.

`ci/run` builds and tests the project in pinned Ubuntu 26.04 and Fedora 44 images. It remains unchanged. The package command reuses those images after adding one native packaging tool to each Dockerfile.

`docs/roadmap.md` is the only lifecycle authority. SDD-004 remains its only Active task until final clean review.

The applicable immutable Plan records are:

- `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`
- `.agent/plan-history/plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- `.agent/plan-history/plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md`
- `.agent/plan-history/plan-summary.20260901T163218079291Z.30d64bb2faf269861f88faf3432e8a0268edc0e84e7653fe1d8a362799077f87.md`
- `.agent/plan-history/plan-summary.20260901T181723354265Z.e5524360a445bc95804a35781f50d2b095f673ff5d690f019e6d493a8279d1fe.md`
- `.agent/plan-history/plan-summary.20260901T182134563661Z.8250eabe2bf83218118a65973b2f84697e7f8200987d04bd15768e6ed0fbb3b0.md`
- `.agent/plan-history/plan-summary.20260901T210430122701Z.2649b14c4cd2877a93d28ca5e07cd6fce0e06766442bdb90f40ed697ab1b75d5.md`

The last record has a byte-identical sibling beside this specification.

## Product Boundary

The task composes CMake install rules, CPack, the two package images, `ci/package`, package inspection, and packaging documentation.

Production CLI and plugin behavior are opaque. `ci/run` is also opaque and must retain its existing interface and results.

SDD-005 owns package installation, removal, upgrade, permissions, and installed shared-library resolution. Hosted automation, signing, repositories, checksums, source packages, debug packages, ARM, package splitting, and publication are deferred.

The supported model is one developer on x86_64 Linux with Docker. A compatible native host has CPack and its format-specific native package tools. Docker image construction can use the network. Package build containers cannot.

## Prior Plan Reconciliation

The `20260901T045809` record is historical and superseded where it excluded packaging. Its process-isolation and no-redistribution rules remain carried.

The `20260901T141135` roadmap record is carried for CPack, x86_64 packages, generated dependencies, and no Synology package dependency.

The `20260901T153200` record is carried for local Docker CI and the post-1.0 hosted-workflow deferral.

The `20260901T163218`, `20260901T181723`, and `20260901T182134` records are carried for Drive compatibility, runtime isolation, and version history. SDD-004 does not change those behaviors.

Record: `.agent/plan-history/plan-summary.20260901T210430122701Z.2649b14c4cd2877a93d28ca5e07cd6fce0e06766442bdb90f40ed697ab1b75d5.md`

Status: carried, with one accepted roadmap supersession.

Scope: SDD-004 publication timing only.

Reason: The user requires SDD-005 package-lifecycle validation before publication.

Replacement: SDD-004 produces local `0.3.0` candidates. SDD-005 gates their publication.

## Scenario Proof

| Boundary | Required result and contrast | Runnable proof |
| --- | --- | --- |
| B1: official full set | Zero arguments produce exactly the named Ubuntu DEB and Fedora RPM. One argument exits 2 before Docker. A missing or extra leg cannot pass. | Run the orchestration check and real `./ci/package`. Inspect the complete artifact set. |
| B2: native entry points | Direct `cpack -G DEB` and `cpack -G RPM` work without wrapper-only state. | Invoke each generator directly in its target container. |
| B3: identity | Both packages use the exact name, version, release, product name, contact, homepage, architecture, and license fields. A correct DEB cannot hide stale RPM metadata. | Query every applicable DEB and RPM metadata field. |
| B4: payload | Each archive contains the command, target plugin, and copyright file once. No later archive record can add a test or Synology file. | Compare each complete regular-file inventory with the exact three-path target set. |
| B5: dependencies | Native scanning adds linked dependencies. Each format adds Dolphin and its exact Nautilus runtime. No dependency names Synology Drive. | Inspect the complete `Depends` and `Requires` collections and the configured auto-scan gates. |
| B6: isolation and failure | Both runs use read-only source, no network, and an ephemeral build. First-leg and later-leg failures stop once and return nonzero. | Use the recording Docker test for both failure positions and every run argument. |
| B7: opaque regressions | `ci/run` and caller-selected source install prefixes remain correct. | Run `./ci/run`, its argument check, CTest, and a temporary-prefix staged install. |

For stale-output cleanup, seed one older matching package and one unrelated sentinel in each output directory. A successful run removes only matching stale candidates and preserves each sentinel.

## Plan of Work

Update CMake with version `0.3.0`, project metadata, one license install rule, and native CPack settings. Keep the normal source-install prefix caller-controlled. Set only the CPack staging prefix to `/usr`.

Add `dpkg-dev` and its required `file` utility to the Ubuntu image. Add `rpm-build` to the Fedora image. Add no other container dependency.

Create executable `ci/package` with POSIX shell and `set -eu`. It accepts no arguments, verifies x86_64, builds Ubuntu before Fedora, and stops on failure. Each container uses read-only source, `--network none`, and temporary `/build`. It invokes one Release CPack generator and validates the resulting archive with native tools. It prints both paths only after both legs pass.

Keep the native inspection rules in executable `ci/validate-package`. The package command calls it for each real archive. The orchestration check calls the same path through fake native tools for normal and adversarial full-set results.

The fake Docker seam preserves the public `ci/package` sequence and executes its inner command with fake external tools. This composes identity, payload, and dependency counterexamples through the same owner chain without nested Docker.

Add one Python orchestration check. It copies the command and Dockerfiles to a temporary checkout and puts a recording Docker shim first on `PATH`. Cover argument rejection, fixed order, both failure positions, one attempt per failed leg, isolation arguments, and no false success output.

Update the smallest documents that own package generation, source installation, package metadata, release notes, and lifecycle state. Do not document package installation until SDD-005 validates it.

## Concrete Steps

Run commands from the task worktree.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build --output-on-failure
    ./ci/run
    ./ci/package
    git diff --check

Keep detailed output in `.agent/test-results/SDD-004.md`. Never stage that file.

## Validation and Acceptance

Acceptance requires:

- Host build and complete CTest pass.
- The package orchestration check passes.
- Ubuntu and Fedora `ci/run` legs pass without source changes.
- Direct native CPack commands pass in both target environments.
- `./ci/package` produces the exact two standard filenames.
- Complete metadata, payload, and dependency inspections pass for both candidates.
- Stale matching candidates are replaced and unrelated sentinels remain.
- A temporary-prefix source install keeps all three files below that prefix.
- No production C++ file, GitHub workflow, tag, release, or remote asset changes.
- Documentation is current and `git diff --check` passes.
- Boundary trace closure and native correctness review are clean.

## Idempotence and Recovery

The command replaces only matching `synodrive-dolphin` package candidates in the two exact ignored output directories. It preserves unrelated files and Docker images.

Each Docker build and run gets one attempt. A failure leaves any earlier candidate in place and returns nonzero. Re-running the command replaces stale matching candidates.

Do not remove user files, unrelated build output, worktrees, branches, tags, or remote state.

## Artifacts and Notes

Official candidates stay in `build/packages/ubuntu-26.04` and `build/packages/fedora-44`. Docker images remain normal local cache artifacts.

Detailed validation and review evidence stays in ignored `.agent/test-results/SDD-004.md`.

## Interfaces and Dependencies

New public command:

    ./ci/package

It accepts no arguments. Unsupported input prints usage and exits 2. Build, validation, or container errors return nonzero.

Native package entry points:

    cpack --config build/CPackConfig.cmake -G DEB -B build/packages/host
    cpack --config build/CPackConfig.cmake -G RPM -B build/packages/host

The project adds no production library dependency. Package metadata explicitly requires Dolphin and the existing Nautilus extension runtime. Developers need Docker only for official candidates.

## Plan Deviations

The roadmap originally assigned publication to SDD-004. The user moved publication behind SDD-005. This task creates local release candidates only.

The accepted plan named `dpkg-dev` as the Ubuntu package tool. The first real build proved that CPack also calls the separately packaged `file` utility. The Ubuntu image includes it as a required direct dependency of the accepted shared-library scan.
