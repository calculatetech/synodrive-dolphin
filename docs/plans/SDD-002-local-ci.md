# Add local distribution CI

This ExecPlan is a living specification until review closure. Keep durable decisions, scope, instructions, and design-shaping discoveries current until then.

Maintain this document according to `/home/mbeutler/.codex/PLANS.md`. The task uses branch `sdd-002-local-ci`, based on `main` commit `322e2d1fca544a3c2043b0f216020d0ec1606900`.

## Purpose / Big Picture

After this change, a developer can run `./ci/run` from an x86_64 Linux checkout. This one command builds and runs all tests in Ubuntu 26.04 and Fedora 44 containers. It returns zero only after both distribution legs pass.

The local gate is required before v1.0. GitHub Actions, release workflows, and branch protection start only after the `v1.0.0` release.

## Progress

- [x] (2026-09-01) Reconciled the accepted Plan records and moved SDD-002 to Active.
- [x] (2026-09-01) Added the local runner, Ubuntu image, Fedora image, and README instructions.
- [x] (2026-09-01) Completed syntax, argument, ordering, failure, container, repeatability, and scope checks.

## Surprises & Discoveries

- Observation: Both target containers build the project, but the plugin test needs a container init process.
  Evidence: Without `docker run --init`, `PluginTest::destructionDoesNotWait` observed an orphaned query child for approximately 17.5 seconds in both distributions. With `--init`, Ubuntu and Fedora each passed both CTest tests in approximately 2.6 seconds.

- Observation: Fedora development dependencies include many weak packages by default.
  Evidence: `dnf --setopt=install_weak_deps=False` reduced the proof environment from 507 packages to 290 packages without changing the build result.

## Decision Log

- Decision: Use one executable POSIX shell command at `ci/run` and two Dockerfiles.
  Rationale: Docker is already available. Separate Dockerfiles keep the two package-manager commands explicit and cacheable.

- Decision: Accept no command arguments.
  Rationale: SDD-002 needs one complete distribution gate. Optional selectors add an unsupported public interface.

- Decision: Run Ubuntu before Fedora and stop on the first error.
  Rationale: A successful result requires both legs. Failure aggregation adds no accepted result.

- Decision: Use `--rm`, `--init`, `--network none`, and a read-only source mount for each test container.
  Rationale: The tests need child reaping, no network, and no permission to change the checkout.

- Decision: Use release tags instead of immutable image digests and do not force image pulls.
  Rationale: The tags fix the distribution releases. Normal Docker layer caching keeps repeated local runs practical.

- Decision: Defer all GitHub Actions, including tagged-release automation, until after `v1.0.0`.
  Rationale: The user selected the all-Actions deferral. The v1.0 package release remains manual.

## Outcomes & Retrospective

The local gate now runs the complete CTest collection on Ubuntu 26.04 and Fedora 44. Two repeated runs passed without changing the tracked source.

A recording Docker shim proved ordering and outer failure propagation. A disposable failing CTest proved that an inner test failure makes the gate fail.

The implementation needed no production-code or CMake change. Docker `--init` was the only container behavior needed beyond the original plan.

## Context and Orientation

`CMakeLists.txt` builds a C++17 command and a KF6 Dolphin overlay plugin. It registers two CTest tests named `cli` and `plugin`. The tests build fake Synology libraries and pass their paths through test-only environment variables.

`README.md` currently documents a host configure, build, and CTest sequence. That sequence remains the fast development loop. The new command becomes the full two-distribution acceptance gate.

`docs/roadmap.md` is the only lifecycle authority. SDD-002 must remain its only Active task until final clean review.

The applicable immutable Plan record is `.agent/plan-history/plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md`. Its byte-identical sibling for this specification is `docs/plans/SDD-002-local-ci.plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md`.

## Product Boundary

Apply [Scope boundaries](/home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md).

The current user requirement and this ExecPlan are the authoritative task sources.

- `composes`: `docs/roadmap.md`, the new local CI command, the two toolchain images, and README development instructions.
- `opaque`: Docker Engine, existing CMake targets, the complete existing CTest collection, fake helpers, and production process lifecycle.
- `deferred`: DEB and RPM generation, package lifecycle checks, context menus, live Synology checks, GitHub Actions, GitHub release automation, and branch protection.

Do not change production C++, test timing, CMake test registration, or Synology integration. Do not add Podman, Compose, Make, another task runner, an image updater, or retained host build artifacts.

The supported model is one developer on x86_64 Linux with a reachable local Docker Engine. Initial image acquisition and later image refresh can use network access. Offline operation, registry failures, mirror failures, retries, interruption recovery, remote Docker, SELinux variants, and other architectures are outside this task.

## Scenario Proof

Apply [Scenario discrimination](/home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md), [Owner composition](/home/mbeutler/.agents/skills/design-preflight/references/owner-composition.md), and [Full-set results](/home/mbeutler/.agents/skills/design-preflight/references/full-set-results.md).

| Boundary | Required contrast and oracle | Production path | Runnable check |
| --- | --- | --- | --- |
| B1: one local aggregate command | One local invocation completes Ubuntu and Fedora once each. Omitting a leg or an outer Docker error makes the command nonzero. | `./ci/run` to Docker build and run calls to the shell exit status | Run the real command. Use a temporary recording Docker shim to prove ordering and outer error propagation. |
| B2: exact targets | Ubuntu 26.04 and Fedora 44 on x86_64 pass. Another release or architecture fails its identity assertion. | Dockerfile base image to `/etc/os-release` and `uname -m` assertions | Retain each real leg log and inspect its identity lines. |
| B3: complete build and test gate | Configure, build, and both registered CTest tests pass in each leg. A failing registered CTest makes the real command nonzero. | Container shell to CMake configure, build, and unfiltered CTest | Run both real legs. In a disposable source copy, register `/bin/false` as a CTest and require `./ci/run` to fail. |
| B4: fake-only integration | Fresh images pass with repository fake helpers. Real Synology software is never acquired or mounted. | CMake fake targets to CTest environment seams | Run both real legs. Enforce the exact Dockerfile instruction and package allowlist. |
| B5: source preservation | Zero, one, and two invocations leave the tracked checkout snapshot equal. | Read-only `/src` to disposable `/build` | Hash tracked status and diff before, between, and after two runs. |
| B6: local CI before v1 | SDD-002 adds the local gate and no GitHub workflow or setting. Post-v1 GitHub CI has a permanent roadmap owner. | Task diff to `docs/roadmap.md` | Reject `.github/workflows` changes. Require the SDD-002 and SDD-010 roadmap text. |
| B7: all Actions after v1 | SDD-006 is Planned later and absent from the v1 completion set. A pre-v1 release workflow is wrong. | User decision to `docs/roadmap.md` | Require the SDD-006 post-v1 gate and the revised production-readiness sentence. |
| B8: command grammar | Zero arguments start the gate. One arbitrary argument exits 2 with exact usage and makes no Docker call. | Shell argument gate to terminal status | Invoke `./ci/run unexpected` with a recording Docker shim. |

The CTest collection is opaque and unfiltered. Every registered test owes one result in each distribution. No grouping, deduplication, or result limit applies.

## Plan of Work

The task has one semantic subtask, `SDD-002-A`. Its observable outcome is a documented `./ci/run` command that passes the complete test set in both distributions. The primary owner is `ci/run`. The allowed change boundary is `ci/`, README development instructions, this ExecPlan, applicable Plan records, and roadmap lifecycle text.

Create `ci/ubuntu.Dockerfile` with exactly these functional instructions:

    FROM ubuntu:26.04
    ENV LANG=C.UTF-8 LC_ALL=C.UTF-8
    RUN apt-get update \
        && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
            cmake g++ libkf6kio-dev ninja-build python3 qt6-base-dev \
        && rm -rf /var/lib/apt/lists/*

Create `ci/fedora.Dockerfile` with exactly these functional instructions:

    FROM fedora:44
    ENV LANG=C.UTF-8 LC_ALL=C.UTF-8
    RUN dnf install -y --setopt=install_weak_deps=False \
            cmake gcc-c++ kf6-kio-devel ninja-build python3 qt6-qtbase-devel \
        && dnf clean all

Neither Dockerfile can use `ADD`, `COPY`, a remote URL, an extra repository, another package, or an acquisition tool.

Create executable `ci/run` with `#!/bin/sh` and `set -eu`. Resolve the repository root from the script path with shell built-ins. Reject all arguments before the first Docker command.

Define one small shell function that accepts a label, Dockerfile, image name, and expected distribution version. For each leg, build the image with `ci/` as the context. Then run it with `--rm`, `--init`, `--network none`, a read-only `/src` bind mount, `/src` as the working directory, and `EXPECTED_VERSION` in the environment.

The in-container shell must source `/etc/os-release`, print the distribution and architecture, and assert the expected values. It must then run these commands with `&&` so that no error is masked:

    cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build /build
    ctest --test-dir /build --output-on-failure

Call the function for Ubuntu first and Fedora second. Let native Docker and shell errors propagate. Do not add daemon detection, retries, selectors, refresh flags, or cleanup state.

Update the README Development section. Present `./ci/run` first as the complete Ubuntu and Fedora gate. State that its first run downloads the toolchain images. Keep the current host commands and identify them as the fast local loop.

Finalize all substantive documentation before review. Keep SDD-002 Active during review. After final clean review, move SDD-002 to Completed without changing its outcome text.

Complete `SDD-002-A` with the validation below and one clean full native review. Its local commit boundary includes the complete task. No push, pull request, tag, release, or branch cleanup is part of this task.

## Concrete Steps

Run all commands from the repository root.

    sh -n ci/run
    ./ci/run unexpected
    ./ci/run
    ./ci/run
    git diff --check

The invalid invocation must print only `usage: ./ci/run` to standard error and exit 2. Each successful real invocation must show Ubuntu 26.04, Fedora 44, x86_64, and two passed CTest tests per distribution.

Use temporary files outside the tracked checkout for the Docker shim and the deterministic failing-test source copy. Keep the detailed commands and results in `.agent/test-results/SDD-002.md`. This file is ignored and must never be staged.

## Validation and Acceptance

Acceptance requires all these results:

- `sh -n ci/run` succeeds.
- The argument grammar contrast succeeds without a Docker invocation.
- The recording Docker shim proves Ubuntu-first ordering, both-leg success, and outer failure propagation.
- Two complete real runs pass `cli` and `plugin` in both target distributions.
- Both real logs show the exact distribution version and x86_64 architecture.
- The real command returns nonzero for the disposable source copy with a registered failing CTest.
- The tracked checkout snapshot stays equal across both complete real runs.
- The Dockerfiles match the allowed instructions and packages exactly.
- The task diff has no production C++ or `.github/workflows` change.
- `git diff --check` succeeds.
- Documentation maintenance reports that README and roadmap content is current.
- Native correctness review reports no retained applicable finding.

## Idempotence and Recovery

Normal runs are repeatable. Docker reuses image layers. Containers and `/build` disappear after each leg.

Attempt each Docker build and run once. If an operation fails, stop and report the native error. Do not remove Docker images, user files, branches, worktrees, or unrelated untracked files.

## Artifacts and Notes

Keep detailed validation and review evidence only in `.agent/test-results/SDD-002.md`. Preserve Docker images as normal local cache artifacts.

## Interfaces and Dependencies

The only new public interface is:

    ./ci/run

It accepts no arguments. Unsupported input exits 2. Build, identity, or test errors return a nonzero native status.

The only new runtime dependency for developers is Docker Engine. The project gains no production dependency. The implementation uses POSIX shell, Docker, existing CMake commands, and existing tests.

Change note: This specification replaces the earlier pre-v1 GitHub workflow plan with one local Docker gate. It defers all GitHub Actions and branch protection until after v1.0.
