# Prove context-menu integration

This ExecPlan is the durable specification for SDD-007. Maintain it according to `/home/mbeutler/.codex/PLANS.md` until review closure.

## Purpose / Big Picture

Synology Drive includes two useful Nautilus menu actions: “Get link” and “Browse previous versions.” Synology does not provide a public Dolphin API for these actions.

This task determines whether a separate process can use the installed private helper. The process must list both actions for one synced file. It must also invoke each real returned action through Synology's local UI channel.

Success permits SDD-008. Failure declines SDD-008 with evidence and does not block version 1.0.

## Study Disposition

SDD-007 succeeded as a viability study. Its live ABI, listing, action, timing, and isolation evidence permits SDD-008.

The study does not deliver the diagnostic probe as a product. SDD-008 owns production code, package changes, CI, and correctness review.

## Surprises & Discoveries

- Observation: `plugin-cb-4.so` contains DWARF data and unstripped private symbols.
  Evidence: GDB reports the C entry point and the C++ handler types from Synology's source paths.
- Observation: `cstn_private_get_file_item` calls only `nautilus_file_info_get_uri` on each selected Nautilus object.
  Evidence: The function disassembly contains one Nautilus file-information call before Synology's deciders run.
- Observation: The private callback does not use the provider object for either required handler.
  Evidence: `ContextMenuCallback` rejects a null provider but does not dereference a non-null provider before these handler calls.
- Observation: Both handlers require one path, resolve it with `realpath`, send a typed request through `PStream::Send`, and return.
  Evidence: The handler disassembly shows the one-record check, `OpenChannel`, request construction, and send call.

## Decision Log

- Decision: Keep the prototype outside Dolphin and do not install or package it.
  Rationale: SDD-007 proves the private interface. SDD-008 owns the later Dolphin plugin.
- Decision: Use the installed user helper at `~/.SynologyDrive/SynologyDrive.app/icon-overlay/current/lib/plugin-cb-4.so`.
  Rationale: Synology's Nautilus wrapper uses this path. The project must not redistribute the library.
- Decision: Interpose only `nautilus_file_info_get_uri` in the diagnostic executable.
  Rationale: The installed helper needs no other selection method. This avoids a fake GObject implementation.
- Decision: Require one existing synced regular file to expose both exact enabled actions.
  Rationale: One file gives the strongest useful proof for the later Dolphin action plugin.
- Decision: Treat an empty returned menu as a successful but ineligible list result.
  Rationale: Normal decider absence is different from a loader, symbol, or process failure.
- Decision: Measure the complete list process from launch through normal exit.
  Rationale: This is the delay that a later Dolphin integration must hide or bound.
- Decision: Require every one of 20 sequential list runs to finish in less than 250 ms.
  Rationale: A maximum result detects a slow first run and avoids a favorable average.
- Decision: Require three normal typed UI-channel sends for each action.
  Rationale: One successful send does not establish basic repeatability.
- Decision: Use one visible Synology response for each action as non-gating identity evidence.
  Rationale: Opaque UI scheduling must not override a proved matching channel send.
- Decision: Keep all returned menu objects alive until synchronous activation returns.
  Rationale: The callback reads private action data stored on the selected item.
- Decision: Record experimental crashes, but apply reliability gates to the finished candidate.
  Rationale: A corrected experiment can still prove feasibility. A failed final sample cannot be erased by a retry.

## Outcomes & Retrospective

The private menu interface is viable for the recorded Drive 8.0.2 installation and ABI 15. One synced file returned both required actions. Twenty list processes finished in 6.106 ms through 9.070 ms.

The real callbacks sent `share_link` and `list_version` frames to `~/.SynologyDrive/ui.sock`. Three consecutive processes sent each action and exited normally. The Dolphin process did not load the helper.

Repeated action validation forced Synology windows to the foreground. One window did not close normally, so the user authorized a forced stop of `cloud-drive-ui`. SDD-008 must run actions only after a direct user selection. Automated tests must use a fake UI channel.

## Context and Orientation

`docs/roadmap.md` owns task status and the SDD-008 gate. `docs/SYNODRIVE_DOLPHIN_RECON.md` owns the installed private-interface record.

The existing Dolphin module is `synodrive-overlay.so`. It must remain free of private Synology dependencies. The existing `synodrive-status` executable is unrelated to the context-menu protocol.

The installed Synology Drive package is version `8.0.2-17889` on AMD64. Its internal client version is `4.0.2-17889`. The current overlay ABI directory is `15`.

The private menu entry point is:

    GList *cstn_private_get_file_item(NautilusMenuProvider *, GList *);

It returns a top-level Nautilus item that owns a submenu. Each child item stores private action data. The `activate` signal calls `ContextMenuCallback`, which selects the matching Synology handler.

## Product Boundary

Supported normal use is one existing absolute local file or directory. The success fixture is one synced regular file that returns both required actions. A directory run is informative and does not control the final result.

The diagnostic process owns input checks, helper loading, URI conversion, menu traversal, exact action selection, activation, timing, output, cleanup, and exit status. Synology owns its helper, deciders, menu data, UI channel, and visible response.

The task excludes Dolphin action code, a persistent process, multiple selections, remote URIs, non-English labels, other Drive versions, new package dependencies, packaging, publication, and Synology binary redistribution.

## Prior Plan Reconciliation

The immutable roadmap plan from 2026-09-01 defines SDD-007 as a bounded prototype. It requires both actions, one local selection, a separate process, a 250 ms result, and private ABI evidence.

Later plan records preserve process isolation, local CI, Drive 8.x policy, version `0.3.0`, package work, and the post-version-1.0 hosted CI policy. SDD-007 does not change those results.

The user clarified that SDD-007 is a viability study. This decision supersedes the CI, package, review, and standalone-prototype delivery gates in this document. The recorded live evidence controls the study result.

## Scenario Proof

### B1 — Process isolation

Run the diagnostic process while Dolphin exists. Hold the process after `dlopen` and inspect both process maps. Inspect the Dolphin module dependencies.

The Synology helper must appear only in the diagnostic process. The Dolphin process and module must remain free of the helper.

### B2 — Input and normal empty results

Reject a wrong argument count, a relative path, a missing path, and an unknown action before helper loading. Each case must exit 2.

Run one existing synced file, one existing synced directory, and one outside-root path through the real helper. A normal empty menu must exit 0 and print no item.

### B3 — Installed ABI

Record the package version, internal version, architecture, ABI directory, helper real path, SHA-256 value, required symbols, URI seam, menu APIs, callback mapping, and handler send behavior.

The executable must stop with a nonzero result if `dlopen` or a required `dlsym` call fails. It must have no fallback helper path.

### B4 — Complete menu traversal

Use the same traversal function for the live result and a deterministic self-check. The self-check must cover an empty menu, one item, a disabled item, an earlier prefix, and a later exact target.

The live synced-file result must contain both exact enabled labels. The executable must preserve returned order and must not synthesize labels.

### B5 — Get link

Select the real enabled “Get link” item. Keep the selection and all menu objects alive through activation.

At least one debugger run must reach `ShareLinkHandler::Handle` and `PStream::Send`. The typed request must contain the canonical selected path. Three consecutive runs must send the matching request and exit normally.

### B6 — Browse previous versions

Apply the B5 path to “Browse previous versions” and `BrowseVersionHandler::Handle`. The same ownership, typed request, sample, and exit rules apply.

### B7 — Listing latency

Measure external monotonic wall time from process launch through successful list exit. Run 20 sequential trials without dropping operating-system caches.

Every trial must return both labels in less than 250 ms. Record action times as non-gating diagnostic data.

### B8 — Disposition

Fail SDD-007 if a required action is missing or a final sample crashes, returns nonzero, sends the wrong request, or reaches 250 ms. An unproved terminal also fails the task.

If all gates pass, complete SDD-007 and keep SDD-008 planned. If one gate fails, decline SDD-008 and record the evidence. Version 1.0 remains unblocked.

### B9 — Regression and artifacts

The source tree must contain no Synology binary. Existing install manifests must not contain the diagnostic executable. SDD-008 owns later product and package validation.

## Plan of Work

First, add one small C++ diagnostic source file. It uses only the C++ standard library and `dl`. It resolves public GLib and Nautilus functions from the installed helper dependencies.

Add the diagnostic as a build target, but do not add an install rule. Add one internal self-check mode for complete menu traversal.

Then run the live checks on an existing synced file. Use GDB to bind each returned action to its handler and typed request. Use `strace` to corroborate local transport.

Finally, update the technical report with the exact ABI and results. Update the roadmap with one terminal disposition after review closure.

## Concrete Steps

Build the project and diagnostic process:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build

Run the deterministic traversal check:

    ./build/synodrive-context-menu-probe --self-test

Run the live list:

    ./build/synodrive-context-menu-probe /absolute/synced/file

Run one action per fresh process:

    ./build/synodrive-context-menu-probe --activate get-link /absolute/synced/file
    ./build/synodrive-context-menu-probe --activate browse-versions /absolute/synced/file

Store raw commands and results in `.agent/test-results/SDD-007-context-menu.md`.

## Validation and Acceptance

All B1 through B9 oracles must pass. The final candidate must have 20 successful list runs and three successful sends for each action.

The helper can schedule its visible user interface after the diagnostic process exits. One visible response per action identifies the opaque destination, but it does not control acceptance.

The probe must remain outside every SDD-007 install and package manifest. SDD-008 can promote its proved logic only through the accepted production plan.

## Idempotence and Recovery

Build, list, self-check, and timing commands are repeatable. Each action command can open or focus a Synology window.

If the diagnostic process crashes during development, record the crash and correct the candidate. Do not change or replace the installed Synology files.

If a final reliability sample fails, preserve the result. Do not rerun the sample to erase the failure.

## Artifacts and Notes

Preserve this ExecPlan, the technical report, and the roadmap result. Do not deliver the study probe as a standalone installed tool. Do not commit raw debugger or system-trace files.

## Interfaces and Dependencies

The diagnostic executable has no installed interface. It links only to `dl` and uses run-time symbols from the installed helper dependencies.

The task adds no package dependency. It does not change the existing Dolphin plugin or status protocol.

## Plan Deviations

- The installed Nautilus item uses the `menu` property for its submenu. The first self-check used the incorrect name `submenu` and failed before a live helper call. The corrected self-check passes.
- The local CI build images did not contain `libnautilus-extension.so.4`. The offline traversal check exposed this gap. The corrected images install the existing product runtime package.
- Repeated live action samples forced Synology windows to the foreground. The user stopped further action calls after the required evidence was complete.
