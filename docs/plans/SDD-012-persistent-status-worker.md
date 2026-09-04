# Prove a persistent GLib-aware status worker

This ExecPlan is a living specification until review closure. Maintain it in accordance with `/home/mbeutler/.codex/PLANS.md`.

## Purpose / Big Picture

The status bridge currently starts a new query process for each overlay refresh. This design stays current, but it repeats process and library initialization during active synchronization.

SDD-012 keeps one private-library worker alive behind the existing `synodrive-status --stdio` process. The worker runs Synology's extension lifecycle and the GLib default context. Dolphin remains asynchronous and never loads a private Synology library.

Adopt the worker only if one live worker stays current and uses less CPU than released version 0.4.0. Successful adoption becomes version 0.5.0. This task does not publish, tag, push, or release the candidate.

## Surprises & Discoveries

- Observation: Repeated `PrepareCacheTable()` and `GetIconOverlayInfoByPath()` calls do not keep a same-process cache current.
  Evidence: The earlier helper stayed at `syncing` for 180 seconds after the Synology client became stable.
- Observation: Synology's Nautilus extension object starts the update threads that feed the status cache.
  Evidence: Local symbol and disassembly inspection found `cstn_private_initialize`, three worker threads, and `g_idle_add` callbacks.
- Observation: The existing public stream process can supervise one persistent child.
  Evidence: This arrangement preserves the current timeout and recovery boundary without a new Dolphin timer.
- Observation: The GLib-aware worker still remained on `syncing` after Synology completed the transfer.
  Evidence: The live adoption test observed no `synced` result during its 120-second completion window. It removed its test file successfully.

## Decision Log

- Decision: Keep the public `synodrive-status --stdio` process as a supervisor.
  Rationale: The supervisor already owns public framing and the ten-second query deadline.
- Decision: Start one private worker lazily for the first valid stream request.
  Rationale: Invalid input does not need a Synology process or library mapping.
- Decision: Keep `PrepareCacheTable()` before each `GetIconOverlayInfoByPath()` call.
  Rationale: This preserves the verified private helper call contract while lifecycle dispatch is added.
- Decision: Keep the private libraries mapped until the worker exits.
  Rationale: Static GLib type registration keeps callbacks into the loaded module. Process isolation contains failures.
- Decision: Use only the known Ubuntu and Fedora module locations in release builds.
  Rationale: Broad filesystem discovery would add unsupported installation layouts.
- Decision: Measure CPU at the production one-second syncing cadence.
  Rationale: Rapid polling proves live correctness but does not represent Dolphin's active-sync workload.
- Decision: Advance to version 0.5.0 only after the live correctness and CPU gates pass.
  Rationale: Version 0.4.0 is published and remains the comparison baseline.

## Outcomes & Retrospective

The persistent worker passed all deterministic tests, but it failed the live-currentness gate. It observed `syncing` and then remained stale for 120 seconds. The released version 0.4.0 runtime path remains in production. The user selected that runtime as the version 1.0.0 base, so SDD-012 is declined.

The CPU comparison did not run because live correctness is an earlier adoption gate. The source version remains 0.4.0. Review, CI, publication, and release did not run. The retained experiment can guide later research, but none of its production or test code is part of version 1.0.0.

## Context and Orientation

`src/synodrive-status.cpp` implements two interfaces. One-shot mode writes one line for one absolute path. Stream mode reads NUL-terminated absolute paths and writes one NUL-terminated status for each complete request.

`src/status_provider.cpp` starts one stream process with `QProcess`. It owns the asynchronous request queue, cache, and refresh policy. `src/synodrive_overlay_plugin.cpp` returns cached overlays without waiting for the process.

The current stream implementation forks a fresh child for every request. The child loads `plugin-cb-4.so`, calls the two verified status functions, and exits. The stream supervisor kills a child after ten seconds.

The new worker is a long-lived child of the same supervisor. A GLib main context is an event dispatcher. Synology schedules cache updates on the process-wide default context, so that context must run for the complete worker lifetime.

The task uses branch `task/SDD-012-persistent-status-worker`. Its base branch is `main` at commit `b055b80cd9f131de5cef86427d0f7d26cfeffec6`. The implementation uses the main checkout as one isolated writable stream.

## Product Boundary

The product boundary follows `/home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md` and `/home/mbeutler/.agents/skills/design-preflight/references/supported-model.md`.

The authoritative source is SDD-012 in `docs/roadmap.md`. The supported system is one x86_64 Linux desktop user with KDE 6, Synology Drive public 8.x, internal major 4, and overlay ABI 15. Ubuntu 26.04 and Fedora 44 are the supported package targets.

The stream supervisor, persistent worker lifecycle, worker recovery, live proof, CPU comparison, and adoption decision compose with this task. The one-shot CLI, provider cache policy, overlay mapping, context menus, package formats, and publication remain opaque.

The task excludes private Synology mappings in Dolphin, the plugin, or the supervisor. It also excludes worker pools, direct database or socket protocol implementations, broad module scans, implicit retry of a failed request, unsupported platforms, and Synology redistribution.

Permanent library loading is intentional inside the worker. The operating system removes all mappings when the worker exits. The supervisor can replace a failed worker without restarting Dolphin.

## Prior Plan Reconciliation

The following records are carried for their process-isolation, public-interface, compatibility, polling, CI, packaging, and publication decisions:

- `.agent/plan-history/plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
- `.agent/plan-history/plan-summary.20260901T153200798151Z.58f1bf2f2ea73fa7042b0e0c253e069e4c5580eb91eb556ee782461f32439b0f.md`
- `.agent/plan-history/plan-summary.20260901T210430122701Z.2649b14c4cd2877a93d28ca5e07cd6fce0e06766442bdb90f40ed697ab1b75d5.md`
- `.agent/plan-history/plan-summary.20260902T032236776229Z.03aa9cd44a8385c3489770bbc8821e3531d52f2d35c9832b603f8ec58ab0b49b.md`
- `.agent/plan-history/plan-summary.20260903T013156964362Z.81c9393a6ff77bd9d7461f5febe470fa712c427ba57f416fff4f9af722423d90.md`

Record: `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`

Status: superseded

Reason: The accepted SDD-012 proof replaces only the fresh-child-per-query implementation after its adoption gates pass.

Replacement: Keep one GLib-aware private worker behind the isolated stream supervisor. Carry all other decisions.

Record: `.agent/plan-history/plan-summary.20260901T163218079291Z.30d64bb2faf269861f88faf3432e8a0268edc0e84e7653fe1d8a362799077f87.md`

Status: superseded

Reason: The accepted SDD-012 proof replaces only the fresh-child-per-query implementation after its adoption gates pass.

Replacement: Keep one GLib-aware private worker and preserve all Drive 8.x compatibility gates.

Record: `.agent/plan-history/plan-summary.20260901T181723354265Z.e5524360a445bc95804a35781f50d2b095f673ff5d690f019e6d493a8279d1fe.md`

Status: superseded

Reason: The published 0.1.0 release invalidated that record's version statement.

Replacement: Record `8250eabe` already selected version 0.2.0 for SDD-011. The current released version is 0.4.0.

Record: `.agent/plan-history/plan-summary.20260901T182134563661Z.8250eabe2bf83218118a65973b2f84697e7f8200987d04bd15768e6ed0fbb3b0.md`

Status: superseded

Reason: The accepted SDD-012 proof replaces only the fresh-child-per-query implementation after its adoption gates pass.

Replacement: Keep one GLib-aware private worker and preserve the SDD-011 demand and polling policy.

Record: `.agent/plan-history/plan-summary.20260902T053027990715Z.455303d1f1a965144aa3e84aac88b9517fc5d07047146d702e4ed915255e5f87.md`

Status: superseded

Reason: The user confirmed that current Windows Get link behavior matches Linux.

Replacement: Close SDD-013 without a Linux follow-on task. Carry all SDD-007 and SDD-008 implementation decisions.

The accepted SDD-012 plan is `.agent/plan-history/plan-summary.20260903T113448802599Z.d4a789f1d83b9c01f697dee5e17b29925dcbb9dcc5d5c5f8b8213b25e6647e03.md`. Its exact sibling is stored beside this ExecPlan.

Record: `.agent/plan-history/plan-summary.20260904T030553255401Z.890479e1a919a22ebb44fbae67c784f36e2c4699a5489d33f0b470aabb1f0208.md`

Status: superseded

Reason: The user selected the released version 0.4.0 runtime as the version 1.0.0 base after the persistent worker failed live currentness.

Replacement: Decline SDD-012, retain its evidence, and make no persistent-worker source or test change for version 1.0.0.

## Scenario Proof

The proof follows `/home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md`, `owner-composition.md`, and `full-set-results.md`.

For persistent isolation, send `A`, duplicate `A`, and distinct `B`. Require three ordered results, one worker PID, one initialization, and no private mapping in Dolphin, the plugin, or the supervisor.

For GLib dispatch, let the fake helper schedule a status update with `g_idle_add`. Wait for its completion marker. Then require the changed result from the same worker.

For framing, cover empty input, one frame, split input, empty and oversized frames, a frame larger than pipe capacity, an invalid frame between valid requests, and partial EOF. Require one exact result for every complete frame.

For timeout recovery, hold one worker request beyond the test deadline. Require one `error`, the absence of the old worker, and a successful later request from a replacement PID.

For teardown, kill the supervisor through `StatusProvider` destruction. Require both the supervisor and worker to disappear without blocking Dolphin.

For live currentness, initialize the worker before a unique file exists. Create the file, then require `synced`, `syncing`, and `synced` from the same worker. Remove the file in all outcomes.

For provider behavior, retain every SDD-011 case. Stable entries create no idle work. Only recent syncing entries poll. Active and queued paths receive failure terminals. Later Dolphin demand restarts work.

For performance, run at least three paired version 0.4.0 and candidate trials. Use the production one-second syncing cadence and the same workload. Require every candidate trial to stay current and its median process-tree CPU to be less than the baseline median.

For compatibility, run focused tests with a test-only exact module path. The final Ubuntu and Fedora container gate uses the release binary and the literal module path for that distribution.

## Plan of Work

### SDD-012-A: Persistent worker and deterministic proof

Change `src/synodrive-status.cpp` and `CMakeLists.txt`. Keep the public stream parser as the supervisor. Replace the per-query child with one `WorkerSession` that owns two pipes and one worker PID.

The supervisor must complete partial nonblocking writes before the ten-second deadline. It must buffer the worker response through NUL. On timeout, EOF, malformed output, or worker exit, it must kill and reap the worker and return `error`.

The child must set `PR_SET_PDEATHSIG` and repeat the parent PID check. It must load the installed Synology Nautilus wrapper and private status helper. It must resolve the module and status symbols before it initializes the module.

Call `nautilus_module_initialize(nullptr)`. Require exactly one nonzero type from `nautilus_module_list_types`, and construct one GObject. Add the request pipe with `g_unix_fd_add` and run the global default loop with `g_main_loop_run`.

Call `PrepareCacheTable()` before each status query. Keep the object and module handles resident. Do not call `dlclose` on a successful worker lifecycle.

Add the smallest fake Nautilus module and extend the fake status helper. The fake must distinguish a loaded library from a dispatched GLib update. Extend `tests/test_cli.py` with the accepted framing, lifecycle, timeout, and replacement cases.

The observable result is one worker identity across valid requests and a new identity after failure. The checkpoint boundary contains the status executable, fake module, CLI tests, and required CMake changes.

### SDD-012-B: Provider composition and live adoption

Run the existing provider tests through the candidate CLI. Add only the teardown identity evidence that the current tests do not expose. Do not change provider behavior.

Update `tests/live_transition.py` so it initializes the worker before it creates the test file. Record the supervisor and worker identities, then prove the complete live transition.

Add the minimum reusable performance driver. It compares the released 0.4.0 binary and candidate at a one-second query cadence. It reports each process-tree CPU value, both medians, and a pass or fail adoption result.

If live currentness or CPU reduction fails, restore the released worker path and version. Retain the evidence in the ignored result, keep SDD-012 active, and stop for user direction.

If both gates pass, remove the fresh-query-child path. Set the project version to 0.5.0. Update the authoritative architecture documents and prepare the complete candidate for review.

## Concrete Steps

Run all commands from `/home/mbeutler/Projects/dolphin-drive`.

Configure and run focused tests:

    cmake -S . -B build -DBUILD_TESTING=ON
    cmake --build build --parallel
    TMPDIR=$PWD/build/tmp ctest --test-dir build --output-on-failure

Run the rapid live correctness proof:

    python3 tests/live_transition.py ./build/synodrive-status <synced-test-path>

Build released version 0.4.0 in a separate temporary source tree. Run the paired performance driver with the baseline binary, candidate binary, and the same sync root.

After the documentation and read-only review gates are green, the coordinator runs:

    ./ci/run

Reviewers do not run this command. GitHub Actions remain disabled before version 1.0.

## Validation and Acceptance

All focused and existing tests must pass. The public CLI bytes and exit codes must remain unchanged.

The rapid live proof must show `synced`, `syncing`, and `synced` from one worker. The test file must be absent after cleanup.

Every candidate performance trial must stay current. The candidate median CPU must be less than the version 0.4.0 median at the one-second cadence.

The private helper must appear only in the persistent worker's process map. Dolphin, the plugin, and the stream supervisor must not map it.

The local Ubuntu and Fedora gate must pass after review. Generated DEB and RPM dependencies must include required GLib libraries and no Synology payload.

## Idempotence and Recovery

Build, fixture, and benchmark commands are repeatable. Every live trial uses a unique non-hidden file and removes it in a guaranteed cleanup path.

A failed worker request returns `error`. The supervisor reaps the worker. A later complete request starts a new worker.

If a core adoption gate fails, do not mark SDD-012 complete. Do not change the published version or create a tag.

## Artifacts and Notes

Keep detailed commands and results in `.agent/test-results/SDD-012.md`. This ignored file is not part of the candidate.

Do not commit Synology binaries, generated packages, temporary source trees, process maps, account data, or generated share links.

## Interfaces and Dependencies

Public interfaces do not change. `synodrive-status ABSOLUTE_PATH` remains line-oriented. `synodrive-status --stdio` remains an ordered NUL-framed stream.

The internal `WorkerSession` owns the worker PID, request descriptor, and response descriptor. It provides one operation that returns a status string or `error` within ten seconds.

The worker uses GLib, GObject, and GLib Unix integration from the target distribution. The project adds no third-party library.

Release builds use these exact module paths:

- Ubuntu: `/usr/lib/nautilus/extensions-4/libnautilus-drive-extension-4.so`
- Fedora: `/usr/lib64/nautilus/extensions-4/libnautilus-drive-extension-4.so`

Test builds can use one exact module override. The release binary must not contain the override name.

The final revision must explain the adopted or rejected outcome and its reason.
