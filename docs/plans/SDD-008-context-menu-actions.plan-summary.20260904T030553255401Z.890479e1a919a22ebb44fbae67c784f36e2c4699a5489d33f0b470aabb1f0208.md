<proposed_plan>
# Fast-track v1.0.0 from v0.4.0

## Summary

Release `1.0.0` from commit `b055b80`, preserving the v0.4.0 runtime behavior. Use one combined protected PR for SDD-009 and SDD-010.

Preflight: ready. The user retired the old legal-review requirement and selected a draft-first PR lifecycle. SDD-012 is declined because its live-currentness test failed.

## Implementation changes

- Create `task/SDD-009-v1-release` from the verified `origin/main`/v0.4.0 commit. Preserve unrelated `.mcp.json`, `.vscode/`, worktrees, and user files.
- Reconcile plan history:
  - Supersede the earlier post-1.0 timing for hosted CI and branch protection.
  - Record SDD-012 as Declined while retaining its failed experiment, plan, and evidence.
  - Retire the superseded legal-review gate without rewriting historical records.
  - Keep all released compatibility, isolation, package, context-action, performance, and tray-patch contracts.
- Use SDD-009 as the sole Active umbrella task. Add ExecPlans for SDD-009 and SDD-010. After hosted CI is proven, finalize both as Completed in the same PR.

### GitHub protection and CI

- Before the first branch push, create an active GitHub ruleset for the default branch:
  - Require pull-request integration and conversation resolution.
  - Require the strict, current-head status `distribution-gate`.
  - Block deletion and force/non-fast-forward updates.
  - Configure no bypass actors and zero required approving reviews.
- Add one workflow:
  - Events: `pull_request` to `main` with only `ready_for_review`, plus `push` to `main`.
  - Permissions: `contents: read`.
  - One Ubuntu job named exactly `distribution-gate`.
  - Timeout: 60 minutes.
  - Pin `actions/checkout` to commit `3d3c42e5aac5ba805825da76410c181273ba90b1`.
  - Execute the existing `./ci/run` without filters or replacement logic.
- Document the permanent draft-first convention: open or convert every PR to draft, request `@codex review`, resolve findings, and mark ready only when reviews are clean. Corrections return the PR to draft before a new commit.
- Do not add branch-push, `opened`, `synchronize`, manual, matrix, path-filter, or release-publishing triggers. GitHub rulesets support named required checks, and `ready_for_review` provides the deliberate CI transition. [GitHub ruleset documentation](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-rulesets/creating-rulesets-for-a-repository), [workflow event documentation](https://docs.github.com/en/actions/reference/workflows-and-actions/events-that-trigger-workflows#pull_request).

### Release candidate

- Change every current release identity to `1.0.0` while preserving historical v0.4.0 references and upgrade-baseline references.
- Produce exactly:
  - `synodrive-dolphin_1.0.0-1_amd64.deb`
  - `synodrive-dolphin-1.0.0-1.x86_64.rpm`
- Do not change runtime C++ behavior or its behavioral tests.
- Extend `./ci/package` with exactly one optional interface: `--upgrade-from DIR`.
  - The directory must contain the published v0.4.0 DEB and RPM.
  - Verify both files against the recorded v0.4.0 SHA-256 digests.
  - Run the existing clean lifecycle checks.
  - Install each v0.4.0 package, upgrade it natively to the same-format v1 package, then reuse the existing identity, six-file inventory, dependency, loading, passive-tray, integrity, removal, and cleanup checks.
  - Keep the no-argument behavior unchanged.
  - Do not download release assets from package scripts or hosted CI.
- Update README, installation, usage, release notes, and the current technical report:
  - State the x86_64, KDE/Qt 6, Drive 8.x/internal-major-4/ABI-15 support contract.
  - State Ubuntu 26.04 support and Fedora 44 package validation.
  - Request Fedora runtime reports and pull-request fixes.
  - Add v1 install, upgrade, verification, and removal commands.
  - Remove the retired legal-review statement from current documentation.
- Add:
  - `SECURITY.md`, supporting the current 1.0.x line and routing reports through GitHub private vulnerability reporting.
  - A focused bug-report form covering distribution, installation method, KDE/Dolphin version, Drive public/internal/ABI versions, affected feature, reproduction, expected/actual behavior, diagnostic output, and a privacy warning.
  - Concise contribution instructions for the draft-first review and CI lifecycle.
- Keep MIT licensing. Do not add Synology payloads, signing, attestations, hosted package repositories, ARM support, or release automation.

## Validation and publication sequence

1. Verify the branch base, runtime diff, version owners, documentation agreement, workflow syntax, and protection configuration.
2. Download the published v0.4.0 packages into a temporary directory and run the exact release package gate:
   `./ci/package --upgrade-from <directory>`.
3. Install the exact v1 DEB on the Ubuntu desktop and record:
   - overlay visibility;
   - a live syncing-to-stable transition;
   - operation after restarting Dolphin;
   - user-driven Get link and Browse previous versions actions;
   - clean package removal and absence of owned files.
   Reinstall the v1 candidate afterward so the desktop remains usable. Do not automate Synology windows.
4. Run focused host build and CTest checks. These and package validation are pre-review evidence, not the aggregate CI gate.
5. Complete documentation maintenance and roadmap trace checks. Fresh reviewers inspect the exact candidate and existing evidence only; reviewers execute no build, test, package, or CI command.
6. Commit and push the branch, open a draft PR, and confirm that the branch push and draft PR created no Actions run. Request `@codex review` and resolve every finding on the current head.
7. Only after every non-CI publication gate and both reviews are green, run local `./ci/run`. Mark the PR ready and require one successful hosted `distribution-gate`.
8. For a project-controlled failure:
   - Return the PR to draft.
   - Apply the smallest fix and rerun its direct non-CI check plus affected prerequisites.
   - Repeat local and GitHub reviews.
   - Run local CI, mark ready, and obtain a new current-head hosted result.
   External GitHub failures stop publication and are reported without bypasses.
9. After the first hosted proof passes, return the PR to draft and add the final roadmap/ExecPlan outcome commit marking SDD-009 and SDD-010 Completed. Rebuild the exact packages, repeat clean reviews, run local CI, then mark ready for the final current-head hosted CI.
10. Squash-merge only when the latest reviewed head has the green required check. Verify the merged tree equals the reviewed candidate. Require the main-push `distribution-gate` to pass; if it fails, fix it through another protected draft-first PR.
11. Record both package digests but upload no `SHA256SUMS` asset; that remains part of SDD-006.
12. Create annotated tag `v1.0.0` on the verified merged main commit and publish a normal/latest GitHub release titled `Synology Drive Dolphin Extension (Unofficial) 1.0.0`, containing exactly the validated DEB and RPM.
13. Verify the tag, CMake version, package identities, asset digests, release state, and equality of local `main` and `origin/main`.

## Assumptions and fixed boundaries

- The user explicitly retired the inherited legal-review requirement for v1.0.0.
- Draft-to-ready is the only supported PR-to-CI transition. Ready PR corrections must first return to draft.
- SDD-012 production and test code must not enter v1.0.0.
- Prior v0.4.0 live evidence remains valid, but the exact v1 DEB still receives the roadmap-required Ubuntu smoke test.
- SDD-006 remains planned for later release automation, checksums, and attestations.
- No direct main push, protection bypass, unreviewed correction, reviewer-run validation, or early CI is permitted.

<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-002-local-ci.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/plans/SDD-004-packaging.md -->
<!-- cpk-plan-spec: docs/plans/SDD-005-package-lifecycle.md -->
<!-- cpk-plan-spec: docs/plans/SDD-007-context-menu.md -->
<!-- cpk-plan-spec: docs/plans/SDD-008-context-menu-actions.md -->
<!-- cpk-plan-spec: docs/plans/SDD-009-v1-release.md -->
<!-- cpk-plan-spec: docs/plans/SDD-010-protected-github-ci.md -->
<!-- cpk-plan-spec: docs/plans/SDD-011-idle-performance.md -->
<!-- cpk-plan-spec: docs/plans/SDD-012-persistent-status-worker.md -->
<!-- cpk-plan-spec: docs/plans/SDD-015-tray-patch.md -->
</proposed_plan>