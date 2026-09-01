# Installation

This guide installs Dolphin Drive for all users on the supported Linux system.

## Requirements

Install Synology Drive `8.0.2-17889` before you build Dolphin Drive. Start Synology Drive and configure at least one synchronization folder.

The build requires these packages:

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

## Install

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

## Upgrade

Get the new source, rebuild it, and install it over the current files:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build --prefix /usr
```

Then restart Dolphin.

After a Synology Drive upgrade, check the supported version before you reinstall Dolphin Drive. Unsupported versions fail closed.

## Remove

Remove the two installed files:

```bash
sudo rm /usr/bin/synodrive-status
sudo rm "$(qtpaths6 --plugin-dir)/kf6/overlayicon/synodrive-overlay.so"
```

Then restart Dolphin. Removal does not change Synology Drive files, settings, or synchronization data.
