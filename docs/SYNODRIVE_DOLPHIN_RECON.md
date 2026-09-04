# Synology Drive Dolphin Extension (Unofficial): technical report

This report records the local findings and the implementation result for this computer. The interface is private and version-specific.

## 1. Installed client

The installed package is `synology-drive` version `8.0.2-17889` for AMD64. The relevant files are:

- `/opt/Synology/SynologyDrive/INFO`, which reports internal client version `4.0.2-17889`
- `/usr/lib/nautilus/extensions-4/libnautilus-drive-extension-4.so`
- `/opt/Synology/SynologyDrive/package/cloudstation/icon-overlay/15/lib/plugin-cb-4.so`
- `~/.SynologyDrive/SynologyDrive.app/icon-overlay/current`, which resolves to ABI directory `15`
- `~/.SynologyDrive/data/db/sys.sqlite`
- `~/.SynologyDrive/data/db/file-status.sqlite`
- `~/.SynologyDrive/ui.sock`
- `~/.SynologyDrive/daemon.sock`

The package includes the Nautilus extension. The Nautilus application is not installed. The helper also needs the runtime library from `libnautilus-extension4`. An APT simulation showed that this package does not install Nautilus.

## 2. Running components

| Process | Executable | Role | IPC observed |
| --- | --- | --- | --- |
| `cloud-drive-ui` | Synology Drive package | Desktop user interface | User D-Bus and local sockets |
| `cloud-drive-connect` | Synology Drive package | Connection manager | Loopback TCP and local sockets |
| `cloud-drive-daemon` | Synology Drive package | File synchronization worker | `daemon.sock` and local database files |

No named Synology service was found on the user or system D-Bus. Synology processes use anonymous user-bus connections for desktop integration.

## 3. Nautilus extension architecture

The Nautilus extension is a small wrapper. It loads the user copy of `plugin-cb-4.so` with `dlopen` and resolves private C++ symbols with `dlsym`.

```text
Nautilus extension
    -> PrepareCacheTable()
    -> GetIconOverlayInfoByPath(const char *, IconOverlayInfo &)
    -> plugin-cb-4.so path cache
    -> file-status.sqlite and sys.sqlite
```

Debugger type information shows that `IconOverlayInfo` contains two integers: `enable` and `file_status`. This project uses that layout only after it verifies internal major 4 and ABI directory 15.

## 4. IPC, database, and metadata findings

The private library is the smallest usable local status interface. It reads `file-status.sqlite` and `sys.sqlite` and uses process-global path-cache state. It can also initialize local `ui.sock` activity. No usable named D-Bus status API was found.

A live trace opened the main SQLite files with `O_RDONLY`. SQLite also opened the associated `file-status.sqlite-wal` and `file-status.sqlite-shm` files with read/write coordination flags. No Internet-family endpoint was used by the query. The prototype does not write the main databases or call a public NAS API.

Repeated `dlopen`, query, and `dlclose` calls in one process did not refresh a completed upload. The Synology user interface showed the file as up to date while that process still returned `syncing`. A fresh process returned the new value. Therefore, the persistent stream wrapper uses a fresh child for each private-library query.

## 5. Private wrapper protocol

One-shot use is:

```text
synodrive-status ABSOLUTE_PATH
```

Success writes one status name and exits 0. A usage error exits 2. A version, ABI, library, symbol, exception, or raw-value error exits 1 and writes no status to stdout.

The Dolphin-facing mode is:

```text
synodrive-status --stdio
```

Requests are UTF-8 absolute paths terminated by NUL. Responses are one verified status name or `error`, also terminated by NUL. The wrapper retains partial input, separates coalesced frames, rejects empty or larger-than-1-MiB frames, and discards a partial frame at EOF. Requests are processed in order. Each request runs in a fresh query child to isolate Synology's process-global cache.

## 6. Status mapping

| Raw value | Verified meaning | Dolphin overlay |
| ---: | --- | --- |
| 0 | Unknown | None |
| 1 | Synced | `emblem-default` |
| 2 | Syncing | `emblem-synchronizing` |
| 3 | Unsupported file | None |
| 4 | Read-only | `emblem-readonly` |
| 5 | No permission | `emblem-unreadable` |

No error or conflict value was verified. The first version does not invent those states.

## 7. Sync-root discovery

`sys.sqlite` contains `session_table`. Its `sync_folder` column identifies local roots. The live check selected an enabled, mounted session and did not guess a path. The root returned `synced`. `/tmp`, outside the configured roots, returned `unknown`.

The prototype lets the Synology helper decide path membership. It does not copy the session schema into the Dolphin plugin.

## 8. Recommended Dolphin architecture

```text
Dolphin KOverlayIconPlugin
    -> asynchronous in-process status cache
    -> one persistent synodrive-status --stdio wrapper
    -> one fresh private-library query child per request
    -> installed plugin-cb-4.so
```

`getOverlays` must return immediately. It returns a cached overlay and queues one deduplicated status request. A cache miss returns no overlay. A later visible change updates the cache and emits `overlaysChanged`. This keeps the private Synology code and its threads out of Dolphin.

Stable cached paths have no timer query. Recent syncing paths poll once per second until they become stable or inactive. The plugin emits `overlaysChanged` only when the mapped overlay changes.

One hundred warm stream queries used approximately 0.48 seconds of CPU time, or 4.8 milliseconds per query. The persistent wrapper used no measurable CPU during a five-second idle sample. Removing stable timer queries therefore removes the material idle cost without changing private-library loading.

The v0.2.0 candidate opened a real synchronized directory in Dolphin. Its 243 initial query children started within 1.994 seconds. No query child started during the remaining 48 seconds of the trace. A separate 20-second sample reported 0.00% Dolphin CPU after a five-second settle.

## 9. Fragility assessment

The internal metadata, ABI directory, mangled symbols, C++ struct layout, raw status values, helper path, and SQLite behavior can change after a Synology update. The executable accepts internal major 4 and checks the ABI directory, library, symbols, exceptions, and raw range. An unsupported installation fails without an overlay.

NUL framing and the KF6 plugin interface are under this project's control. The private helper contract is not. Do not redistribute Synology binaries. This project contains no Synology payload and does not claim a supported Synology API.

## 10. Prototype and acceptance result

The CLI fixture suite passes. It covers valid and malformed INFO metadata, supported majors, all six raw states, and invalid values.

It also covers input errors, helper errors, newline paths, stream framing, query isolation, and wrapper recovery. Test overrides exist only in the test binary.

The installed binary always checks the top-level client INFO file and the real runtime.

The live check on 2026-09-01 produced these results:

- Active sync root: `synced`
- Outside root: `unknown`
- First same-process unload design: stale `syncing` after the Synology interface reported completion. Result: rejected
- Fresh-child persistent wrapper: `syncing -> synced -> synced -> syncing -> syncing -> synced`
- Wrapper after transition: alive
- Unique file: `synodrive-overlay-test-1788240371250812885`
- Cleanup: file absent

The runtime trace found two read-only main-database opens, two read/write SQLite WAL/SHM coordination opens, no Synology-path rename, unlink, truncate, or positional-write operation, and no Internet-family endpoint.

The KF6 plugin is complete. Its test loads the built module through `QPluginLoader`. A composed check calls `getOverlays` through the reviewed CLI and its private-library fixture. It proves that one wrapper uses different query-child PIDs. The suite also proves nonblocking cache misses, all six mappings, UTF-8 framing, and active and queued deduplication. It proves a terminal result for each distinct queued URL. The failure checks cover the full queue, oversized responses, `error` responses, and wrapper exit. The suite proves zero stable timer queries, syncing-only polling, mixed-cache isolation, mapped-overlay notification equality, process restart, and silent inactive-cache pruning. Plugin destruction is nonblocking and removes the wrapper and its active query child. Static inspection shows that the plugin depends only on Qt, KF6 KIO, and standard runtime libraries. It has no private helper symbols or dynamic-loader calls. All four selected emblem names exist in the installed icon themes.

The temporary install manifest for the overlay-only candidate contained:

```text
/usr/bin/synodrive-status
/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/overlayicon/synodrive-overlay.so
/usr/share/doc/synodrive-dolphin/copyright
```

No file contains a Synology binary.

Version 0.4.0 adds the private action helper, the KF6 file-action plugin, and the optional tray-patch command. The Ubuntu 26.04 DEB and Fedora 44 RPM contain the same six regular files. Native scanners generate linked-library dependencies. Explicit metadata adds Dolphin and the Nautilus extension runtime. Neither package requires or contains Synology software.

Version 1.0.0 promotes the same runtime and six-file package payload. It adds a package-upgrade gate, complete release documentation, protected pull requests, and review-gated GitHub CI.

The package command also validates each candidate in a clean target container. It uses the native package manager to install, query, verify, and remove the package. The check runs both installed binaries through the dynamic loader. It also confirms the exact file set, metadata, ownership, permissions, command result, license bytes, and complete removal.

## 11. Context-menu feasibility

SDD-007 proved both installed Synology menu actions on 2026-09-02. The test system used Synology Drive `8.0.2-17889`, internal version `4.0.2-17889`, AMD64, and overlay ABI directory `15`.

The user helper resolved to:

```text
/home/mbeutler/.SynologyDrive/SynologyDrive.app/icon-overlay/15/lib/plugin-cb-4.so
SHA-256: 84d7d68f1722e8943c76701a4d7f351474e55266a74ebd5df8078dd0e1c9bb44
```

The private entry point is:

```text
GList *cstn_private_get_file_item(NautilusMenuProvider *, GList *);
```

The helper calls `nautilus_file_info_get_uri` for the selected object. It converts the URI to a local path and runs its private deciders. It returns a Nautilus root item with a `menu` submenu.

The probe reads the real returned menu objects. It activates a selected child through `nautilus_menu_item_activate`. The private callback then reads the stored action data and calls the matching handler.

One synced file returned both exact enabled labels:

```text
Get link
Browse previous versions
```

The returned GObject names were exact and stable during the study:

```text
NautilusCloudStation::ShareLink
NautilusCloudStation::VersionBrowse
```

Twenty fresh list processes passed. Results ranged from 6.106 ms to 9.070 ms, with a mean of 7.938 ms. Each result included process startup, helper loading, private deciders, traversal, output, and exit.

The debugger reached `ShareLinkHandler::Handle` and `BrowseVersionHandler::Handle`. Each handler received one path: `/home/mbeutler/Documents/wsmbug.txt`.

The “Get link” action connected to `~/.SynologyDrive/ui.sock`. It sent one 69-byte frame with `action=share_link` and the exact path.

The “Browse previous versions” action used the same socket. It sent one 71-byte frame with `action=list_version` and the exact path.

Three consecutive processes sent each action frame and exited normally. The Synology user interface forced each action window to the foreground. The repeated validation windows required a forced stop of `cloud-drive-ui`. The sync daemon and connection process continued to run.

Get link opened the Synology share window. That window contained the sharing options and a copy-link action. Browse previous versions opened the Synology file-history window. The tested window had minimize and download actions, but it had no close action.

SDD-007 succeeded for the recorded installed ABI. SDD-008 implements one user-triggered action at a time through a separate process. Dolphin owns each action process until it exits, even if the context menu closes. If the menu owner closes before a failure, the application shows one nonblocking error dialog. The Dolphin process does not load a Synology library. Automated tests use a fake private provider and do not invoke real actions on an interactive desktop.

## 12. Tray left-click patch

The official Synology archive contains Drive Client packages from 1.0.0 through 4.2.0. Static inspection covered 15 x86_64 packages. Every inspected package exports `SysTray::iconActivated(QSystemTrayIcon::ActivationReason)` and `SysTray::showStyledMenu()`.

The inspected Linux activation handler ignores `QSystemTrayIcon::Trigger`, which is a normal left click. Builds from 3.5.0 through 4.2.0 use the same normalized 96-byte handler and 134-byte styled-menu function. The supported major-4 set included 4.0.0, 4.0.1, 4.0.2, 4.0.3, and 4.2.0.

The patch changes seven positions in the activation handler. It replaces the Trigger terminal block with a direct relative jump to `showStyledMenu()`. It also moves MiddleClick to the old epilogue. All other activation paths remain unchanged.

`synodrive-tray-patch` parses the user's ELF symbol table and validates the complete expected handler before it writes. It accepts only internal major 4, x86-64 ELF, the exact function symbols and sizes, and the recognized original or patched instruction template. Unknown and partial layouts fail without a write.

The command writes a complete adjacent temporary file and atomically replaces the target. Restore applies the exact seven-byte inverse. It does not keep a full backup because a Synology update can make that backup stale.

The project contains no downloaded package or Synology binary. Automated tests generate a small synthetic ELF fixture. A temporary copy of the installed official 4.0.3 executable passed status, apply, status, restore, and byte-for-byte comparison.

## 13. Build and install

Install the development and runtime dependencies. This does not require the Nautilus application:

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev libkf6kio-dev libnautilus-extension4
```

Build and test:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
```

Install for the system:

```bash
sudo cmake --install build
```

To opt in during a source installation, configure with `-DSYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL=ON`. Package installation remains passive. Package users run `synodrive-tray-patch apply` separately.

Restart Dolphin after installation. Do not copy any Synology library into the install tree. The helper loads the user's installed Synology copy at run time.
