# Synology Drive Dolphin Extension (Unofficial)

The Synology Drive Dolphin Extension (Unofficial) adds Synology Drive status overlays and file actions to Dolphin.

The project contains five components:

- `synodrive-status` reads status information through the installed Synology Drive helper.
- `synodrive-overlay.so` supplies asynchronous overlays through the KF6 `KOverlayIconPlugin` interface.
- `synodrive-fileitemaction.so` adds a Synology Drive submenu to Dolphin.
- `synodrive-action` gets available actions and sends one selected action to Synology Drive.
- `synodrive-tray-patch` manages the optional tray left-click patch.

Dolphin never loads the private Synology library. Separate helper processes isolate status queries and user-selected actions.

## Supported system

The source build supports this configuration:

- x86_64 Linux
- Synology Drive 8.x with internal client major 4
- Synology icon-overlay ABI directory `15`
- Dolphin with KF6 and Qt 6
- `libnautilus-extension4`

The release packages target Ubuntu 26.04 and Fedora 44 on x86_64. Ubuntu is the supported live target. Fedora package behavior is validated, but Synology Drive runtime support depends on a community package. Fedora runtime reports and focused pull-request fixes are welcome.

Other internal client majors and malformed client metadata fail without an overlay. The Synology interface is private and can change in a future release.

## Status overlays

| Synology status | Dolphin overlay |
| --- | --- |
| Synced | `emblem-default` |
| Syncing | `emblem-synchronizing` |
| Read-only | `emblem-readonly` |
| No permission | `emblem-unreadable` |
| Unknown or unsupported | No overlay |

Error and conflict overlays are not included because their Synology status values are not verified.

## File actions

Right-click one existing local file or folder. The Synology Drive submenu can contain these actions:

- Get link
- Browse previous versions

Synology Drive decides which actions are available. Get link opens the Synology share window, which contains a copy-link action. Browse previous versions opens the Synology file-history window.

## Optional tray patch

Synology Drive Client does not open its tray menu after a normal left click on Linux. The extension includes an optional patch for recognized internal-major-4 executables.

Package and default source installation do not apply the patch. Run `synodrive-tray-patch apply` to opt in. Run `synodrive-tray-patch restore` before you remove the extension. You must restart Synology Drive Client after either change.

The patch changes a private Synology executable. A client update can remove it or make the layout unsupported. The command checks the executable before each change and fails without modifying unknown layouts.

## Install and use

Read [Installation](docs/INSTALL.md) for dependencies, build commands, installation, upgrades, and removal.

Read [Usage](docs/USAGE.md) for Dolphin operation, command-line use, status names, and troubleshooting.

Read the [technical report](docs/SYNODRIVE_DOLPHIN_RECON.md) for the private interface, process isolation, validation evidence, and compatibility risks.

Read the [release notes](docs/RELEASE_NOTES.md) for changes in each published version.

Read the [production-readiness roadmap](docs/roadmap.md) for accepted packaging, compatibility, CI, and context-menu work.

Read the [security policy](SECURITY.md) for private vulnerability reports. Read [Contributing](CONTRIBUTING.md) before you submit a pull request or a Fedora runtime report.

Install the v1.0.0 Ubuntu package with:

```bash
sudo apt install ./synodrive-dolphin_1.0.0-1_amd64.deb
```

On Fedora 44, install the RPM with:

```bash
sudo dnf install ./synodrive-dolphin-1.0.0-1.x86_64.rpm
```

## Development

Run the full Ubuntu and Fedora distribution gate:

```bash
./ci/run
```

The first run downloads and builds the toolchain images. Later runs reuse Docker image layers.

Build and inspect both release packages:

```bash
./ci/package
```

This command writes one DEB and one RPM below `build/packages`. It installs, verifies, and removes each package in a disposable container.

For a release upgrade check, download both published v0.4.0 packages into one directory and run:

```bash
./ci/package --upgrade-from /absolute/path/to/v0.4.0-packages
```

This command verifies both baseline package digests. It tests clean installation and a native v0.4.0-to-v1.0.0 upgrade for each format.

GitHub CI uses the same `./ci/run` command. It starts only when a reviewed draft pull request becomes ready, and after a merge to `main`.

For a fast build on the current host, run:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

The tests use fake Synology helpers. They do not stop or modify the active Synology Drive daemon.

## License and third-party software

The Synology Drive Dolphin Extension (Unofficial) is licensed under the [MIT License](LICENSE).

Synology Drive, its libraries, and its data are not part of this project. The MIT License does not grant rights to Synology software.
