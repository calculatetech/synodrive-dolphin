<proposed_plan>
# SDD-011 — Eliminate idle overlay polling for v0.2.0

## Summary

- Preserve published `v0.1.0` and its tag without modification.
- Release SDD-003 and SDD-011 as `0.2.0`.
- Update the source version from `0.1.0` to `0.2.0` before final validation.
- The CPU defect comes from one-second queries and unchanged overlay notifications forming a feedback loop.
- Stable directories must create zero query children until Dolphin requests them again.
- Recent `syncing` paths poll once per second until stable or inactive for 30 seconds.
- Keep the persistent `synodrive-status --stdio` wrapper and fresh query children.

### Permanent-loading verdict

Permanent loading is not a safety concern under the accepted crash policy. A Synology fault may require a Dolphin restart.

Direct loading into Dolphin remains out of scope because:

- It provides no stable-idle benefit after SDD-011.
- Earlier same-process tests remained incorrectly stuck on `syncing` for 180 seconds. See the [transition report](/home/mbeutler/Projects/dolphin-drive/.agent/test-results/synodrive-status.md:69).
- KDE calls `getOverlays()` on Dolphin’s main thread and requires a quick return. [KDE API documentation](https://api.kde.org/koverlayiconplugin.html)
- The current asynchronous process boundary already exists and works. [Qt QProcess documentation](https://doc.qt.io/qt-6/qprocess.html)

Create SDD-012 to test a persistent out-of-process loader with a working GLib main context. [GLib main-loop documentation](https://docs.gtk.org/glib/main-loop.html)

## Implementation changes

### Version and delivery lifecycle

1. Commit the reviewed SDD-003 checkpoint without changing its version. Local checkpoints do not consume versions.
2. Create `task/SDD-011-idle-performance` from that commit.
3. Create the SDD-011 ExecPlan before source edits.
4. Link every applicable Plan history record from that ExecPlan.
5. Copy the unlinked `20260901T181723` record beside the new ExecPlan without changing its bytes.
6. Add the replacement Plan record produced by this plan through the same handoff.
7. Preserve `.mcp.json` and `.vscode/`.
8. Change `project(synodrive-dolphin VERSION 0.1.0 ...)` to version `0.2.0`.
9. Update all release-facing version references before final validation.
10. Do not publish `v0.2.0` until SDD-011 passes review and performance acceptance.

The future release identity is:

- Source version: `0.2.0`
- Git tag: `v0.2.0`
- Release type: normal GitHub release
- Release title: `Synology Drive Dolphin Extension (Unofficial) 0.2.0`

Never move or reuse `v0.1.0` or `v0.2.0`.

### Provider scheduling

- Keep one shared timer.
- Use one-second intervals only while a recent cached entry is `Syncing`.
- Use a 30-second interval only for silent stable-cache cleanup.
- Stop the timer when the cache is empty.
- Only Dolphin demand renews a path’s access timestamp.
- Timer queries and responses must not renew access.
- Remove inactive entries without a query or notification.
- Never query stable cached entries from the timer.
- Preserve active and queued request deduplication.

### Demand and notification flow

- Each local `getOverlays()` call returns the cached result immediately.
- It also queues one deduplicated asynchronous query, including on cache hits.
- Non-local URLs remain inert.
- Change the internal provider signal to include previous and current raw status.
- Emit that signal only when the raw status changes.
- Emit KDE `overlaysChanged` only when the mapped overlay list changes.
- An unchanged confirmation response must end the notification cycle.
- Preserve all six existing status mappings.

### Failure handling

- Test error frames and wrapper exits as separate paths.
- Clear the complete active and queued request set.
- Invalidate affected cache records.
- Notify Dolphin only when a visible overlay changes.
- Allow later Dolphin demand to restart the wrapper.
- Preserve the CLI, NUL framing, Drive 8.x checks, ABI checks, and fresh-child isolation.
- Add no dependency, thread, protocol, worker pool, configuration option, or telemetry.

### Roadmap and documentation

- Add SDD-011 as the active `v0.2.0` release correction.
- Add SDD-012 as planned work for a persistent GLib-aware worker.
- SDD-012 must prove live transitions, repeated queries, crash recovery, and lower active-sync CPU.
- Reject SDD-012 if it becomes stale, blocks, or requires direct Dolphin loading.
- Keep SDD-012 outside the `v0.2.0` release gate.
- Record baseline and final performance results in the SDD-011 ExecPlan.
- Add a supersession note to the original status plan.
- Update usage documentation for demand-driven stable refresh.
- Add `v0.2.0` release notes covering Drive 8.x support, the idle CPU fix, and delayed stable-state detection.

## Test and acceptance plan

### Deterministic tests

- Parameterize every stable status and prove no timer query occurs.
- Prove a later Dolphin demand creates one deduplicated query.
- Test mixed `Syncing` and stable cache entries:
  - Poll only the syncing entry.
  - Do not duplicate a held query.
  - Do not renew access from polling.
  - Expire inactive entries silently.
- Prove syncing polling stops after a stable response.
- Prove identical statuses do not notify.
- Prove `Unknown ↔ Unsupported` does not notify.
- Prove each visible overlay change notifies once.
- Cover error-frame recovery and unexpected wrapper exit.
- Prove expired empty-overlay records leave the cache.
- Preserve FIFO, full-set failure, nonblocking, selector, teardown, CLI, and fresh-child tests.

### Validation

- Require the source version to equal `0.2.0`.
- Run the complete host CTest suite.
- Run `./ci/run` on the existing Ubuntu and Fedora targets.
- Run the live Synology transition test.
- Keep GitHub-hosted CI and branch protection deferred until after `v1.0.0`.

### Installed-desktop performance acceptance

1. Trace process creation before opening a stable Drive directory.
2. Wait for the initial overlay and confirmation query.
3. Require zero later query children during the next 35 seconds.
4. Continue for five minutes as supporting idle evidence.
5. Record `pidstat`, but do not gate on total Dolphin CPU.
6. Modify a harmless visible synced file.
7. Require Dolphin to discover `Syncing`.
8. Require polling to reach `Synced` within two seconds after the helper settles.

The hard performance gate is zero stable-idle query children.

## Prior plan reconciliation

Record: `.agent/plan-history/plan-summary.20260901T181723354265Z.e5524360a445bc95804a35781f50d2b095f673ff5d690f019e6d493a8279d1fe.md`  
Status: superseded  
Scope: release identity and publication statements only  
Reason: `v0.1.0` is already published, and its Git tag has consumed that version.  
Replacement: SDD-003 and SDD-011 target source version `0.2.0` and Git tag `v0.2.0`.

All performance, loading, behavior, testing, and documentation decisions from that record are carried forward.

- The `20260901T045809` record remains carried for process isolation and nonblocking callbacks.
- The `20260901T141135` record remains superseded only for periodic stable refresh and expiry notifications.
- The `20260901T153200` record remains carried for local CI and post-1.0 GitHub enforcement.
- The `20260901T163218` record remains carried for Drive 8.x compatibility and fresh query children.
- Published `v0.1.0` remains the completed SDD-001 proof of concept.
- `v0.2.0` becomes the next release and must not reuse any `v0.1.0` artifact or tag.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/plans/SDD-011-idle-performance.md -->

Review: preflight completed; implementation and release review remain pending.

Docs: roadmap, ExecPlan, usage, version references, and `v0.2.0` release notes are planned.
</proposed_plan>