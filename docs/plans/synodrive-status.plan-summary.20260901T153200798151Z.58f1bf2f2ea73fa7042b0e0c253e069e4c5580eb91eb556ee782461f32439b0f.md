<proposed_plan>
# SDD-002: Local Distribution CI

## Summary

SDD-002 is viable. Both target containers configure and build successfully. Ubuntu 26.04 and Fedora 44 pass both CTest tests when Docker uses `--init`.

Docker’s init process reaps orphaned children, which the plugin teardown test requires. The source can remain read-only during the complete build. [Docker documents `--init`](https://docs.docker.com/reference/cli/docker/container/run/#specify-an-init-process) and [read-only bind mounts](https://docs.docker.com/engine/storage/bind-mounts/#use-a-read-only-bind-mount).

Preflight: ready — add one POSIX shell entry point and two Dockerfiles. Do not change production code.

## Implementation Changes

- Update `docs/roadmap.md` before other edits:
  - Move SDD-002 to Active and redefine it as local distribution CI.
  - Move SDD-006 to Planned later, after `v1.0.0`.
  - Remove SDD-006 from the v1.0 production-readiness gate.
  - Allocate SDD-010 for post-v1 GitHub CI.
  - Make SDD-010 reuse the local command and require its uniquely named check on `main` through branch protection. [GitHub supports required status checks on protected branches](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-protected-branches/about-protected-branches#require-status-checks-before-merging).
  - Keep the v1.0 package release manual. Add no GitHub workflow or setting now.
- Create `docs/plans/SDD-002-local-ci.md` as the single ExecPlan. Include the accepted preflight boundaries, checks, and applicable Plan history.
- Add the public command `./ci/run`:
  - Accept no arguments. An argument prints `usage: ./ci/run` and exits 2.
  - Run Ubuntu first and Fedora second. Stop on the first error.
  - Build cached images named `synodrive-dolphin-ci:ubuntu-26.04` and `synodrive-dolphin-ci:fedora-44`.
  - Use `ci/` as the Docker build context. Do not copy source into either image.
  - Run containers with `--rm`, `--init`, `--network none`, and read-only `/src`.
  - Build in disposable `/build`.
  - Assert the expected `VERSION_ID` and `x86_64` architecture.
  - Configure with Ninja and `RelWithDebInfo`, build, then run unfiltered CTest with failure output.
  - Propagate native Docker and build errors once. Add no retry or custom daemon handling.
- Add two minimal Dockerfiles:
  - Ubuntu: `ubuntu:26.04`, CMake, Ninja, Python, G++, Qt 6 development files, and KF6 KIO development files.
  - Fedora: `fedora:44`, the equivalent package set, with weak dependencies disabled.
  - Permit only the exact base image, locale, package installation, and package-cache cleanup instructions.
- Update README Development:
  - Present `./ci/run` as the full distribution gate.
  - Retain the host CMake commands as the fast development loop.
  - State that the first image build needs network access.
- Do not change installation or usage documentation unless the implementation makes current text incorrect.

## Validation

- Run `sh -n ci/run`.
- Confirm that an unsupported argument exits 2 before any Docker call.
- Use a temporary Docker command shim to prove Ubuntu-first ordering, both-leg success, and outer failure propagation.
- Run `./ci/run` twice. Require 2/2 CTest passes in each distribution on both runs.
- Confirm Ubuntu `VERSION_ID=26.04`, Fedora `VERSION_ID=44`, and `uname -m=x86_64`.
- In a disposable source copy, register one deterministic failing CTest. Require the real `./ci/run` command to exit nonzero.
- Compare the tracked checkout before, between, and after both successful runs. Require no source mutation.
- Enforce an exact Dockerfile allowlist. Reject `ADD`, `COPY`, remote URLs, extra repositories, acquisition tools, and undeclared packages.
- Confirm that the task adds no `.github/workflows` path and changes no production C++ source.
- Finish documentation maintenance, complete correctness review, then move SDD-002 to Completed.

## Assumptions and Defaults

- The supported host is x86_64 Linux with a reachable local Docker Engine.
- Image acquisition and later refreshes can use network access. Offline operation is not promised.
- Release tags remain pinned to `ubuntu:26.04` and `fedora:44`. Immutable image digests are not maintained.
- Docker is the only supported container engine. Podman, Compose, Make, and additional task runners remain out of scope.
- Tests use repository fake helpers. CI never downloads or mounts Synology software.
- All GitHub Actions, including tagged-release automation, start only after `v1.0.0`.

## Prior Plan Reconciliation

- Record: `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`
- Record: `docs/engineering-task.plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`
- Status: superseded
- Scope: exact-version-only support and the earlier packaging and context-menu exclusions.
- Reason: the accepted production-readiness roadmap expanded those areas.
- Replacement: the SDD-003 through SDD-008 roadmap work. Process isolation, fail-closed behavior, status mappings, and no Synology redistribution remain carried.

- Record: `.agent/plan-history/plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- Record: `docs/engineering-task.plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- Record: `docs/plans/synodrive-status.plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- Record: `docs/roadmap.plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- Status: superseded
- Scope: SDD-002 GitHub execution, SDD-006 pre-v1 placement, and the v1.0 completion formula.
- Reason: the user requires local CI before v1.0 and selected deferral of all GitHub Actions.
- Replacement: SDD-002 supplies local Docker CI. SDD-006 and new SDD-010 start after v1.0. The v1.0 release remains manual.
- All decisions outside this scope remain carried.

<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/SDD-002-local-ci.md -->
</proposed_plan>