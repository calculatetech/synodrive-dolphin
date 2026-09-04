# Build version 1.0.1 packages from the oldest viable baseline

This ExecPlan is a living specification until review closure. Maintain it in accordance with `/home/mbeutler/.codex/PLANS.md`.

## Purpose / Big Picture

Version 1.0.1 must install on Debian 13 without requesting Qt 6.10.2. The DEB build moves from Ubuntu 26.04 to Debian 13, whose Qt 6.8.2 and KDE Frameworks 6 packages are old enough to produce compatible generated dependencies. Fedora 44 remains the RPM target. Existing Ubuntu 26.04 users must be able to upgrade the published version 1.0.0 DEB to the Debian-built version 1.0.1 DEB.

The observable result is a Debian-built `synodrive-dolphin_1.0.1-1_amd64.deb` whose dependency metadata accepts Debian 13, plus a Fedora-built `synodrive-dolphin-1.0.1-1.x86_64.rpm`. Both formats pass their complete archive and lifecycle checks and are published in the version 1.0.1 GitHub release.

## Surprises & Discoveries

- Observation: The published version 1.0.0 DEB requires `libqt6core6t64 (>= 6.10.2)` because it was built natively on Ubuntu 26.04.
  Evidence: `dpkg-deb -f synodrive-dolphin_1.0.0-1_amd64.deb Depends` reports that floor, while Debian 13 provides Qt 6.8.2.
- Observation: Supported Ubuntu LTS releases are not viable KF6 build baselines.
  Evidence: Ubuntu package search lists `libkf6kio-dev` only in Ubuntu 25.10 and newer, not in supported Ubuntu 22.04 or 24.04 releases.
- Observation: The unchanged source builds and packages on Debian 13 with a generated Qt Core floor of 6.8.2.
  Evidence: A clean Debian 13 container produced a DEB whose generated dependencies include `libqt6core6t64 (>= 6.8.2)`.

## Decision Log

- Decision: Use Debian 13 as the DEB build, clean-lifecycle, and first local-CI target.
  Rationale: It is the oldest viable stable distribution found with the required Qt 6 and KDE Frameworks 6 development packages, and it produces dependencies Debian 13 can satisfy.
- Decision: Replace the Ubuntu build Dockerfile with a Debian build Dockerfile, but retain a small Ubuntu 26.04 runtime Dockerfile.
  Rationale: Candidate packages must never inherit Ubuntu 26.04 build dependencies. The runtime-only image proves an existing Ubuntu user can upgrade from version 1.0.0.
- Decision: Prepare project version 1.0.1 with package release 1 for both formats.
  Rationale: This is a backward-compatible packaging correction after published version 1.0.0.
- Decision: Use the published version 1.0.0 packages as the upgrade baselines.
  Rationale: They are the packages users currently have. Their exact SHA-256 values are `b080ca138ffbfb5383beae9008e3aadb2f0ee6c56e2b3b46612bdf951d75fe0a` for the DEB and `b804cb8a29f9d0ddf5acdc442db3d07f3d302922456c7cb3ac7a336602193f3e` for the RPM.
- Decision: Keep the `./ci/package [--upgrade-from DIR]` command interface unchanged.
  Rationale: The existing interface already expresses clean and upgrade validation. Only its version, baseline, and distribution ownership must change.
- Decision: Keep all production C++ source unchanged.
  Rationale: The defect is generated package metadata from the build environment, not runtime behavior.
- Decision: Rebuild both package formats under version 1.0.1 even though the reported defect affects the DEB.
  Rationale: One release identity must describe one complete DEB and RPM candidate set.
- Decision: Run the real `./ci/run` only after source review is clean, at the unchanged reviewed tree.
  Rationale: Repository policy reserves CI for the final publication gate. Recording tests prove orchestration before review but cannot prove real Debian image construction and compilation.
- Decision: Publish version 1.0.1 after the protected pull request and post-merge CI gates pass.
  Rationale: The user explicitly requested publication and release after the package candidate passed its local compatibility gates. This supersedes this plan's earlier preparation-only publication boundary.

## Outcomes & Retrospective

Version 1.0.1 provides a Debian 13-compatible DEB without changing the version 1.0 runtime payload. Building on the oldest supported distribution that supplies the required development stack prevents generated dependencies from inheriting a newer distribution's library floor.

## Context and Orientation

`CMakeLists.txt` owns the project version and CPack package metadata. CPack is CMake's native package generator. `ci/debian.Dockerfile` will own the Debian build and clean runtime image. `ci/ubuntu.Dockerfile` will own only the Ubuntu upgrade runtime image. `ci/fedora.Dockerfile` remains the Fedora build and runtime image.

`ci/run` builds both distribution test images and runs the complete test suite. `ci/package` builds both package formats, validates each archive, and starts clean or upgrade lifecycle containers. `ci/validate-package` checks archive metadata and payload. `ci/validate-package-lifecycle` installs, checks, upgrades when requested, and removes the package. `tests/test_package_command.py` replaces Docker and package tools with recording fakes so orchestration and failures can be checked without containers.

The README, `docs/INSTALL.md`, `docs/USAGE.md`, `docs/RELEASE_NOTES.md`, `docs/SYNODRIVE_DOLPHIN_RECON.md`, `.github/ISSUE_TEMPLATE/bug_report.yml`, and `docs/roadmap.md` own current user, support, release, diagnostic, and task facts.

This plan supersedes the Ubuntu 26.04 build-target decisions in `.agent/plan-history/plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md`, both identical SDD-004 records with content hash `2649b14c4cd2877a93d28ca5e07cd6fce0e06766442bdb90f40ed697ab1b75d5`, both identical SDD-005 records with content hash `03aa9cd44a8385c3489770bbc8821e3531d52f2d35c9832b603f8ec58ab0b49b`, `.agent/plan-history/plan-summary.20260904T030553255401Z.890479e1a919a22ebb44fbae67c784f36e2c4699a5489d33f0b470aabb1f0208.md`, and the corresponding checked-in SDD-002, SDD-004, SDD-005, SDD-009, and SDD-010 ExecPlans. Their runtime, x86_64, package-content, isolation, Fedora 44, no-Synology-payload, and CI-order contracts remain in force.

## Product Boundary

The product boundary follows [Scope boundaries](</home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md>) and [Supported model](</home/mbeutler/.agents/skills/design-preflight/references/supported-model.md>).

The authoritative product source is the current version 1.0 runtime plus the user's Debian 13 installation failure. Package identity, build containers, archive and lifecycle validation, package orchestration tests, local CI orchestration, and current documentation compose with this task. Production files under `src/`, protected GitHub workflow behavior, Synology software, and Fedora implementation details other than the shared version and baseline are opaque.

The task excludes dependency rewriting, compatibility shims, Ubuntu candidate builds, new package formats, additional architectures, live Synology automation, signing, hosted repositories, and automated release publication. The user-authorized delivery includes the protected pull request, tag `v1.0.1`, GitHub release, and the two validated package assets.

## Scenario Proof

The scenarios follow [Scenario discrimination](</home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md>) and [Full-set results](</home/mbeutler/.agents/skills/design-preflight/references/full-set-results.md>).

The Debian build scenario configures, compiles, and generates the DEB inside Debian 13. The resulting dependency field must contain `libqt6core6t64 (>= 6.8.2)` or a lower satisfiable floor and must not contain the published package's `>= 6.10.2` floor.

The clean lifecycle scenarios install, validate, and remove version 1.0.1 on Debian 13 and Fedora 44. Each must prove the exact six-file payload, required distribution dependencies, no bundled or linked Synology library, passive package scripts, native integrity, and no owned files after removal.

The archive scenarios cover both formats completely. The DEB must be version `1.0.1-1` for `amd64`. The RPM must be version `1.0.1`, release `1`, for `x86_64`. Both must pass every existing metadata, dependency, payload, permission, scriptlet, trigger, and forbidden-content contrast in `ci/validate-package`.

The direct native-generation scenarios run `cpack -G DEB` in Debian 13 and `cpack -G RPM` in Fedora 44, then pass each result to the real archive validator. The wrapper cannot be the only proof for documented native CPack entry points.

The upgrade scenarios first verify the exact names and SHA-256 values of both published version 1.0.0 packages before any Docker call. Ubuntu 26.04 installs the published DEB and upgrades it to the Debian-built candidate. Fedora 44 installs the published RPM and upgrades it to the Fedora-built candidate. Each upgraded system must pass all candidate checks and clean removal.

The package-command tests target Debian clean lifecycle, Ubuntu DEB upgrade, Fedora clean lifecycle, and Fedora RPM upgrade failures separately. A failure stops all later work and suppresses both final candidate paths. Missing or altered DEB or RPM baselines fail before Docker starts. The no-argument command still builds and validates both clean candidates.

The local-CI orchestration test requires Debian 13 first and Fedora 44 second, with the existing fake Synology helpers and no new arguments. That recording proof does not attest a real distribution result. The candidate is incomplete until the real `./ci/run` passes after final review at the unchanged reviewed tree. Any source change after review invalidates review and requires the ordered gates again.

Current documentation must say Debian 13 is the primary DEB and clean-lifecycle target, Ubuntu 26.04 is an upgrade-compatibility target, and Fedora 44 remains the RPM target. It must link to the version 1.0.1 release and retain version 1.0.0 as release history and the upgrade baseline. Check the README, installation guide, usage guide, release notes, issue form, roadmap, technical report, and this plan.

All build, test, and package containers must run with networking disabled and the repository mounted read-only. The existing fake helpers and tray fixtures remain the only Synology substitutes. No command downloads or installs Synology software.

## Plan of Work

Change the project version in `CMakeLists.txt` to 1.0.1. Create `ci/debian.Dockerfile` from the existing package-capable image structure with `debian:13` and Debian's required package names. Reduce `ci/ubuntu.Dockerfile` to the packages needed to install and exercise a DEB during the version 1.0.0-to-1.0.1 upgrade.

Update `ci/run` so its first leg builds and runs Debian 13, then retains Fedora 44. Update `ci/package` so Debian builds and clean-validates the DEB, Ubuntu only validates the optional DEB upgrade, and Fedora builds and validates the RPM. Replace version 0.4.0 upgrade identities and hashes with the published version 1.0.0 identities and hashes. Update both validator scripts for version 1.0.1 and update `tests/test_package_command.py` to prove exact command order, mounts, baselines, hashes, terminal outputs, and separate failures.

Update current documentation in Simple English. Preserve historical version 1.0.0 statements and publish version 1.0.1 through the protected GitHub delivery path.

This task has one atomic proof boundary because the shared version and package command must identify a complete two-format candidate. Run focused validation and package lifecycle proof before one complete checkpoint review. Commit, publish, merge, tag, and release only because the user later requested those operations explicitly.

## Concrete Steps

Work from `/home/mbeutler/Projects/synodrive-dolphin` on branch `task/SDD-016-debian13-packaging`, based on commit `bf9abc35ea612ab9b39305f8bd72e74b27585184`.

Run shell syntax checks for the changed scripts and the focused Python package-command test. Configure and build the host tree, then run CTest if the build owners require it. Run the direct CPack scenarios inside their matching distribution containers and pass each artifact to `ci/validate-package`.

Use the previously downloaded, checksum-verified published assets in `/tmp/tmp.TiyDnBaFUa` and run:

    ./ci/package --upgrade-from /tmp/tmp.TiyDnBaFUa

Expect these final paths only after both clean and upgrade lifecycle sets pass:

    build/packages/debian-13/synodrive-dolphin_1.0.1-1_amd64.deb
    build/packages/fedora-44/synodrive-dolphin-1.0.1-1.x86_64.rpm

Inspect the DEB dependency field with:

    dpkg-deb -f build/packages/debian-13/synodrive-dolphin_1.0.1-1_amd64.deb Depends

Expect a Qt Core floor compatible with Debian 13 and no `libqt6core6t64 (>= 6.10.2)` text.

Record detailed validation evidence in the ignored file `.agent/test-results/SDD-016.md`. Reviewers inspect source, diffs, and that existing evidence only; they run no command. After the complete candidate receives a clean final review and while its tree is unchanged, run:

    ./ci/run

Expect the Debian 13 leg and then the Fedora 44 leg to build and pass the complete test collection. If that gate requires a source change, repeat validation and review before rerunning it.

## Validation and Acceptance

Acceptance requires the focused command tests, both direct CPack archive checks, both clean package lifecycles, both published-version upgrades, and the final post-review distribution gate to pass. The exact DEB dependency metadata must be satisfiable on Debian 13. The exact RPM and DEB candidate identities and full payloads must agree on version 1.0.1 release 1.

Every current document must identify version 1.0.1 as the current release and retain version 1.0.0 as its upgrade baseline. No production C++ source may change. No package or container may contain or download Synology software.

## Idempotence and Recovery

The build, focused tests, package generation, lifecycle validation, and local CI commands are repeatable. They replace their own generated build outputs. A missing or altered published baseline must stop before Docker starts. A lifecycle failure must stop later legs and suppress final candidate paths.

Do not weaken a dependency or validator to make a failed package pass. Diagnose and correct the build owner. If the reviewed tree changes after final review, invalidate the review and repeat every affected gate. Create the immutable tag and release only from the verified merged `main` commit.

## Artifacts and Notes

The published version 1.0.0 package names are:

    synodrive-dolphin_1.0.0-1_amd64.deb
    synodrive-dolphin-1.0.0-1.x86_64.rpm

The published DEB SHA-256 is `b080ca138ffbfb5383beae9008e3aadb2f0ee6c56e2b3b46612bdf951d75fe0a`. The published RPM SHA-256 is `b804cb8a29f9d0ddf5acdc442db3d07f3d302922456c7cb3ac7a336602193f3e`.

## Interfaces and Dependencies

The public package command remains `./ci/package [--upgrade-from DIR]`. The optional directory contains the exact published version 1.0.0 DEB and RPM. No other option is accepted.

`ci/validate-package-lifecycle` retains `DEB|RPM ARTIFACT [BASELINE]`. Without a baseline, it performs a clean install. With a baseline, it installs version 1.0.0 and upgrades to version 1.0.1 before applying every existing candidate check.

No new runtime or third-party dependency is added. Debian 13 supplies the Qt 6 and KDE Frameworks 6 build dependencies. Ubuntu 26.04 supplies only the runtime dependencies needed for its DEB upgrade check. Fedora 44 remains unchanged except for the shared candidate and baseline version identities.

This revision creates the SDD-016 specification, incorporates the accepted preflight corrections, formally supersedes the prior Ubuntu build-target decisions, and records the user's later instruction to publish version 1.0.1.
