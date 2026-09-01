<proposed_plan>
# Synology Drive Dolphin Overlay — Viability Verdict

## Summary

- **Viable as a local, version-gated integration.**
- The installed Synology Drive `8.0.2-17889` exposes enough functionality to build the CLI proof requested in [engineering-task.md](/home/mbeutler/Projects/dolphin-drive/docs/engineering-task.md).
- Do not promise permanent compatibility across Synology releases. The interface is private.

## Evidence and Decision

- Synology ships a thin Nautilus wrapper that loads `plugin-cb-4.so`.
- That helper exports:
  - `PrepareCacheTable()`
  - `GetIconOverlayInfoByPath(const char *, IconOverlayInfo &)`
- A read-only runtime proof returned:
  - Active sync root: `enable=1`, `status=1` (`synced`)
  - Outside path: `enable=1`, `status=0` (`unknown`)
- Verified statuses are:
  - `0 unknown`
  - `1 synced`
  - `2 syncing`
  - `3 unsupported`
  - `4 read-only`
  - `5 no-permission`
- The helper reads `sys.sqlite`, loads transient state from `file-status.sqlite`, and uses `ui.sock` for updates. It does not require D-Bus.
- KF6 officially supports asynchronous cached overlays through `KOverlayIconPlugin`, `getOverlays()`, and `overlaysChanged()`. [KDE API documentation](https://api.kde.org/koverlayiconplugin.html)
- Error and conflict are not exposed as verified per-file states. Do not include them in v1.

## Minimum Delivery Path

1. Build `synodrive-status <absolute-path>` as a separate process that loads the user-installed `plugin-cb-4.so`.
2. Require `libnautilus-extension4`, not the Nautilus application.
3. Return the six verified states and fail cleanly for unsupported Synology versions.
4. Prove `synced → syncing → synced` using one harmless test file.
5. Only after that proof, add a cached KF6 overlay plugin that calls a persistent helper asynchronously.
6. Never load Synology’s private library directly inside Dolphin. A debugger-only probe produced correct results but also raised a C++ lifecycle warning; process isolation protects Dolphin.
7. Defer direct protocol reimplementation until the private helper becomes an observed maintenance problem.

## Tests and Boundaries

- Test synced, outside-root, syncing, read-only, daemon-stopped, and missing-library cases.
- Confirm Dolphin’s main thread never waits for Synology.
- Treat `unknown` as no overlay.
- Remove the unique transition-test file after validation.
- Do not redistribute Synology binaries. Load the installed copy, and obtain legal review before a public release; Synology’s package is marked “All rights reserved.”

<!-- cpk-plan-spec: docs/engineering-task.md -->
</proposed_plan>