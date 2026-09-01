# Usage

The Synology Drive Dolphin Extension (Unofficial) operates automatically after installation. It adds overlays to local files that Synology Drive knows about.

## Use Dolphin overlays

1. Start Synology Drive.
2. Open a configured Synology Drive folder in Dolphin.
3. Wait for Dolphin to request the file status.

A new or changed status usually appears within two seconds for a visible file. The extension removes entries that Dolphin does not request for 30 seconds.

The plugin does not add overlays to remote URLs. Unknown and unsupported files have no overlay.

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
- Synology Drive version `8.0.2-17889` is installed.
- The `current` icon-overlay link resolves to ABI directory `15`.
- `libnautilus-extension.so.4` is available.
- The Dolphin plugin exists under the Qt 6 plugin directory.

Run these commands:

```bash
dpkg-query -W -f='${Version}\n' synology-drive
readlink -f "$HOME/.SynologyDrive/SynologyDrive.app/icon-overlay/current"
ldconfig -p | grep libnautilus-extension.so.4
test -f "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
```

Then query a known synchronized file:

```bash
synodrive-status "/absolute/path/to/a/synced/file"
```

The diagnostic on stderr identifies version, ABI, library, symbol, or helper errors. Restart Dolphin after you reinstall the plugin.

### An overlay is temporarily absent

The plugin returns immediately while it requests a missing or expired status. The overlay appears after the helper returns a valid result.

If the helper fails, the plugin clears affected overlays. A later local request starts a new helper process.

### Synology Drive was upgraded

This project uses a private, version-specific Synology interface. Do not bypass the version or ABI check.

Use a release that explicitly supports the installed Synology Drive version. A new release must verify the private symbols, status values, and live transitions again.

## Privacy and file safety

The extension sends local path names only to the locally installed Synology helper. It does not call a public NAS or Internet API.

The project does not modify Synology databases or synchronization data. SQLite can open its WAL and shared-memory coordination files during a query.
