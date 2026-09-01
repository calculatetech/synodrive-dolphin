# Synology Drive Dolphin Extension (Unofficial)

The Synology Drive Dolphin Extension (Unofficial) shows Synology Drive file-status overlays in the Dolphin file manager.

The project contains two components:

- `synodrive-status` reads status information through the installed Synology Drive helper.
- `synodrive-overlay.so` supplies asynchronous overlays through the KF6 `KOverlayIconPlugin` interface.

Dolphin never loads the private Synology library. The status command isolates each private-library query in a short-lived child process.

## Supported system

This release supports this configuration:

- Debian or Ubuntu on AMD64
- Synology Drive `8.0.2-17889`
- Synology icon-overlay ABI directory `15`
- Dolphin with KF6 and Qt 6
- `libnautilus-extension4`

Other Synology Drive versions fail without an overlay. The Synology interface is private and can change in a future release.

## Status overlays

| Synology status | Dolphin overlay |
| --- | --- |
| Synced | `emblem-default` |
| Syncing | `emblem-synchronizing` |
| Read-only | `emblem-readonly` |
| No permission | `emblem-unreadable` |
| Unknown or unsupported | No overlay |

Error and conflict overlays are not included because their Synology status values are not verified.

## Install and use

Read [Installation](docs/INSTALL.md) for dependencies, build commands, installation, upgrades, and removal.

Read [Usage](docs/USAGE.md) for Dolphin operation, command-line use, status names, and troubleshooting.

Read the [technical report](docs/SYNODRIVE_DOLPHIN_RECON.md) for the private interface, process isolation, validation evidence, and compatibility risks.

Read the [production-readiness roadmap](docs/roadmap.md) for accepted packaging, compatibility, CI, and context-menu work.

## Development

Build and run all automated checks:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests use fake Synology helpers. They do not stop or modify the active Synology Drive daemon.

## License and third-party software

The Synology Drive Dolphin Extension (Unofficial) is licensed under the [MIT License](LICENSE).

Synology Drive, its libraries, and its data are not part of this project. The MIT License does not grant rights to Synology software.
