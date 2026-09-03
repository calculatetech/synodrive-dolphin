# Usage

The Synology Drive Dolphin Extension (Unofficial) operates automatically after installation. It adds overlays to local files that Synology Drive knows about.

## Use Dolphin overlays

1. Start Synology Drive.
2. Open a configured Synology Drive folder in Dolphin.
3. Wait for Dolphin to request the file status.

A stable path has no background status query. A file-system event or a later Dolphin overlay request checks the status again.

If a recent path is syncing, the extension checks it once per second. The checks stop when the path becomes stable. They also stop after 30 seconds without a Dolphin request.

The plugin does not add overlays to remote URLs. Unknown and unsupported files have no overlay.

## Use file actions

1. Right-click one existing local file or folder.
2. Open the Synology Drive submenu.
3. Select Get link or Browse previous versions.

Get link opens the Synology share window. Use its copy-link action to copy the share link. Browse previous versions opens the Synology file-history window.

Synology Drive owns both windows. Some installed versions do not show a close action in the file-history window. You can minimize that window or close the Synology Drive user interface.

The submenu is absent when Synology Drive returns no supported action. It is also absent for remote URLs, multiple selections, missing paths, incompatible client versions, and slow helper responses.

## Use the optional tray patch

Inspect the current state:

```bash
synodrive-tray-patch status
```

The command prints `patched` or `unpatched` for a recognized executable.

Apply the patch only if you want a normal tray-icon left click to open Synology's styled menu:

```bash
synodrive-tray-patch apply
```

Restart Synology Drive Client after the command succeeds. The extension does not stop or restart it.

Restore the original tray behavior with:

```bash
synodrive-tray-patch restore
```

Restart Synology Drive Client again. Repeated `apply` and `restore` commands are safe no-ops when the requested file state already exists.

The command accepts only recognized x86_64 internal-major-4 executable layouts. It makes no change if the file is missing, incomplete, symbolic, or unknown. A Synology update can remove the patch. Run `status` before you apply it again.

## Query one path

Use `synodrive-status` to inspect one local path:

```bash
synodrive-status "/absolute/path/to/file"
```

The path must be absolute. Spaces and newlines are supported.

The command prints one of these values:

| Output | Meaning |
| --- | --- |
| `unknown` | Synology returned no known file status. |
| `synced` | The file is synchronized. |
| `syncing` | Synology is transferring or processing the file. |
| `unsupported` | Synology does not support an overlay for the file. |
| `read-only` | The synchronized item is read-only. |
| `no-permission` | The current user does not have permission. |

Successful queries exit with code 0. Invalid command syntax exits with code 2. Compatibility or helper errors exit with code 1.

## Stream protocol

The Dolphin plugin uses the persistent stream mode:

```bash
synodrive-status --stdio
```

This mode is for integrations. It reads UTF-8 absolute paths terminated by a NUL byte.

Each response is a status name or `error`, followed by a NUL byte. Responses remain in request order.

Example with one request:

```bash
printf '/absolute/path/to/file\0' | synodrive-status --stdio | tr '\0' '\n'
```

## Troubleshooting

### No overlays appear

Make sure that these conditions are true:

- Synology Drive is running.
- The file is inside a configured synchronization folder.
- Synology Drive 8.x reports internal client major 4.
- The `current` icon-overlay link resolves to ABI directory `15`.
- `libnautilus-extension.so.4` is available.
- The Dolphin plugin exists under the Qt 6 plugin directory.
- The Dolphin action plugin and private helper are installed.

Run these commands:

```bash
sed -n '/^\[Version\]$/,/^\[/p' /opt/Synology/SynologyDrive/INFO
readlink -f "$HOME/.SynologyDrive/SynologyDrive.app/icon-overlay/current"
ldconfig -p | grep libnautilus-extension.so.4
test -f "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
test -f "$(qtpaths6 --plugin-dir)/kf6/kfileitemaction/synodrive-fileitemaction.so"
test -x /usr/libexec/synodrive-dolphin/synodrive-action
```

Then query a known synchronized file:

```bash
synodrive-status "/absolute/path/to/a/synced/file"
```

The diagnostic on stderr identifies metadata, ABI, library, symbol, or helper errors. Restart Dolphin after you reinstall the plugin.

### An overlay is temporarily absent

The plugin returns immediately while it requests a missing or expired status. The overlay appears after the helper returns a valid result.

A stable overlay can remain unchanged until Dolphin requests that path again. A local file-system event usually causes this request.

If the helper fails, the plugin clears affected overlays. A later local request starts a new helper process.

### Synology Drive was upgraded

This project uses a private Synology interface. Do not bypass the internal-major or ABI check.

Internal client major 4 remains supported when all private ABI checks pass. A different internal major requires a new compatibility review.

If you used the optional tray patch, run:

```bash
synodrive-tray-patch status
```

An `unpatched` result means that you can apply the patch again. An error means that the new executable layout is not supported.

## Privacy and file safety

The extension sends local path names only to the locally installed Synology helper. It does not call a public NAS or Internet API.

The project does not modify Synology databases or synchronization data. SQLite can open its WAL and shared-memory coordination files during a query.

The tray command modifies `cloud-drive-ui` only after an explicit `apply` or source-install choice. Package installation and removal never modify that file.
