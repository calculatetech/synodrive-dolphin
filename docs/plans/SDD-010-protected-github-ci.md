# Add protected GitHub CI for version 1.0

This ExecPlan records the completed protected GitHub CI gate. Maintain it in accordance with `/home/mbeutler/.codex/PLANS.md`.

## Purpose / Big Picture

Changes after version 0.4.0 must reach `main` through a reviewed pull request. A contributor keeps the pull request in draft, requests review, and marks it ready only after review is clean. That transition runs the same Ubuntu and Fedora gate that developers can run locally with `./ci/run`.

## Decision Log

- Decision: Use one GitHub ruleset for the default branch.
  Rationale: A ruleset can name the required check before its first run and can apply without an administrator bypass.
- Decision: Trigger pull-request CI only on `ready_for_review`.
  Rationale: The repository rule prohibits CI before review is clean. Draft-first is the accepted contributor workflow.
- Decision: Use one job named `distribution-gate` that runs `./ci/run`.
  Rationale: One stable status avoids ambiguous required checks and keeps local and hosted behavior identical.
- Decision: Complete SDD-010 inside the active SDD-009 release task.
  Rationale: The user moved protected GitHub CI before version 1.0.0 and requested one fast-tracked release path.

## Outcomes & Retrospective

Version 1.0 uses a protected, draft-first pull-request workflow. The outcome is visible when the reviewed release pull request and its merged `main` commit both have a successful `distribution-gate` result.

GitHub ruleset `22244652` protects `main` without a bypass actor. It requires a pull request, resolved conversations, and the strict `distribution-gate` check.

The task-branch push and draft pull request created no Actions run. Local Ubuntu 26.04 and Fedora 44 gates passed after both reviews were clean.

The first ready transition started one hosted run for commit `7e3eb7adb3f1da3c3e676fbf2ecd1c0fd679def8`. Run `33835906565` passed in 20 minutes and 12 seconds.

## Context and Orientation

`ci/run` is the existing no-argument local distribution gate. It builds pinned Ubuntu 26.04 and Fedora 44 Docker images and runs the complete CTest collection in both. It does not download Synology software.

`.github/workflows/ci.yml` is the only hosted validation workflow. `CONTRIBUTING.md` explains the draft-first transition. `docs/roadmap.md` owns the task status. SDD-009 in `docs/plans/SDD-009-v1-release.md` owns the combined release sequence.

The task branch is `task/SDD-009-v1-release`, based on version 0.4.0 commit `b055b80cd9f131de5cef86427d0f7d26cfeffec6`.

## Product Boundary

The product boundary follows `/home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md` and `/home/mbeutler/.agents/skills/design-preflight/references/supported-model.md`.

The GitHub ruleset, validation workflow, contributor transition, and pull-request integration compose with this task. The existing `ci/run` behavior is opaque. Release publication is owned by SDD-009. Tag-triggered release automation, path filters, matrices, signing, attestations, and hosted package repositories are deferred.

The accepted plan is `.agent/plan-history/plan-summary.20260904T030553255401Z.890479e1a919a22ebb44fbae67c784f36e2c4699a5489d33f0b470aabb1f0208.md`.

It supersedes the post-version-1.0 GitHub timing in the SDD-002, SDD-007, SDD-008, and SDD-011 plan records. Their local gate, isolation, compatibility, and runtime decisions remain in force.

## Scenario Proof

A task-branch push and a draft pull request must create no Actions run. After the current pull-request head has clean local and GitHub reviews, `./ci/run` must pass locally. Marking that draft ready must create exactly one required `distribution-gate` result for the current head.

A correction must first return the pull request to draft. Its push must create no Actions run. A new clean review and ready transition must create a new result; an older commit result cannot satisfy the updated head.

After squash merge, the `main` push must run the same job. Its exact commit must pass before release tagging.

## Plan of Work

Create `.github/workflows/ci.yml`. Give the workflow read-only contents permission and a 60-minute timeout. Trigger it only for `pull_request` `ready_for_review` events that target `main`, and for pushes to `main`. Add one `distribution-gate` job on `ubuntu-latest`. Pin `actions/checkout` to `3d3c42e5aac5ba805825da76410c181273ba90b1`, then run `./ci/run`.

Create `CONTRIBUTING.md`. Tell contributors to open a draft pull request, request `@codex review`, resolve findings while draft, and mark it ready only when review is clean. Tell them to return a ready pull request to draft before each correction.

Before the first branch push, create an active ruleset for the default branch. Require pull requests, resolved conversations, a strict current `distribution-gate`, and fast-forward history. Block deletion and force updates. Configure no bypass actor and no required approval count.

## Concrete Steps

Run commands from `/home/mbeutler/Projects/dolphin-drive`.

Inspect the workflow without starting CI. Confirm its triggers, permissions, job name, action pin, and command. Inspect the GitHub ruleset through the API. Push the task branch and open a draft pull request. Confirm that GitHub reports no Actions run for either event.

After all release prerequisites and reviews are green, the coordinator runs:

    ./ci/run

Then mark the draft ready. Expect one required `distribution-gate` result whose log contains successful Ubuntu 26.04 and Fedora 44 terminal results.

## Validation and Acceptance

Reviewers inspect source, diffs, and existing evidence only. They must not run builds, tests, package validation, or CI. The coordinator must not start local or hosted CI until every other publication gate and current-head review is green.

The ruleset must be active before the first branch push. A draft must have no run. A ready transition must run the gate. The final pull-request head and merged `main` commit must each have a successful result named exactly `distribution-gate`.

If a project-controlled CI failure occurs, return the pull request to draft, apply the smallest correction, run its direct non-CI check, and repeat review before another ready transition. Report an external GitHub failure without bypassing protection.

## Idempotence and Recovery

Ruleset inspection, draft conversion, review requests, and check inspection are repeatable. Never retry an unchanged failed run. Never push directly to `main`, weaken protection, or reuse review evidence from an older head.

## Interfaces and Dependencies

The permanent local interface remains `./ci/run` with no arguments. The hosted status interface is exactly `distribution-gate`. The public contribution interface is draft, review, ready, CI, then merge.

The workflow uses only GitHub-hosted runners, the pinned official checkout action, Docker, and repository scripts. It needs no write permission and no new project dependency.

This revision records the accepted pre-version-1.0 CI timing and draft-first workflow.
