# Ship isolated Synology Drive context actions

This ExecPlan is the durable specification for SDD-008. Maintain it according to `/home/mbeutler/.codex/PLANS.md` until review closure.

## Purpose / Big Picture

Dolphin users can open a `Synology Drive` submenu for one eligible local item. The submenu contains the actions that the installed Synology helper permits.

`Get link` opens Synology's share window. `Browse previous versions` opens Synology's history window. Only an explicit user click activates either action.

Synology's private libraries run in a short-lived helper process. They never load into Dolphin.

## Surprises & Discoveries

- Observation: KF6 calls `KAbstractFileItemActionPlugin::actions()` synchronously.
  Evidence: The installed KF6 header identifies asynchronous actions as a KF7 task.
- Observation: A complete Synology menu query took 6.106 ms through 9.070 ms in 20 fresh processes.
  Evidence: The SDD-007 study used the installed Drive 8.0.2 helper and one synced file.
- Observation: The returned item names are stable internal identifiers on the recorded ABI.
  Evidence: A list-only debugger inspection mapped `NautilusCloudStation::ShareLink` to `Get link` and `NautilusCloudStation::VersionBrowse` to `Browse previous versions`.
- Observation: Synology owns both foreground windows.
  Evidence: The share window contains a copy-link action. The history window can lack a close action.
- Observation: `CMAKE_INSTALL_FULL_LIBEXECDIR` is fixed when CMake configures the plugin.
  Evidence: An install-time `--prefix` override does not update the helper path embedded in the module.

## Decision Log

- Decision: Use one private libexec helper and one native KF6 action plugin.
  Rationale: This keeps Synology code outside Dolphin and uses the supported KDE extension point.
- Decision: Give listing a 200 ms internal deadline and a 250 ms external ceiling.
  Rationale: KF6 is synchronous, and the user accepted the bounded lookup.
- Decision: Match internal item names instead of Synology display labels.
  Rationale: The recorded names select the proved handlers without depending on Synology's locale.
- Decision: Keep project menu labels in English.
  Rationale: Translation catalogs are outside this task.
- Decision: Track activation children asynchronously and report one native plugin error on failure.
  Rationale: A user click must not block Dolphin or retry silently.
- Decision: Parent each activation process to the application.
  Rationale: The selected action must finish after Dolphin closes the context menu.
- Decision: Share compatibility parsing and ABI resolution with `synodrive-status`.
  Rationale: One owner prevents the two helpers from accepting different Drive installations.
- Decision: Do not run the installed real actions again during automated validation.
  Rationale: SDD-007 proved the opaque UI channel, and repeated activation disturbed the desktop.
- Decision: Configure package and system source builds with `CMAKE_INSTALL_PREFIX=/usr`.
  Rationale: The embedded helper path and the installed libexec path must use the same prefix.

## Outcomes & Retrospective

The implementation uses Dolphin's native action-plugin entry point. It keeps Synology's private libraries in a short-lived helper process.

The shared compatibility owner keeps status and action behavior aligned. A configure-time absolute helper path also keeps source installs and distribution packages consistent when they use the documented `/usr` prefix.

## Context and Orientation

`src/synodrive-status.cpp` validates internal Drive major 4 and icon-overlay ABI 15 before it loads Synology's status helper. `src/synodrive_overlay_plugin.cpp` is the existing Dolphin overlay module.

The discarded SDD-007 probe proved the private Nautilus menu entry. It created one Nautilus-style selection, traversed the returned menu, and activated one returned item.

The new `synodrive-action` executable promotes only that proved mechanism. It is an internal program under the package libexec directory.

The new `synodrive-fileitemaction.so` module implements `KAbstractFileItemActionPlugin`. KDE discovers it from `kf6/kfileitemaction` through KPluginFactory metadata.

## Product Boundary

Apply [Scope boundaries](</home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md>).

The roadmap and accepted Plan define the product result. The action plugin, action helper, compatibility slice, and package inventories compose this result.

The installed Synology helper, its deciders, UI socket, and visible windows are opaque. Existing status-query and overlay behavior remain unchanged except for moving their compatibility implementation to the shared owner.

SDD-013 Windows inspection is deferred. It cannot change SDD-008 or block version 1.0.

Supported normal use is one x86_64 Linux user with KDE 6, Dolphin, and Synology Drive public 8.x. The internal client major is 4, and the icon-overlay ABI is 15.

The task excludes direct clipboard access, Windows code, persistent workers, background polling, multiple selections, remote URLs, other Synology actions, retries, Synology redistribution, publication, and hosted CI.

## Interfaces

Install the private program under `${CMAKE_INSTALL_LIBEXECDIR}/synodrive-dolphin/synodrive-action`.

It accepts only:

    synodrive-action --list ABSOLUTE_PATH
    synodrive-action --activate get-link ABSOLUTE_PATH
    synodrive-action --activate browse-versions ABSOLUTE_PATH

Exit 0 means that listing completed, including an empty list, or that activation reached the selected callback. Exit 1 means a compatibility, loader, symbol, menu, or action error. Exit 2 means invalid input.

Successful list stdout is empty or contains these LF-terminated ASCII records:

    get-link
    browse-versions

Each record can occur once. A nonempty stream must end with LF. Reject CRLF, blank records, duplicates, unknown records, unterminated output, and trailing bytes.

The action plugin creates one `Synology Drive` submenu. It adds `Get link` and `Browse previous versions` only when their stable identifiers occur in a valid helper result.

## Scenario Proof

Apply [Scenario discrimination](</home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md>), [Owner composition](</home/mbeutler/.agents/skills/design-preflight/references/owner-composition.md>), and [Full-set results](</home/mbeutler/.agents/skills/design-preflight/references/full-set-results.md>).

### B1 — Supported installation

Both executables accept valid internal-major-4 metadata and ABI 15. Malformed metadata, another major, another ABI, or missing required symbols fail closed.

The focused context-action test must drive the shared compatibility owner through both executables for every contrast.

### B2 — Selection gate

Exactly one existing local regular file or directory can start one list helper. Empty, multiple, duplicate-multiple, remote, missing, or other local objects start no helper and produce no action.

### B3 — Complete action set

Traverse every returned menu leaf. Select only enabled items whose internal names exactly match the two proved names.

Cover empty, one, both, disabled, unrelated, duplicate, prefix, and prefix-before-later-exact sets. Duplicate supported names fail closed. Preserve the order of the valid returned set.

The plugin parser must cover empty, exact, duplicate, unknown, blank, CRLF, unterminated, and valid-prefix-plus-trailing-data streams.

### B4 — Terminal result and deadline

Accept output only after a normal zero exit within the 200 ms internal deadline. Start failure, crash, nonzero exit, partial output, or timeout produces no action.

On timeout, kill the child and allow 25 ms for terminal collection. The complete plugin call must return within 250 ms. The test must prove that the child PID is gone and no later helper starts.

### B5 — Menu construction

Return no root action for an empty valid result. Otherwise, return one `Synology Drive` submenu with only the available project labels in helper order.

The actual `KFileItemActions::addActionsTo` path must discover the staged production module and receive the submenu. Direct class construction or `QPluginLoader` alone cannot close this boundary.

### B6 — Get link transition

Listing sends no UI action. Triggering `Get link` starts one fresh action helper. The fake opaque seam must record only `share_link` and the canonical selected path.

### B7 — Browse transition

Listing sends no UI action. Triggering `Browse previous versions` starts one fresh action helper. The fake opaque seam must record only `list_version` and the canonical selected path.

For each activation action, cover start failure, zero exit, and nonzero exit. Require one attempt, no retry, one error on failure, no error on success, and terminal child cleanup.

### B8 — Process isolation

The actual action module host must not link or map a Synology library. Only the separate production action-helper process can map the fake private library.

### B9 — Package lifecycle

The DEB and RPM must contain the existing product files plus the private action helper and action module. They must exclude the diagnostic probe and all Synology files or dependencies.

Archive and lifecycle validation must cover the complete file set, loader results, permissions, native integrity, and removal.

## Plan of Work

First, create a small shared compatibility source that owns INFO parsing and ABI-15 resolution. Give it explicit paths so release callers use fixed installed paths and tests use fixtures. Preserve the existing status command's output and failure text.

Then convert the probe logic into the private production action helper. Remove its diagnostic self-test interface. Use a fake private menu library to exercise the production traversal and activation code.

Next, add the KF6 action plugin and metadata. Reject unsupported selections before process creation. Use one local `QProcess` for the bounded list call. Use tracked asynchronous `QProcess` objects for explicit activation.

Add one focused context-action CTest path. It must use the production module, production action helper, fake private menu owner, and fake UI seam.

Finally, expand DEB and RPM validation for the two new files. Update the user guides and technical report. Keep version 0.3.0 unreleased.

## Concrete Steps

Configure and build from the repository root:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build

Run the focused proof:

    ctest --test-dir build -R context-actions --output-on-failure

Run the complete local gates only after the production candidate exists:

    ctest --test-dir build --output-on-failure
    ./ci/run
    ./ci/package

Do not run either action against the installed interactive Synology client.

## Validation and Acceptance

The focused test must prove B1 through B8 through real production entry points and fake opaque external owners. `./ci/package` must prove B9 for both distributions.

The context-menu request must complete in less than 250 ms for a held fake helper. A normal fake result must show the correct submenu through the real KF6 action loader.

The final module and Dolphin-equivalent host must contain no Synology dependency or mapping. The packages must contain no Synology binary.

After substantive documentation is final, run boundary trace closure and native correctness review. Move SDD-008 to Completed only after the final clean result.

## Idempotence and Recovery

Builds and fake-owner tests are repeatable. Each helper process runs once. Do not retry a failed list or action.

If a local validation command fails, correct the in-scope cause and rerun its direct check. Do not use the installed Synology UI as an automated fallback.

## Artifacts and Notes

Keep detailed commands and results in `.agent/test-results/SDD-008-context-menu-actions.md`. Do not stage that file.

Preserve the SDD-007 technical evidence. Do not commit raw debugger or system-trace output.

## Interfaces and Dependencies

Use C++17, the C++ standard library, and `dl` in the action helper. Load `libnautilus-extension.so.4` and the installed Synology helper at runtime.

Use Qt 6 Widgets, Qt 6 Core, KF6 KIO Widgets, and KF6 Core Addons in the action module. Add no new third-party dependency.

Use KPluginFactory metadata with service type `KFileItemAction/Plugin`. Install the module only in `kf6/kfileitemaction`.

The package retains the explicit Nautilus-extension runtime dependency. Native package generators calculate the other shared-library dependencies.

## Prior Plan Reconciliation

Record: `.agent/plan-history/plan-summary.20260901T141135241158Z.eeb9fa667ce56a9825769936caa61679ed330bf28b9dd59527512d9374a53243.md`
Status: superseded
Scope: SDD-008 must never synchronously wait in Dolphin.
Reason: KF6 defines `actions()` as synchronous, and the user accepted a bounded lookup.
Replacement: Use a 200 ms internal deadline and a 250 ms external ceiling, with no idle polling.

Record: `docs/plans/SDD-007-context-menu.md`
Status: superseded
Scope: CI, packaging, native review, and standalone prototype delivery as SDD-007 completion gates.
Reason: The user clarified that SDD-007 is a viability study, not a vetted product implementation.
Replacement: The recorded live evidence completes SDD-007. SDD-008 owns production code, packaging, CI, and review.

All other accepted compatibility, isolation, package, version, and post-version-1.0 hosted-CI decisions remain carried.
