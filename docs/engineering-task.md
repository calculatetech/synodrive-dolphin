# Synology Drive → Dolphin Overlay Integration: Local Reverse-Engineering Task

## Objective

Investigate the locally installed Synology Drive Client and determine how its bundled Nautilus extension obtains per-file synchronization status.

The end goal is to build a KDE/Dolphin `KOverlayIconPlugin` that shows Synology Drive synchronization overlays such as:

- Synced
- Syncing
- Error
- Conflict
- Possibly ignored / unavailable / placeholder states if Synology exposes them

Do **not** implement the Dolphin plugin yet.

This task is strictly reconnaissance and protocol/interface discovery.

The machine currently has the official Synology Drive **DEB package installed**, but **Nautilus is not installed**.

The public Synology Drive API is available, but avoid it for this project unless investigation proves there is no usable local interface. The preferred architecture is to retrieve status from the already-running Synology Drive desktop client/daemon.

---

# Known Background

Synology's Linux package includes a Nautilus extension similar to:

```text
/usr/lib/nautilus/extensions-3.0/libnautilus-drive-extension.so
/usr/lib/nautilus/extensions-4/libnautilus-drive-extension-4.so
```

The package also contains Synology's own application libraries under:

```text
/opt/Synology/SynologyDrive/
```

Historically, Synology packages have contained files related to:

```text
package/cloudstation/icon-overlay/
```

and a library such as:

```text
plugin-cb-4.so
```

Exact paths may differ in the currently installed version.

The Nautilus extension presumably asks the running Synology Drive client or daemon for sync state.

We need to determine **how**.

Likely possibilities:

1. Unix-domain socket IPC
2. Local TCP IPC
3. D-Bus
4. Shared-memory IPC
5. Local SQLite/database lookup
6. Filesystem extended attributes
7. Private Synology shared-library API
8. Status/cache files maintained by the Synology daemon
9. Some combination of the above

---

# Constraints

Do not:

- Modify Synology Drive files.
- Break the installed client.
- Delete or rewrite its databases.
- Connect to or manipulate the NAS through the public Drive API.
- Install Nautilus unless there is a compelling reason and no better approach.
- Perform sync operations that risk user data.
- Patch Synology binaries during this reconnaissance pass.

Read-only inspection, tracing, disassembly, and controlled test-file creation inside an existing sync root are acceptable.

If a test file is required, create a harmless uniquely named file and document it.

---

# Phase 1 — Inventory the Installed Package

Identify package version and package contents.

Try:

```bash
dpkg -l | grep -i synology
dpkg -s synology-drive
dpkg -L synology-drive
```

If the package has another exact name, discover it first.

Determine:

- Installed Synology Drive version
- Executables
- Shared libraries
- Nautilus extension files
- Overlay-related files
- Icons
- Configuration files
- Databases
- Runtime helpers

Search package contents for likely relevant names:

```bash
dpkg -L synology-drive | grep -Ei \
'nautilus|overlay|icon|cloudstation|daemon|socket|ipc|status|sync|plugin'
```

Also inspect:

```bash
find /opt/Synology/SynologyDrive \
  -type f \
  \( -name '*.so' -o -name '*.db' -o -name '*.sqlite*' -o -name '*.conf' \) \
  -print 2>/dev/null
```

Produce a short inventory of anything likely relevant.

---

# Phase 2 — Identify Running Synology Components

Determine what Synology Drive processes are active.

Use:

```bash
ps aux | grep -Ei 'synology|cloud-drive|cloudstation' | grep -v grep
```

Also:

```bash
pgrep -a -f 'synology|cloud-drive|cloudstation'
```

Record:

- Process names
- PIDs
- Executable paths
- Parent-child relationships
- Command-line arguments

Inspect each relevant process:

```bash
readlink -f /proc/<PID>/exe
tr '\0' ' ' < /proc/<PID>/cmdline
ls -l /proc/<PID>/fd
```

Do not attach invasive debuggers yet.

---

# Phase 3 — Discover IPC Endpoints

Search for Unix sockets associated with Synology.

```bash
ss -xlpn
```

Then narrow:

```bash
ss -xlpn | grep -Ei 'synology|drive|cloud'
```

Also:

```bash
find /tmp /run /run/user/$UID "$HOME" \
  -type s \
  -print 2>/dev/null | grep -Ei 'synology|drive|cloud'
```

Inspect open Unix sockets:

```bash
lsof -U | grep -Ei 'synology|drive|cloud'
```

Also check TCP listeners:

```bash
ss -ltnp | grep -Ei 'synology|drive|cloud'
```

If the processes appear to communicate through unnamed sockets, inspect `/proc/<PID>/fd`.

Record any likely IPC endpoints.

---

# Phase 4 — Check D-Bus

Determine whether Synology exposes anything over D-Bus.

Use:

```bash
busctl --user list | grep -Ei 'synology|drive|cloud'
busctl --system list | grep -Ei 'synology|drive|cloud'
```

Also inspect process environment for D-Bus usage if helpful.

If a likely Synology D-Bus service exists, introspect it, but do not invoke mutating methods.

---

# Phase 5 — Inspect the Nautilus Extension Without Nautilus

Locate the current Nautilus extension.

Examples:

```text
/usr/lib/x86_64-linux-gnu/nautilus/extensions-3.0/
/usr/lib/x86_64-linux-gnu/nautilus/extensions-4/
/usr/lib/nautilus/extensions-3.0/
/usr/lib/nautilus/extensions-4/
```

Once found, inspect it.

Example:

```bash
file /path/to/libnautilus-drive-extension*.so

ldd /path/to/libnautilus-drive-extension*.so

readelf -d /path/to/libnautilus-drive-extension*.so

readelf -Ws /path/to/libnautilus-drive-extension*.so | c++filt

nm -D /path/to/libnautilus-drive-extension*.so | c++filt
```

Search strings aggressively:

```bash
strings -a /path/to/libnautilus-drive-extension*.so | \
grep -Ei \
'socket|connect|status|overlay|icon|sync|synced|error|conflict|daemon|cloud|drive|cloudstation|ipc|dbus|sqlite|tmp|run'
```

Also dump all reasonably interesting strings to a file for inspection:

```bash
strings -a /path/to/libnautilus-drive-extension*.so \
  > /tmp/synology-nautilus-strings.txt
```

Look specifically for:

- Socket paths
- Function names
- Protocol keywords
- Status names
- Icon names
- Environment variables
- Configuration paths
- Database paths
- Synology helper library names

---

# Phase 6 — Follow the Dependency Chain

If the Nautilus extension is just a thin wrapper around another Synology `.so`, inspect that next.

Search dependencies from:

```bash
ldd /path/to/libnautilus-drive-extension*.so
```

Then locate interesting private libraries beneath:

```text
/opt/Synology/SynologyDrive/
```

Run the same inspection:

```bash
file
ldd
readelf -d
readelf -Ws
nm -D
strings
```

Particularly investigate anything whose name contains:

```text
overlay
cloudstation
drive
daemon
client
shell
extension
plugin
ipc
sync
status
```

The goal is to trace:

```text
Nautilus callback
    ↓
Synology helper function
    ↓
status lookup mechanism
```

Document that call chain as far as possible.

---

# Phase 7 — Static Binary Analysis

If symbols and strings are insufficient, use available tools such as:

```text
objdump
readelf
nm
strings
radare2
rizin
Ghidra
IDA Free
```

Use whichever is already available.

Focus on functions referenced by Nautilus callbacks such as:

```text
get_file_items
update_file_info
get_file_info
get_emblem
get_status
overlay
```

Names may be stripped.

Search for xrefs to:

- Socket-related libc calls:
  - `socket`
  - `connect`
  - `send`
  - `recv`
  - `sendmsg`
  - `recvmsg`
- SQLite:
  - `sqlite3_open`
  - `sqlite3_prepare`
- Filesystem metadata:
  - `getxattr`
  - `lgetxattr`
- D-Bus / GDBus:
  - `g_dbus_*`
- GLib IPC:
  - `GSocket`
  - `GDBus`
  - `GIOChannel`

If a socket path string exists, identify the function referencing it.

If a status enum or set of icon names exists, map numerical values to likely meanings.

---

# Phase 8 — Extended Attributes

Determine whether Synology stores status directly on files.

First identify an existing sync root.

Do this from Synology configuration/database files if possible rather than guessing.

For a known synced file:

```bash
getfattr -d -m - /path/to/synced/file
```

For a known synced directory:

```bash
getfattr -d -m - /path/to/sync/root
```

Also:

```bash
stat /path/to/synced/file
```

Compare:

- Synced file
- Newly modified file
- Unsynced file outside Drive
- File inside Drive root but excluded from sync, if available

Do not conclude absence from one file alone.

---

# Phase 9 — Look for Local Databases

Search for Synology databases under:

```bash
find "$HOME" /opt/Synology/SynologyDrive \
  \( -name '*.db' -o -name '*.sqlite' -o -name '*.sqlite3' \) \
  -print 2>/dev/null
```

Also examine likely application data locations:

```text
~/.SynologyDrive/
~/.config/
~/.local/share/
```

If SQLite databases are found, inspect read-only:

```bash
sqlite3 -readonly database.db '.tables'
sqlite3 -readonly database.db '.schema'
```

Search schemas for:

```text
path
status
sync
state
file
folder
error
conflict
```

Do not alter databases.

If the client keeps direct per-file state in SQLite, determine whether the DB appears:

- authoritative,
- cache-only,
- dynamically updated,
- safe to read concurrently.

---

# Phase 10 — Runtime Tracing Without Nautilus

Because Nautilus is not installed, investigate whether the extension or underlying helper library can be exercised independently.

Do **not** blindly `dlopen()` unknown libraries inside arbitrary processes without understanding their initialization expectations.

Safer runtime approaches:

## Option A — Trace the Drive Daemon

Attach `strace` to the relevant running daemon:

```bash
strace -ff \
  -p <PID> \
  -e trace=network,ipc,openat,read,write,sendto,recvfrom,sendmsg,recvmsg \
  -s 4096 \
  -o /tmp/synology-daemon-trace
```

Then perform a harmless sync event:

```bash
touch /path/to/sync/root/.synology-overlay-test-<timestamp>
```

Wait for it to synchronize, then optionally modify it once.

Inspect the trace for:

- status database activity,
- local IPC,
- event notifications,
- socket communication.

Clean up the test file after documenting behavior.

## Option B — Trace the Synology GUI

Launch or attach to the Synology Drive GUI and look for IPC with its daemon.

## Option C — Minimal Harness

Only if static analysis reveals that the Nautilus extension exposes a simple function or initialization API, consider writing a tiny disposable harness to load it.

Do not proceed with this unless the calling convention and required environment are reasonably understood.

---

# Phase 11 — Detect Runtime Status Changes

Once a likely status source is discovered, prove that it changes with real sync state.

Test sequence:

1. Pick an already-synced harmless file.
2. Verify its initial state.
3. Modify it.
4. Observe transition to syncing/pending.
5. Wait for sync completion.
6. Observe transition back to synced.

If practical, also trigger a safe error-like state without risking data.

Possible harmless candidates:

- Disconnect network briefly
- Pause Synology Drive if the client supports it
- Modify a test file while paused
- Resume

Avoid deliberately creating destructive sync conflicts unless necessary.

Capture exact status responses.

---

# Phase 12 — Determine the Status Vocabulary

Map all discovered values to human-readable states.

Desired result might look like:

```text
0 = unknown
1 = synced
2 = syncing
3 = error
4 = conflict
5 = excluded
6 = offline
```

Do not invent missing states.

Only report values actually observed or strongly supported by static analysis.

If the Nautilus extension maps statuses directly to icon names, document those mappings.

Example:

```text
STATUS_SYNCED      -> synology-drive-ok
STATUS_SYNCING     -> synology-drive-sync
STATUS_ERROR       -> synology-drive-error
```

Exact names are unknown until inspection.

---

# Phase 13 — Determine the Best Integration Layer

At the end of exploration, rank these implementation options:

## Preferred

### A. Direct local IPC

Example:

```text
Dolphin plugin
    ↓
local Unix socket
    ↓
Synology Drive daemon
```

This is likely ideal if a stable request/response protocol can be identified.

---

### B. Small companion helper

Example:

```text
Dolphin KOverlayIconPlugin
    ↓
our background helper
    ↓
Synology local IPC
```

Prefer this if:

- queries are asynchronous,
- connections must stay open,
- subscriptions/events exist,
- protocol parsing is nontrivial.

This keeps Dolphin's UI thread clean.

---

### C. Read Synology status database

Acceptable if:

- schema is stable enough,
- reads are safe,
- status freshness is good,
- no IPC exists.

This is less desirable because schema changes may break us.

---

### D. Call private Synology shared library

Use only if necessary.

Example:

```text
dlopen(private-synology-lib.so)
dlsym(status_function)
```

This is ABI-fragile and tightly couples us to Synology releases.

---

### E. Public Drive API

Last resort.

Avoid if possible because the server API does not inherently represent the desktop client's current local synchronization state.

---

# Dolphin Implementation Target

Once reconnaissance is complete, the intended KDE architecture is approximately:

```text
KOverlayIconPlugin
        │
        ▼
StatusProvider
        │
        ▼
Synology local status interface
```

The KDE plugin must not block Dolphin's UI thread.

Conceptually:

```cpp
QStringList SynologyOverlayPlugin::getOverlays(const QUrl &url)
{
    if (!url.isLocalFile())
        return {};

    auto status = provider.cachedStatus(url.toLocalFile());

    if (!status.has_value()) {
        provider.requestStatus(url);
        return {};
    }

    switch (*status) {
    case SyncStatus::Synced:
        return {"synology-drive-synced"};

    case SyncStatus::Syncing:
        return {"synology-drive-syncing"};

    case SyncStatus::Error:
        return {"synology-drive-error"};

    case SyncStatus::Conflict:
        return {"synology-drive-conflict"};

    default:
        return {};
    }
}
```

Then when an asynchronous status query completes:

```cpp
emit overlaysChanged(url, overlays);
```

Do not design around synchronous daemon calls from `getOverlays()`.

---

# Deliverable

Create a Markdown report named something like:

```text
SYNODRIVE_DOLPHIN_RECON.md
```

The report should contain:

## 1. Installed Client

- Package name
- Version
- Architecture
- Relevant paths

## 2. Running Components

Table:

```text
Process | Executable | Role | IPC observed
```

## 3. Nautilus Extension Architecture

Identify:

```text
Nautilus extension
    ↓
Synology library/libraries
    ↓
status source
```

Include concrete filenames and functions where known.

## 4. IPC / Database / Metadata Findings

Clearly state which mechanism is actually used.

## 5. Protocol

If IPC is involved, document:

- Transport
- Socket/address
- Connection lifecycle
- Request format
- Response format
- Framing
- Encoding
- Status values
- Notifications/events

Provide sanitized hex or textual examples if useful.

## 6. Status Mapping

Example:

```text
Synology value | Meaning | Overlay/icon
```

Only include verified states.

## 7. Sync Root Discovery

Explain how the Synology client determines which local paths belong to Drive sync tasks.

This is important because the Dolphin plugin should ideally avoid querying Synology for unrelated filesystem paths.

## 8. Recommended Dolphin Architecture

Choose the least fragile integration method based on findings.

Explain why.

## 9. Fragility Assessment

Identify likely breakage points across Synology Drive upgrades:

```text
Stable-ish:
- Unix socket path convention
- textual protocol

Fragile:
- private exported function addresses
- SQLite schema
- binary struct layout
```

Use actual findings rather than this example.

## 10. Prototype Recommendation

Provide a concrete next-step implementation plan.

Prefer something like:

```text
Phase 1:
standalone CLI status query tool

Phase 2:
asynchronous StatusProvider library

Phase 3:
KOverlayIconPlugin

Phase 4:
optional Dolphin context menu integration
```

---

# Strong Preference for a CLI Proof of Concept

Before writing anything for Dolphin, determine whether we can produce a tool with behavior like:

```bash
synodrive-status ~/SynologyDrive/test.txt
```

Output:

```text
synced
```

or:

```text
syncing
```

or structured output:

```json
{
  "path": "/home/user/SynologyDrive/test.txt",
  "status": "synced"
}
```

If this can be made to work using Synology's local status mechanism, then the difficult part of the project is solved.

Focus the investigation on making that possible.

---

# Important Reasoning Principle

Do not start by designing a replacement synchronization-state system.

Synology already has a working Linux file-manager extension.

Treat that extension as the specification.

The central question is:

> Given an absolute local path, how does Synology's Nautilus extension ask the existing Synology Drive client whether that path is synced, syncing, conflicted, or errored?

Reverse-engineer the smallest possible answer to that question.