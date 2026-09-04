# Installation

This guide installs the Synology Drive Dolphin Extension (Unofficial) for all users on the supported Linux system.

## Requirements

Install Synology Drive 8.x before you build the extension. The installed client must report internal major 4 in `/opt/Synology/SynologyDrive/INFO`.

Start Synology Drive and configure at least one synchronization folder.

The source build requires these packages:

- CMake 3.20 or newer
- Ninja
- Python 3
- A C++17 compiler
- Qt 6 Core and Test development files
- KF6 KIO development files
- `libnautilus-extension4`

On Debian or Ubuntu, install the packages:

```bash
sudo apt update
sudo apt install cmake ninja-build python3 g++ qt6-base-dev libkf6kio-dev libnautilus-extension4
```

The Nautilus application is not required. Only its extension runtime library is required.

Fedora uses different package names. Install the source-build dependencies with:

```bash
sudo dnf install cmake gcc-c++ kf6-kio-devel nautilus-extensions ninja-build python3 qt6-qtbase-devel
```

## Build

From the repository root, configure and build the project:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

Run the automated checks:

```bash
ctest --test-dir build --output-on-failure
```

The build creates these delivery files:

- `build/synodrive-status`
- `build/synodrive-tray-patch`
- `build/synodrive-action`
- `build/synodrive-overlay.so`
- `build/synodrive-fileitemaction.so`

## Build release packages

Use Docker to build the version 1.0.1 DEB and RPM packages:

```bash
./ci/package
```

The command writes these files:

- `build/packages/debian-13/synodrive-dolphin_1.0.1-1_amd64.deb`
- `build/packages/fedora-44/synodrive-dolphin-1.0.1-1.x86_64.rpm`

The command verifies package identity, payload, dependencies, and architecture. It also installs, verifies, and removes each package in a disposable container.

To test an upgrade, put both published version 1.0.0 assets in one directory. Then run:

```bash
./ci/package --upgrade-from /absolute/path/to/v1.0.0-packages
```

The command verifies the published package digests. It tests clean version 1.0.1 installations on Debian 13 and Fedora 44.

It tests the DEB upgrade on Ubuntu 26.04 and the RPM upgrade on Fedora 44.

For a native package build, install `dpkg-dev` and `file` for DEB output. Install `rpm-build` for RPM output. Then configure and build the source before you run one generator:

```bash
cpack --config build/CPackConfig.cmake -G DEB -B build/packages/host
cpack --config build/CPackConfig.cmake -G RPM -B build/packages/host
```

Run only the generator that matches the package tools on the current system.

## Install a release package

Download the packages from the [version 1.0.1 GitHub release](https://github.com/calculatetech/synodrive-dolphin/releases/tag/v1.0.1).

On Debian 13 or Ubuntu 26.04, install the DEB:

```bash
sudo apt install ./synodrive-dolphin_1.0.1-1_amd64.deb
```

On Fedora 44, install the RPM:

```bash
sudo dnf install ./synodrive-dolphin-1.0.1-1.x86_64.rpm
```

Restart Dolphin after installation.

Package installation does not apply the optional tray patch. To apply it, run:

```bash
synodrive-tray-patch apply
```

Restart Synology Drive Client after the command succeeds.

## Install from source

Configure the install prefix before you build. The action plugin stores the private helper path at build time.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

The status and tray-patch commands are installed under `/usr/bin`. The action helper is private and is installed below `/usr/libexec/synodrive-dolphin`. CMake installs both Dolphin plugins under the Qt 6 plugin directory.

To apply the tray patch during this source installation, add the option when you configure the build:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DSYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL=ON
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Run the configure command as the desktop user. CMake records that user's absolute home directory. The option does not support `DESTDIR` or package creation.

On the supported Debian or Ubuntu system, the plugin path is:

```text
/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/overlayicon/synodrive-overlay.so
/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/kfileitemaction/synodrive-fileitemaction.so
```

Restart Dolphin after installation:

```bash
kquitapp6 dolphin 2>/dev/null || true
dolphin &
```

The overlays appear automatically for files in configured Synology Drive folders.

## Verify the installation

Query one absolute path in a configured synchronization folder:

```bash
synodrive-status "/absolute/path/to/a/synced/file"
```

A successful query prints one status name. For example:

```text
synced
```

Inspect the optional tray patch:

```bash
synodrive-tray-patch status
```

The command prints `patched` or `unpatched`. It reports an error and makes no change if the installed executable has an unknown layout.

Make sure that Qt reports the plugin directory:

```bash
qtpaths6 --plugin-dir
```

Make sure that the installed module exists:

```bash
test -f "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
test -f "$(qtpaths6 --plugin-dir)/kf6/kfileitemaction/synodrive-fileitemaction.so"
```

## Upgrade a source installation

Get the new source, rebuild it, and install it over the current files:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Then restart Dolphin.

After a Synology Drive upgrade, inspect the installed client metadata:

```bash
sed -n '/^\[Version\]$/,/^\[/p' /opt/Synology/SynologyDrive/INFO
```

Internal major 4 remains supported when the private ABI checks pass. Other majors and malformed metadata fail closed.

The Synology upgrade can replace the tray patch. Run `synodrive-tray-patch status` after the upgrade. Apply it again only if the command recognizes the new executable.

## Upgrade a package installation

Download version 1.0.1. Install it over the published version 1.0.0 package:

```bash
sudo apt install ./synodrive-dolphin_1.0.1-1_amd64.deb
```

On Fedora 44, run:

```bash
sudo dnf upgrade ./synodrive-dolphin-1.0.1-1.x86_64.rpm
```

Restart Dolphin after the upgrade. Package upgrades do not apply or restore the optional tray patch.

## Remove a package installation

If you applied the tray patch, restore it before you remove this package:

```bash
synodrive-tray-patch restore
```

Restart Synology Drive Client after restoration.

On Debian or Ubuntu, remove the DEB installation:

```bash
sudo apt remove synodrive-dolphin
```

On Fedora, remove the RPM installation:

```bash
sudo dnf remove synodrive-dolphin
```

Then restart Dolphin. Package removal does not apply or restore the tray patch. It does not change Synology settings or synchronization data.

## Remove a source installation

If you applied the tray patch, run `synodrive-tray-patch restore` and restart Synology Drive Client first.

Remove the six installed files:

```bash
sudo rm /usr/bin/synodrive-status
sudo rm /usr/bin/synodrive-tray-patch
sudo rm /usr/libexec/synodrive-dolphin/synodrive-action
sudo rm "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
sudo rm "$(qtpaths6 --plugin-dir)/kf6/kfileitemaction/synodrive-fileitemaction.so"
sudo rm /usr/share/doc/synodrive-dolphin/copyright
```

Then restart Dolphin. Source removal does not apply or restore the tray patch. It does not change Synology settings or synchronization data.
