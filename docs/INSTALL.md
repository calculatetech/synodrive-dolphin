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
sudo dnf install cmake gcc-c++ kf6-kio-devel ninja-build python3 qt6-qtbase-devel
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
- `build/synodrive-overlay.so`

## Build package candidates

Use Docker to build the official Ubuntu 26.04 DEB and Fedora 44 RPM:

```bash
./ci/package
```

The command writes these files:

- `build/packages/ubuntu-26.04/synodrive-dolphin_0.3.0-1_amd64.deb`
- `build/packages/fedora-44/synodrive-dolphin-0.3.0-1.x86_64.rpm`

The command verifies package identity, payload, dependencies, and architecture. It also installs, verifies, and removes each package in a disposable container.

For a native package build, install `dpkg-dev` and `file` for DEB output. Install `rpm-build` for RPM output. Then configure and build the source before you run one generator:

```bash
cpack --config build/CPackConfig.cmake -G DEB -B build/packages/host
cpack --config build/CPackConfig.cmake -G RPM -B build/packages/host
```

Run only the generator that matches the package tools on the current system.

## Install a package candidate

On Ubuntu 26.04, install the DEB:

```bash
sudo apt install ./build/packages/ubuntu-26.04/synodrive-dolphin_0.3.0-1_amd64.deb
```

On Fedora 44, install the RPM:

```bash
sudo dnf install ./build/packages/fedora-44/synodrive-dolphin-0.3.0-1.x86_64.rpm
```

Restart Dolphin after installation.

## Install from source

Install the command and the Dolphin plugin under `/usr`:

```bash
sudo cmake --install build --prefix /usr
```

The command is installed at `/usr/bin/synodrive-status`. CMake installs the plugin under the Qt 6 plugin directory.

On the supported Debian or Ubuntu system, the plugin path is:

```text
/usr/lib/x86_64-linux-gnu/qt6/plugins/kf6/overlayicon/synodrive-overlay.so
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

Make sure that Qt reports the plugin directory:

```bash
qtpaths6 --plugin-dir
```

Make sure that the installed module exists:

```bash
test -f "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
```

## Upgrade a source installation

Get the new source, rebuild it, and install it over the current files:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build --prefix /usr
```

Then restart Dolphin.

After a Synology Drive upgrade, inspect the installed client metadata:

```bash
sed -n '/^\[Version\]$/,/^\[/p' /opt/Synology/SynologyDrive/INFO
```

Internal major 4 remains supported when the private ABI checks pass. Other majors and malformed metadata fail closed.

## Remove a package installation

On Ubuntu, remove the DEB installation:

```bash
sudo apt remove synodrive-dolphin
```

On Fedora, remove the RPM installation:

```bash
sudo dnf remove synodrive-dolphin
```

Then restart Dolphin. Removal does not change Synology Drive files, settings, or synchronization data.

Package upgrade instructions will be added after the first package release exists.

## Remove a source installation

Remove the three installed files:

```bash
sudo rm /usr/bin/synodrive-status
sudo rm "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
sudo rm /usr/share/doc/synodrive-dolphin/copyright
```

Then restart Dolphin. Source removal does not change Synology Drive files, settings, or synchronization data.
