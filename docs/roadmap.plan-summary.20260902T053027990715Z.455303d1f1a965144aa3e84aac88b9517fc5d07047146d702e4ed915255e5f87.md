<proposed_plan>
# SDD-008 Context Actions and Windows Follow-up

## Summary

Ship both Synology Drive actions in a `Synology Drive` Dolphin submenu:

- `Get link` opens Synology’s share window.
- `Browse previous versions` opens Synology’s history window.

Use an isolated helper process. Synology libraries must never enter Dolphin.

Use a 200 ms internal lookup deadline. The complete context-menu call must return within 250 ms. Normal measured latency is 6–9 ms.

Keep version `0.3.0` unreleased. Do not push or publish during this task.

Preflight: ready — use one private helper, one KF6 plugin, and one bounded lookup per context-menu request.

## Roadmap and Planning Changes

- Mark SDD-007 Completed as a successful viability study.
- Preserve its ABI, timing, action, and window evidence in the technical report.
- Do not deliver its diagnostic probe as an SDD-007 product.
- Make SDD-008 Active only when implementation starts.
- Create `docs/plans/SDD-008-context-menu-actions.md` with the accepted scenario boundaries.
- Add SDD-013 to Planned later. It does not block SDD-008 or version 1.0.

Define SDD-013 as follows:

- Inspect a current Synology Drive installation on Windows.
- Record the client version and shell-extension registration.
- Determine whether `Get link` copies a URL, opens the share window, or exposes another command.
- Compare clipboard contents before and after each explicit test action.
- Record the invoked executable, IPC action, or shell command when observable.
- Redact account data and generated links from committed evidence.
- If a stable direct-copy interface exists, create a new Linux implementation task. Do not expand SDD-008 automatically.

## Implementation Changes

### Private action helper

Convert the experimental probe into the private installed executable `synodrive-action`. Install it under the project libexec directory, not `/usr/bin`.

Support these private commands:

    synodrive-action --list ABSOLUTE_PATH
    synodrive-action --activate get-link ABSOLUTE_PATH
    synodrive-action --activate browse-versions ABSOLUTE_PATH

Use these exit results:

- `0`: listing completed, including an empty result, or activation reached the selected callback.
- `1`: compatibility, loading, symbol, menu, or action error.
- `2`: invalid command or path.

For successful listing, stdout uses this exact grammar:

- UTF-8 ASCII records separated by `\n`.
- Nonempty output must end with `\n`.
- Allowed records are `get-link` and `browse-versions`.
- Each record can occur once.
- Empty output means that no supported action is available.
- Reject blank records, unknown records, duplicates, CRLF, unterminated output, and trailing data.

Traverse the complete returned menu. Ignore unrelated and disabled leaves.

Match the GObject item names, not translated labels:

- `NautilusCloudStation::ShareLink` maps to `get-link`.
- `NautilusCloudStation::VersionBrowse` maps to `browse-versions`.

A list-only debugger inspection proved both name-to-label mappings on ABI 15. It did not activate either action.

Keep the selection, URI, menu roots, submenus, and selected item alive through activation. Release each owned object once after activation returns.

Extract the existing internal-major and ABI checks into one shared compatibility owner. Preserve all current `synodrive-status` results and error text.

Both executables must require:

- Internal Synology client major `4`.
- Icon-overlay ABI `15`.
- The expected installed library and symbols.

Production code must provide no alternate helper path or version fallback.

### Dolphin action plugin

Add `synodrive-fileitemaction.so` with KPluginFactory metadata. Install it in `kf6/kfileitemaction`.

The plugin must reject the selection before process creation unless all conditions are true:

- The selection contains exactly one item.
- The URL is local.
- The path exists.
- The path is a regular file or directory.

Run `synodrive-action --list` once for an accepted selection.

Use a local `QProcess` with these time budgets:

- Allow 200 ms for start and completion.
- On timeout, send `SIGKILL`.
- Allow 25 ms for terminal child collection.
- Return within 250 ms in the supported operating model.

Accept stdout only after a normal zero exit. Discard partial output after every failure.

Return no action for empty, malformed, timed-out, crashed, or nonzero results.

For a valid result, create one `Synology Drive` submenu. Add only the available actions, in helper order.

Menu construction must never activate Synology. Each explicit click starts exactly one asynchronous helper process.

Track each activation process until it finishes:

- Emit no error after a zero exit.
- Emit one native plugin error after start failure or nonzero exit.
- Remove the completed process from the tracked set.
- Do not retry.

### Packaging and Documentation

Add the private helper and action plugin to both package inventories. Preserve the existing command, overlay plugin, and copyright file.

Update package validation for:

- Exact archive and installed paths.
- File types, ownership, and permissions.
- Dynamic-library resolution.
- Native package integrity.
- Complete removal.
- Exclusion of the diagnostic probe and all Synology files or dependencies.

Keep the existing Nautilus runtime dependency. Add only the Qt Widgets and KF6 KIO Widgets linkage already supplied by the target distributions.

Document these user-visible facts:

- `Get link` opens Synology’s share window, which contains the copy-link action.
- `Browse previous versions` opens Synology’s history window.
- The history window can lack a close action. Synology owns this behavior.
- The extension does not promise direct clipboard population.
- Context actions perform no background polling.

## Test Plan

Add one focused `context-actions` CTest path that uses fake Synology menu and UI owners. Automated tests must never activate the installed interactive Synology client.

Cover these scenarios:

1. Valid internal major 4 and ABI 15 succeed through both executables.
2. Malformed metadata, another major, another ABI, and missing symbols fail through both executables.
3. Empty, multiple, remote, missing, and unsupported selections start no helper.
4. One existing local file or directory starts one list process.
5. Empty, one-action, and two-action menus produce the exact submenu.
6. Disabled, unrelated, duplicate, and prefix records do not create incorrect actions.
7. A later exact item remains visible after an earlier prefix item.
8. Unknown, blank, unterminated, CRLF, and trailing stdout records fail closed.
9. Start failure, crash, nonzero exit, and partial output produce no menu.
10. A held list process is killed and collected before the 250 ms external limit.
11. Menu construction sends no UI action.
12. `Get link` sends only `share_link` with the canonical path.
13. `Browse previous versions` sends only `list_version` with the canonical path.
14. Activation start failure and nonzero exit emit one error and do not retry.
15. Successful activation emits no error and releases its tracked process.
16. The actual `KFileItemActions` loader discovers the staged production module and submenu metadata.
17. The Dolphin-equivalent host maps no Synology library. Only the helper child maps the fake private library.
18. Both packages contain the exact expanded payload and remove every owned file.

After the focused tests pass, run:

    ctest --test-dir build --output-on-failure
    ./ci/run
    ./ci/package

Then complete boundary trace closure and native correctness review for SDD-008. Do not repeat the real SDD-007 actions.

## Assumptions and Prior Plan Reconciliation

- The supported platform remains x86_64 Linux, KDE 6, and Synology Drive public 8.x with ABI 15.
- English project labels are sufficient. Synology label localization does not affect internal action matching.
- Direct clipboard integration, Windows code, multiple selections, remote URLs, and other Synology actions remain out of scope.
- Hosted CI, release publication, and pushing remain separate work.

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

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/SDD-007-context-menu.md -->
<!-- cpk-plan-spec: docs/plans/SDD-008-context-menu-actions.md -->
</proposed_plan>