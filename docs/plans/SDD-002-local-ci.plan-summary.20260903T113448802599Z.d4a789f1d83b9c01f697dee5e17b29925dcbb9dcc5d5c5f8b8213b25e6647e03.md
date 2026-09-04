<proposed_plan>
# SDD-012 — Persistent GLib-Aware Status Worker

## Summary

Close SDD-013 with: “Current Windows behavior matches Linux; no follow-on action is required.” Then make SDD-012 the sole active task and create `docs/plans/SDD-012-persistent-status-worker.md`.

Replace per-query children with one persistent worker child under the existing `synodrive-status --stdio` supervisor. Adopt it only if it remains correct and uses less CPU than released v0.4.0. Successful adoption becomes version 0.5.0. Do not publish, tag, push, or release.

## Implementation

- Preserve the public one-shot CLI, NUL-framed stream protocol, ten-second query deadline, StatusProvider behavior, overlay mappings, and SDD-011 polling policy.
- Keep the existing stream process as supervisor. Add one lazy `WorkerSession` that:
  - Forks one persistent child and communicates through two private pipes.
  - Handles partial writes and reads, including a valid frame larger than pipe capacity.
  - Kills, reaps, and resets the worker after timeout, exit, malformed output, or query failure.
  - Starts a replacement only for a later valid request.
  - Preserves `PR_SET_PDEATHSIG` and the parent-race check so killing the supervisor cannot orphan the worker.
- In the child:
  - Retain internal-major-4, ABI-15, symbol, structure, and status-range checks.
  - Select only the fixed Ubuntu or Fedora Nautilus-extension path.
  - Initialize the module with `nautilus_module_initialize(nullptr)`, require its single GType, and construct one GObject. Static registration with a null `GTypeModule` is supported by GLib 2.56 and later. ([register type](https://docs.gtk.org/gobject/method.TypeModule.register_type.html), [add interface](https://docs.gtk.org/gobject/method.TypeModule.add_interface.html))
  - Run the global default context with [`g_unix_fd_add`](https://docs.gtk.org/glib-unix/func.fd_add.html) and [`g_main_loop_run`](https://docs.gtk.org/glib/method.MainLoop.run.html).
  - Keep the wrapper, helper, GObject, and DSO handles resident for the process lifetime. Do not unload or reinitialize them.
  - Preserve `PrepareCacheTable()` before every `GetInfo()` call unless later authoritative evidence proves a weaker requirement.
- Link the status executable to the existing GLib/GObject platform libraries. Do not add a third-party dependency, public option, worker pool, private-protocol reimplementation, or runtime fallback.
- Update the status architecture documentation and generated package dependencies. Advance to 0.5.0 and complete SDD-012 only after every adoption gate passes.

## Validation and Adoption Gate

- Extend the fake module to prove one GObject initialization, one worker PID, and default-context dispatch. Wait for an explicit callback marker before asserting the changed status.
- Cover exact stream behavior:
  - Empty input, one frame, split input, empty and oversized frames, and partial EOF.
  - Ordered `A`, duplicate `A`, and distinguishable `B`.
  - Valid `A`, invalid frame, valid `B`, with the same worker retained.
  - One valid frame larger than pipe capacity.
- Prove timeout, crash, and teardown recovery:
  - A held request returns `error` within the deadline, the worker disappears, and a later request uses a new PID.
  - Provider destruction kills the supervisor and leaves neither supervisor nor worker alive.
  - Active and queued paths are invalidated once and recover only after later Dolphin demand.
- Preserve all SDD-011 tests for nonblocking overlays, stable-idle behavior, syncing-only polling, queue completeness, expiry, and notification equality.
- Run the real transition proof rapidly for correctness. Initialize the worker before creating a later distinct test file, then require `synced → syncing → synced` from the same worker.
- Compare released v0.4.0 and the candidate with at least three paired live trials at the production one-second syncing cadence. Hold the sync root, write workload, observation limits, and query cadence constant. Measure GNU `time` user-plus-system CPU for each complete reaped process tree.
- Adopt only if every candidate trial stays current and its median CPU is strictly below the v0.4.0 median. Also verify that stable idle remains at zero queries and private Synology libraries appear only in the worker.
- Run focused host tests first. After documentation, trace closure, and read-only correctness review are green, the coordinator alone runs `./ci/run`. Its Ubuntu and Fedora containers must exercise the release-configured binary through their literal extension paths. Reviewers never run CI.
- If a correctness or CPU adoption gate fails, restore the v0.4.0 production path and version, retain the evidence, leave SDD-012 active, and stop for user direction.

## Prior Plan Reconciliation

- Carry the existing process-isolation, public-interface, Drive 8.x, SDD-011 polling, local-CI, packaging, and publication decisions from records `51b36e3d`, `58f1bf2f`, `30d64bb2`, `8250eabe`, `2649b14c`, `03aa9cd4`, and `81c9393a`.
- Record `e5524360` as already superseded by `8250eabe`.
- If adoption passes, supersede only the fresh-child-per-query decision in records `51b36e3d`, `30d64bb2`, and `8250eabe`; all other decisions remain carried.
- Supersede only the Windows follow-up portion of record `455303d1` with the user’s SDD-013 result. Create no new Linux task.

## Assumptions

- Released v0.4.0 is the performance baseline; adopted pre-1.0 feature work becomes 0.5.0.
- Permanent private-library loading is acceptable inside the isolated worker. A worker crash cannot crash Dolphin, and the supervisor supplies recovery.
- Supported scope remains x86_64 KDE 6, Synology Drive public 8.x/internal major 4, ABI 15, Ubuntu 26.04, and Fedora 44.
- No publication operation is authorized by this task.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/SDD-012-persistent-status-worker.md -->
<!-- cpk-plan-spec: docs/plans/SDD-011-idle-performance.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
<!-- cpk-plan-spec: docs/plans/SDD-002-local-ci.md -->
<!-- cpk-plan-spec: docs/plans/SDD-004-packaging.md -->
<!-- cpk-plan-spec: docs/plans/SDD-005-package-lifecycle.md -->
<!-- cpk-plan-spec: docs/plans/SDD-007-context-menu.md -->
<!-- cpk-plan-spec: docs/plans/SDD-008-context-menu-actions.md -->
<!-- cpk-plan-spec: docs/plans/SDD-015-tray-patch.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
</proposed_plan>