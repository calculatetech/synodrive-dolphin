# Synology Drive status overlays for Dolphin

This ExecPlan is a living document. Keep it current until implementation and review are complete.

## Purpose

Build a local, version-gated bridge from the installed Synology Drive status helper to Dolphin. A command-line program proves the private helper contract in a separate process. After that proof passes, a KF6 overlay plugin uses the command-line program asynchronously. Dolphin never loads the private Synology library.

The visible result is a Dolphin overlay for verified Synology states. Unknown and unsupported states have no overlay. The project does not redistribute Synology software.

## Prior plan reconciliation

Record: `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`

Status: carried

Carried decisions: use the installed `plugin-cb-4.so`; expose a version-gated `synodrive-status` process; verify raw values 0 through 5 only; prove a real synced-to-syncing-to-synced transition; then add a cached asynchronous KF6 plugin; keep the private library out of Dolphin; do not redistribute Synology binaries.

Current user decision: on 2026-09-01, the user stated that the accepted plan supersedes `docs/engineering-task.md`. This resolves the old document's no-plugin scope limit. The plugin composes with the CLI after the live transition gate.

## Supported model

The supported system is one local Linux desktop user with Synology Drive `8.0.2-17889`, icon-overlay ABI directory `15`, installed `libnautilus-extension4`, an active Synology daemon, Dolphin/KF6, and normal local file paths representable as UTF-8 without a NUL byte. One Dolphin plugin instance owns one persistent `synodrive-status --stdio` process. Development acceptance can load an exact extracted distro copy through `LD_LIBRARY_PATH` when system installation needs unavailable administrator authentication; the installed product does not contain or bypass that runtime dependency.

The implementation does not support other Synology versions, direct protocol or database access, multiple users, deliberate conflicts, public packaging, a context menu, or a worker pool. A fixture proves daemon-unavailable behavior without stopping the user's live daemon. Error and conflict overlays remain excluded because their raw values are not verified.

## Behavior contract

`synodrive-status ABSOLUTE_PATH` writes exactly one of `unknown`, `synced`, `syncing`, `unsupported`, `read-only`, or `no-permission` plus a newline and exits 0. Usage errors write no stdout, write a diagnostic to stderr, and exit 2. An unsupported package or ABI, missing library or symbol, helper exception, or raw value outside 0 through 5 writes no stdout, writes a diagnostic to stderr, and exits 1.

`synodrive-status --stdio` reads UTF-8 absolute paths terminated by NUL. It writes one NUL-terminated response for each complete request, in order. A success response is one of the six status names. A query failure is `error`. Empty requests and requests larger than 1 MiB return `error`. Partial input is retained until NUL; coalesced frames are processed separately. EOF with a partial frame discards that frame and exits. The persistent wrapper performs each private-library query in a fresh short-lived child and returns its result. This must pass repeated live queries and a live same-wrapper transition before plugin work begins.

The status provider owns the helper process, byte buffer, one active path, a deduplicated FIFO queue, cache, refresh timer, and terminal signal. `getOverlays` never waits. A cache miss queues a local path and returns no overlay. A non-local URL does not start the process or alter provider state. Duplicate active or queued paths are not added twice; later distinct paths are preserved.

Successful results are cached. The plugin maps synced to `emblem-default`, syncing to `emblem-synchronizing`, read-only to `emblem-readonly`, and no-permission to `emblem-unreadable`. Unknown and unsupported map to no overlay. The target theme must resolve every nonempty name before acceptance; otherwise this plan will add verified local fallback assets.

The refresh timer runs every second. Any cached path requested by Dolphin during the last 30 seconds is refreshed at most once per timer pass. Entries not requested for 30 seconds are removed and emit `overlaysChanged` if they held a nonempty overlay. Thus, a visible and repeatedly requested file updates within two seconds after the helper exposes a new state. This is a polling ceiling, not a promise for files Dolphin no longer asks about.

On an `error` frame, malformed or oversized response, helper exit, or startup failure, the provider invalidates the active path and every queued path, emits a terminal change for any removed nonempty overlay, clears the active gate, and stops the process without waiting. The next local request starts a new wrapper. Destruction stops the timer, disconnects signals, and terminates then kills the child without a blocking wait; the QObject-owned process prevents an orphan.

Only the CLI process may load the private helper. It verifies installed package version `8.0.2-17889`, resolves the `current` symlink to basename `15`, requires `libnautilus-extension.so.4`, loads `lib/plugin-cb-4.so` locally, resolves the two observed mangled symbols, catches exceptions, and accepts only raw values 0 through 5. It does not write Synology state. The live acceptance file is unique, checked absent before creation, and removed in a guaranteed cleanup path.

## Evidence and scenarios

1. CLI fixture tests reject missing, relative, and extra arguments; accept absolute paths with spaces and a newline; map all six raw values; reject an invalid raw value; and cover helper exceptions, daemon-unavailable behavior, missing runtime, missing symbol, wrong package version, and wrong ABI target.
2. Stream tests split one request across writes, coalesce two requests, cover empty and oversized frames, send `A`, duplicate `A`, then `B`, and close with a partial frame. Each distinct complete path gets one ordered terminal result.
3. Live evidence records the package version, timestamp, exact commands, sanitized active-root path, outside-root path, outputs, and exits. The root is synced and the outside path is unknown.
4. The hard gate keeps one real `--stdio` wrapper alive while one unique file is observed as synced, then syncing, then synced. Each request uses a fresh query child because live evidence showed that repeated `dlopen` and `dlclose` in the wrapper retained a stale status after the Synology UI had settled. The gate also performs repeated queries, checks the wrapper remains alive, and confirms the test file is absent after cleanup. If raw status 2 is not observed, plugin implementation stops.
5. A trace permits the helper's AF_UNIX `ui.sock` traffic, normal stdio, and SQLite's read/write opens of `-wal` and `-shm` coordination files. It requires read-only opens of the main Synology databases and rejects Internet-family endpoints and durable Synology-state mutations.
6. Provider tests hold a fixture response until `getOverlays` has returned, then release it and assert the later signal. They cover non-local URLs, unknown states, dynamic synced-to-syncing-to-synced refresh, `A`/duplicate `A`/`B` queue preservation, cached-overlay invalidation, helper failure and restart, malformed frames, and destruction with a response pending.
7. Static ELF checks and live process maps show no `plugin-cb-4.so` dependency or mapping in the plugin or Dolphin. The wrapper maps it only during a query and unloads it afterward.
8. Complete build and install manifests contain only this project's files. No Synology binary is present.

## Implementation sequence

Work on branch `task/synodrive-status`. The repository has no base commit and no remote, so each semantic checkpoint becomes a local commit after review.

First, add the C++17 CLI, fake helper, and small integration test. Configure and build with CMake/Ninja. Install only `libnautilus-extension4` if the live runtime dependency is missing; do not install Nautilus. Run fixture tests, live root/outside checks, the read-only trace, repeated live queries, and the hard live transition gate. Record the evidence in `docs/SYNODRIVE_DOLPHIN_RECON.md` and the ignored `.agent/test-results/synodrive-status.md`. Close the checkpoint trace, run native Codex review on a synthetic commit range, correct findings, and commit the reviewed CLI checkpoint.

Second, install only the required KF6 development packages, add the smallest `KOverlayIconPlugin` and provider, and test its framing, queue, cache, recovery, and lifecycle behavior. Verify theme icons, ELF dependencies, process maps, and complete artifact manifests. Update the same report, close the checkpoint trace, review a synthetic commit range, correct findings, and commit the plugin checkpoint.

Finally, run the complete test and acceptance set from a clean build, finalize documentation in Simple English, close a fresh full-candidate trace, and run one final native Codex review of the complete candidate.

## Build and validation commands

From the repository root:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build --output-on-failure

The CLI test scripts and live acceptance commands will be documented after their filenames exist. Install testing uses a temporary `DESTDIR`; it never copies files into Synology directories.

## Research record

Official KF6 documentation defines `KOverlayIconPlugin`, its nonblocking `getOverlays` callback model, `overlaysChanged`, and the `kf6/overlayicon` install namespace: <https://api.kde.org/koverlayiconplugin.html>.

Local inspection on 2026-09-01 established Synology Drive `8.0.2-17889`, ABI directory `15`, the helper paths, the two-int `IconOverlayInfo` layout, raw values 0 through 5, private database reads, `ui.sock` use, and process-global path caching. An APT simulation established that `libnautilus-extension4` can be installed without the Nautilus application. These are local compatibility facts, not a public Synology API. Public release requires legal review.

No arXiv research gate applies. This project is a narrow desktop integration around existing APIs, not a new algorithm, architecture, or protocol.

## Revision note

The preflight challenge was merged before implementation. It corrected the exact package-version gate, stream grammar, cache and failure terminals, full-set queue proof, teardown proof, daemon-unavailable fixture, live persistent-helper proof, and the trace oracle for local Unix-socket traffic. The first live persistent-wrapper run observed a stale `syncing` value after the Synology UI reported completion. This rejected the proposed per-request unload design and selected a fresh query child behind the same persistent stream wrapper.
