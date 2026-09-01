# Synology Drive Dolphin Extension (Unofficial): technical report

This report records the local findings and the implementation result for this computer. The interface is private and version-specific.

## 1. Installed client

The installed package is `synology-drive` version `8.0.2-17889` for AMD64. The relevant files are:

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

Debugger type information shows that `IconOverlayInfo` contains two integers: `enable` and `file_status`. This project uses that layout only after it verifies the exact package version and ABI directory.

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

`getOverlays` must return immediately. A cache miss queues the path and returns no overlay. A later response updates the cache and emits `overlaysChanged`. This keeps the private Synology code and its threads out of Dolphin.

## 9. Fragility assessment

The exact package version, ABI directory, mangled symbols, C++ struct layout, raw status values, helper path, and SQLite behavior can change after a Synology update. The executable checks the package version, ABI directory, library, symbols, exceptions, and raw range. An unsupported installation fails without an overlay.

NUL framing and the KF6 plugin interface are under this project's control. The private helper contract is not. Do not redistribute Synology binaries. A public release needs legal review.

## 10. Prototype and acceptance result

The CLI fixture suite passes. It covers input validation, all six raw states, invalid values, an unavailable daemon seam, wrong version, wrong ABI, missing runtime, missing symbol, newline paths, split and coalesced stream frames, fresh query-child PIDs, a query error, and recovery through the same wrapper. Test overrides exist only in the test binary. The installed binary always checks the real package and runtime.

The live check on 2026-09-01 produced these results:

- Active sync root: `synced`
- Outside root: `unknown`
- First same-process unload design: stale `syncing` after the Synology interface reported completion; rejected
- Fresh-child persistent wrapper: `syncing -> synced -> synced -> syncing -> syncing -> synced`
- Wrapper after transition: alive
- Unique file: `synodrive-overlay-test-1788240371250812885`
- Cleanup: file absent

The runtime trace found two read-only main-database opens, two read/write SQLite WAL/SHM coordination opens, no Synology-path rename, unlink, truncate, or positional-write operation, and no Internet-family endpoint.

The KF6 plugin is complete. Its test loads the built module through `QPluginLoader`. A composed check calls `getOverlays` through the reviewed CLI and its private-library fixture. It proves that one wrapper uses different query-child PIDs. The suite also proves nonblocking cache misses, all six mappings, UTF-8 framing, and active and queued deduplication. It proves a terminal result for each distinct queued URL. The failure checks cover the full queue, oversized responses, and `error` responses. A direct-signal check queues recovery while the old wrapper stops. The suite also proves the scaled refresh deadline, stale-overlay removal, process restart, and inactive-cache pruning. Plugin destruction is nonblocking and removes the wrapper and its active query child. Static inspection shows that the plugin depends only on Qt, KF6 KIO, and standard runtime libraries. It has no private helper symbols or dynamic-loader calls. All four selected emblem names exist in the installed icon themes.

The temporary install manifest contains only:

```text
/usr/bin/synodrive-status
/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/overlayicon/synodrive-overlay.so
```

Neither file contains a Synology binary.

### Build and install

Install the development and runtime dependencies. This does not require the Nautilus application:

```bash
sudo apt install cmake ninja-build g++ qt6-base-dev libkf6kio-dev libnautilus-extension4
```

Build and test:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

Install for the system:

```bash
sudo cmake --install build --prefix /usr
```

Restart Dolphin after installation. Do not copy any Synology library into the install tree. The helper loads the user's installed Synology copy at run time.
