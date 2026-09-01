# Eliminate idle overlay polling for v0.2.0

This ExecPlan is the durable specification for SDD-011. Maintain it according to `/home/mbeutler/.codex/PLANS.md` until review closure.

## Purpose / Big Picture

The Dolphin extension currently queries every recently viewed path once per second. Each query starts a fresh process that loads the private Synology status libraries. Unchanged results also notify Dolphin, which requests the overlay again and keeps the path recent. This loop can consume a constant fraction of one CPU core after synchronization is complete.

After this change, a stable directory creates no query children until Dolphin asks for its overlay again. A recent syncing path continues to poll once per second. Polling stops when the path becomes stable or receives no Dolphin demand for 30 seconds. Version `0.2.0` contains this correction and the completed Synology Drive 8.x support.

## Surprises & Discoveries

- Observation: One hundred warm stream queries used approximately 0.48 seconds of CPU time.
  Evidence: Each query costs approximately 4.8 milliseconds. One query per second is consistent with the reported Dolphin CPU use.
- Observation: An idle `synodrive-status --stdio` wrapper used no measurable CPU during a five-second sample.
  Evidence: Fresh query children, not the persistent wrapper, own the material idle cost.
- Observation: A same-process helper remained on `syncing` for 180 seconds after the Synology client showed completion.
  Evidence: `.agent/test-results/synodrive-status.md` records that fresh query children observed the correct transition.
- Observation: A held response can arrive after its cache entry expires.
  Evidence: The mixed-cache scenario showed that an unguarded response can recreate an inactive entry.

## Decision Log

- Decision: Keep the existing persistent `synodrive-status --stdio` wrapper and one fresh child for each private-library query.
  Rationale: Fresh children preserve correct live status. Removing stable polling removes their idle cost.
- Decision: Do not load the private Synology libraries directly into Dolphin.
  Rationale: Direct loading gives no stable-idle benefit and can block Dolphin's main thread.
- Decision: Query stable paths only after Dolphin calls `getOverlays()`.
  Rationale: This rule gives a deterministic zero-query idle state.
- Decision: Poll only recent `Syncing` entries once per second.
  Rationale: A visible transition stays responsive without polling stable entries.
- Decision: Expire every entry after 30 seconds without Dolphin demand.
  Rationale: A stuck `Syncing` result must not restore permanent CPU use.
- Decision: Remove expired entries without an overlay notification.
  Rationale: An expiry notification causes Dolphin to call `getOverlays()` and restarts work.
- Decision: Discard a response when its demand timestamp expired before the response arrived.
  Rationale: A late response must not recreate an inactive cache entry or restart polling.
- Decision: Emit `overlaysChanged` only when the mapped overlay list changes.
  Rationale: `Unknown` and `Unsupported` both have an empty overlay, and unchanged overlays need no repaint.
- Decision: Release the completed SDD-003 and SDD-011 work as version `0.2.0`.
  Rationale: The published `v0.1.0` tag consumed version `0.1.0`. Pre-1.0 feature work increments the minor version.
- Decision: Defer persistent private-library loading to SDD-012.
  Rationale: A separate GLib-aware worker needs a live-transition proof before adoption.

## Outcomes & Retrospective

Stable directories now stop all query-child creation after their initial demand settles. Syncing-only polling keeps visible transitions responsive. The response gate also prevents late helper results from recreating expired work. Fresh query children remain the smallest correct library-lifecycle design.

## Context and Orientation

`src/synodrive_overlay_plugin.cpp` implements the KDE overlay plugin. Dolphin calls `getOverlays()` on its main thread. The method returns cached overlays and uses `StatusProvider` for asynchronous updates.

`src/status_provider.cpp` owns one `QProcess` wrapper, the request queue, the status cache, access timestamps, and the refresh timer. The wrapper runs `synodrive-status --stdio`. It accepts absolute paths as NUL-terminated frames.

`src/synodrive-status.cpp` owns the stream protocol and forks one fresh child for each status query. The child loads the Synology libraries, reads the status, and exits. SDD-011 does not change this file except for version-independent regression coverage if required.

`tests/test_plugin.cpp` uses a fake status helper. The helper records requested paths and process identities. Production timing is one second for refresh and 30 seconds for recent access. The test build uses shorter values.

The task branch is `task/SDD-011-idle-performance`. Its base is the reviewed SDD-003 checkpoint `3e38b68`. The source version remains `0.1.0` until this task updates `CMakeLists.txt` to `0.2.0`.

## Product Boundary

The product boundary follows `/home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md` and `/home/mbeutler/.agents/skills/design-preflight/references/supported-model.md`.

Supported normal use includes local file URLs, all six mapped Synology statuses, multiple cached paths, held responses, wrapper exit, and normal file-system events from Dolphin. The project controls the cache, timer, process queue, overlay mapping, tests, documentation, and release version.

The task excludes direct library loading in Dolphin, a new worker protocol, a new dependency, telemetry, package work, context-menu work, and GitHub-hosted CI. Whole-Dolphin CPU percentages are supporting evidence because other plugins and hardware affect them. Zero stable-idle query children is the controlled acceptance result.

The user accepts delayed detection when a stable status changes without a file-system event or a later Dolphin overlay request. The user also accepts that a Synology fault can require a Dolphin restart.

## Prior Plan Reconciliation

The following immutable records constrain this task:

- `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md` is carried for process isolation and nonblocking Dolphin callbacks.
- `.agent/plan-history/plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md` is superseded only for periodic stable refresh and expiry notifications.
- `.agent/plan-history/plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md` is carried for local CI and post-1.0 GitHub enforcement.
- `.agent/plan-history/plan-summary.20260901T163218079291Z.30d64bb2faf269861f88faf3432e8a0268edc0e84e7653fe1d8a362799077f87.md` is carried for Drive 8.x compatibility and fresh query children.

Record: `.agent/plan-history/plan-summary.20260901T181723354265Z.e5524360a445bc95804a35781f50d2b095f673ff5d690f019e6d493a8279d1fe.md`

Status: superseded

Scope: Release identity and publication statements only.

Reason: The `v0.1.0` tag is already published and consumed.

Replacement: Use source version `0.2.0` and Git tag `v0.2.0` for the next release.

The replacement plan is `.agent/plan-history/plan-summary.20260901T182134563661Z.8250eabe2bf83218118a65973b2f84697e7f8200987d04bd15768e6ed0fbb3b0.md`. All non-version decisions from the superseded record remain carried.

## Scenario Proof

The scenario proof follows `/home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md`, `owner-composition.md`, and `full-set-results.md`.

For stable demand, resolve each stable raw status and wait through multiple refresh intervals. The fake helper log must not grow. One later `getOverlays()` call must add one deduplicated request.

For mixed cache state, cache stable path B and syncing path A. Hold a later A response across timer intervals. The provider must not duplicate A or query B. Poll responses must not renew A's access. Both entries must expire silently after the inactivity boundary.

For transition closure, a recent syncing path must poll until it returns a stable status. The provider must stop status queries after that response.

For notification equality, identical raw statuses produce no signal. `Unknown` and `Unsupported` changes produce no KDE signal because both map to an empty overlay. Each visible mapping change produces one KDE signal.

For failures, exercise both an `error` frame and an unexpected wrapper exit. Every affected active or queued path must become uncached. A visible cached overlay must change to empty. A later demand must restart the wrapper and recover the overlay.

For expiry, exercise `Synced`, `Unknown`, and `Unsupported`. Expiry must produce no helper request or KDE signal. A direct provider lookup after expiry must return no cached value.

For process isolation, keep one wrapper process and require a distinct query child for each request. Preserve the existing live transition from `synced` to `syncing` to `synced`.

## Plan of Work

First, revise `StatusProvider` in `src/status_provider.h` and `src/status_provider.cpp`. Keep the separate access timestamp map because it already distinguishes Dolphin demand from timer refresh. Make the timer dynamic. It uses the short refresh interval while a recent cached entry is `Syncing`, the recent-cache interval while only stable entries remain, and no interval when the cache is empty.

The refresh function removes inactive entries without a status signal. It requests only recent entries whose cached raw status is `Syncing`. It never requests stable entries. A query response does not update the access timestamp.

Change the internal `statusChanged` signal to include the path, previous raw status, and current raw status. Use `Unknown` as the no-cache visual sentinel. Emit the signal only when the raw value changes. On an error frame or process exit, invalidate all affected active and queued cache entries. Emit previous-to-`Unknown` only for a changed raw value.

Then revise `SynodriveOverlayPlugin::getOverlays()` in `src/synodrive_overlay_plugin.cpp`. For each local URL, read and return the cached overlay without waiting. Always request one deduplicated asynchronous refresh, including for a cache hit. Map both raw values in the provider signal and emit `overlaysChanged` only when the two overlay lists differ.

Adapt `tests/test_plugin.cpp` and the fake helper only where the accepted scenarios require it. Reuse the existing timing constants, request log, control file, wrapper PID, and query PID fixtures. Keep the test suite focused on observable process requests, cache state, and KDE signals.

Update `CMakeLists.txt` to version `0.2.0`. Update the smallest existing user and architecture documents that describe refresh behavior. Add a supersession note to `docs/plans/synodrive-status.md` without rewriting its historical content. Keep `docs/roadmap.md` as the only lifecycle record.

## Concrete Steps

Operate from `/home/mbeutler/Projects/dolphin-drive`.

Build and run focused tests with:

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --parallel
    TMPDIR=$PWD/build/tmp ctest --test-dir build --output-on-failure

Run the distribution gate with:

    ./ci/run

Run the live transition test with the installed Synology client:

    python3 tests/live_transition.py ./build/synodrive-status <synced-test-path>

For the desktop performance check, attach process tracing before Dolphin opens a stable Drive directory. Wait for the initial overlay and its confirmation query. Then make no file-system or Dolphin changes for at least 35 seconds. The wrapper must create no later query child during that interval.

## Validation and Acceptance

The focused plugin tests must prove stable idle, syncing-only polling, mixed-cache isolation, visible-notification equality, silent expiry, and both failure paths. The complete CTest suite must pass on the host. The local Ubuntu and Fedora distribution gate must pass.

The source version must equal `0.2.0`. The existing `v0.1.0` tag must remain unchanged. No `v0.2.0` tag exists until publication is separately authorized and every release gate passes.

The installed desktop must create zero query children after a stable overlay settles. A harmless visible file change must reopen status demand. If the helper reports `Syncing`, the provider must poll only that recent path. It must show `Synced` within two seconds after the helper reports the stable result.

## Idempotence and Recovery

Build and test commands are repeatable. Cache and timer tests use temporary files. They do not modify installed Synology files.

If a build, test, or review command fails, preserve its output in `.agent/test-results/SDD-011-idle-performance.md`. Correct only an accepted in-scope defect. Do not reset, stash, or remove unrelated files.

## Artifacts and Notes

Keep detailed measurements, validation output, review output, and checkpoint data in `.agent/test-results/SDD-011-idle-performance.md`. This ignored file is not part of the release commit.

## Interfaces and Dependencies

No installed public interface changes. The `synodrive-status` one-shot and `--stdio` protocols remain unchanged. The supported Drive major, overlay ABI, library names, symbols, structure, and six raw status values remain unchanged.

The internal provider signal becomes:

    void statusChanged(const QString& path, SyncStatus previous, SyncStatus current);

The implementation uses the existing Qt, KDE, C++ standard-library, and POSIX dependencies. It adds no dependency.

## Plan Deviations

The earlier SDD-011 plan named `0.1.0` as an unpublished release. The user corrected this fact before implementation. This ExecPlan uses `0.2.0` and preserves the published `v0.1.0` release.
