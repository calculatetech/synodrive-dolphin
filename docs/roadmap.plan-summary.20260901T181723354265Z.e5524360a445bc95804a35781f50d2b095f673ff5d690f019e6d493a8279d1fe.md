<proposed_plan>
# SDD-011 — Eliminate idle overlay polling before 0.1.0

## Summary

- The constant CPU use comes from a feedback loop:
  - The provider queries every recent path once per second.
  - It reports unchanged results.
  - Dolphin requests the overlay again.
  - That request renews the path and keeps polling alive.
- Each query costs about 4.8 ms of CPU. One query per second explains the observed 0.3%–0.5% usage.
- A stable directory must create zero query children until Dolphin requests that path again.
- A recent `syncing` path polls once per second. Polling stops when it becomes stable or receives no Dolphin demand for 30 seconds.
- Keep the existing persistent `synodrive-status --stdio` wrapper. Keep one fresh child for each private-library query.

### Permanent-loading verdict

Permanent loading is not a safety concern under the accepted crash policy. A Synology fault may require the user to restart Dolphin.

Direct loading into Dolphin still does not belong in SDD-011:

- It provides no stable-idle benefit after polling stops.
- Earlier same-process tests remained incorrectly stuck on `syncing` for 180 seconds. Fresh children observed the real transition. See the [transition report](/home/mbeutler/Projects/dolphin-drive/.agent/test-results/synodrive-status.md:69).
- KDE calls `getOverlays()` on Dolphin’s main thread and requires a quick return. Direct private-library calls could block the complete UI. [KDE API documentation](https://api.kde.org/koverlayiconplugin.html)
- The current asynchronous process boundary already exists and works with Qt. [Qt QProcess documentation](https://doc.qt.io/qt-6/qprocess.html)

Create SDD-012 to test a persistent, out-of-process loader. It must run the required GLib main context and prove live transitions before adoption. [GLib main-loop documentation](https://docs.gtk.org/glib/main-loop.html)

## Implementation changes

### Delivery sequence

1. Preserve the staged SDD-003 candidate as a local commit. Its reviewed tree is `a776979348a1f0d6afcf42b06f8140613af78910`.
2. Create `task/SDD-011-idle-performance` from that commit.
3. Do not modify the user-owned `.mcp.json` or `.vscode/` files.
4. Keep 0.1.0 unpublished until SDD-011 passes review and performance acceptance.

### Provider scheduling

- Keep one shared timer.
- Use a one-second interval while a recent cached entry is `Syncing`.
- Use a 30-second interval only for silent cache cleanup when all entries are stable.
- Stop the timer when the cache is empty.
- Preserve separate demand timestamps. Only a Dolphin `getOverlays()` call renews them.
- A timer query or query response must not renew demand.
- Remove inactive entries silently. Do not query them or emit a status signal.
- Never query a stable cached entry from the timer.
- Preserve active-request and queued-request deduplication.

### Demand and notification flow

- For every local `getOverlays()` call:
  - Return the cached overlay immediately.
  - Queue one deduplicated asynchronous query, including on cache hits.
- Keep non-local URLs inert.
- Emit the provider’s internal signal only when the raw status changes.
- Include the previous and current raw status in that internal signal.
- Emit KDE `overlaysChanged` only when the mapped overlay list changes.
- A change signal can cause one confirming Dolphin query. An unchanged response must end the cycle.
- Preserve these mappings:
  - `Synced` → `emblem-default`
  - `Syncing` → `emblem-synchronizing`
  - `ReadOnly` → `emblem-locked`
  - `NoPermission` → `emblem-important`
  - `Unknown` and `Unsupported` → no overlay

### Failure handling

- Treat an error frame and a wrapper exit as separate failure paths.
- Clear the complete active and queued request set.
- Invalidate affected cached statuses.
- Notify Dolphin only when invalidation changes the visible overlay.
- Clear request gates so a later Dolphin demand can restart the wrapper.
- Keep the CLI grammar, NUL framing, Drive 8.x checks, ABI checks, status mappings, and fresh-child isolation unchanged.
- Add no dependency, thread, worker pool, protocol, configuration option, or telemetry.

### Roadmap and documentation

- Add SDD-011 as the active pre-publication correction. Complete it only after final review.
- Add SDD-012 as planned work:
  - Load the Synology libraries once in a separate worker.
  - Dispatch the required GLib context.
  - Prove repeated stable queries and `synced → syncing → synced`.
  - Prove recovery after a crash or timeout.
  - Compare active-sync CPU against fresh children.
  - Reject the design if it becomes stale, blocks, or requires direct Dolphin loading.
- SDD-012 is not a 1.0 blocker unless active-sync measurements show a material problem.
- Add the measured baseline and final results to the SDD-011 plan.
- Add a supersession note to the original status plan. Do not rewrite its historical decisions.
- Update usage documentation to explain demand-driven stable refresh and syncing-only polling.

## Test and acceptance plan

### Deterministic tests

- Parameterize all stable statuses. After resolution, several timer intervals must produce no new query.
- A later local `getOverlays()` must produce exactly one deduplicated query.
- Test a mixed cache with `Syncing` A and stable B:
  - Poll only A.
  - Hold an A response across timer intervals.
  - Create no duplicate A query.
  - Never query B from A’s timer.
  - Do not renew A’s demand timestamp from polling.
  - Expire A silently after the inactivity limit.
- Prove `Syncing` polling stops on a stable response.
- Prove identical raw statuses do not notify.
- Prove `Unknown ↔ Unsupported` does not notify because both map to an empty overlay.
- Prove each visible mapping change produces one notification.
- Cover both an error frame and an unexpected wrapper exit.
- After wrapper exit, require overlay invalidation, cleared gates, restart, and recovery.
- Expire `Synced`, `Unknown`, and `Unsupported` entries silently.
- Verify an expired empty-overlay entry returns no cached status.
- Preserve FIFO, full-set failure, held-response, local-selector, teardown, CLI, and fresh-child identity tests.

### Validation

- Run the complete host CTest suite.
- Run `./ci/run` for the existing Ubuntu and Fedora container matrix.
- Run the existing live Synology transition test.
- Keep GitHub-hosted CI disabled until the existing post-1.0 branch-protection task.

### Installed-desktop performance acceptance

1. Attach process tracing before opening a stable Synology Drive directory.
2. Wait for its overlay and the single confirming query to finish.
3. Make no filesystem or Dolphin changes for at least 35 seconds.
4. Require zero later query children during that interval.
5. Continue for five minutes as supporting idle evidence.
6. Record `pidstat` data, but do not use a whole-Dolphin CPU percentage as the hard gate.
7. Modify a harmless visible synced file.
8. Require Dolphin demand to discover `Syncing`.
9. Require syncing-only polling to reach `Synced` within two seconds after the helper reports it.

The hard performance gate is zero stable-idle query children. Temperature and total Dolphin CPU remain supporting observations.

## Assumptions and plan reconciliation

- A stable status can remain stale until Dolphin receives a filesystem event or requests its overlay again. This delay is accepted.
- A syncing entry without Dolphin demand expires after 30 seconds. This prevents a stuck helper from restoring permanent CPU use.
- A Synology crash is accepted. SDD-011 does not add stronger crash containment.
- The `20260901T045809` plan record is carried forward for process isolation and nonblocking callbacks.
- The `20260901T141135` record is superseded only for periodic stable refresh and expiry notifications.
- The `20260901T153200` record is carried forward. Local CI and post-1.0 GitHub enforcement remain unchanged.
- The `20260901T163218` record is carried forward. Drive 8.x compatibility and fresh query children remain unchanged.
- [The original status plan](/home/mbeutler/Projects/dolphin-drive/docs/plans/synodrive-status.md:37) is superseded only where it requires polling every recent path or signaling cache expiry.
- [The SDD-003 plan](/home/mbeutler/Projects/dolphin-drive/docs/plans/SDD-003-drive-8x.md) remains otherwise authoritative.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/plans/SDD-011-idle-performance.md -->

Review: preflight completed; the plan includes all four challenged scenarios. Implementation review has not run.

Docs: roadmap, SDD-011 plan, supersession note, and usage updates are planned.
</proposed_plan>