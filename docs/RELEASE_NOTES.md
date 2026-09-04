# Release notes

## 1.0.1

- Build the DEB on Debian 13 to use its Qt 6.8.2 compatibility floor.
- Validate clean package lifecycles on Debian 13 and Fedora 44.
- Validate upgrades from the published version 1.0.0 DEB on Ubuntu 26.04 and RPM on Fedora 44.
- Keep the version 1.0 runtime behavior and six-file package payload unchanged.

## 1.0.0

- Promote the proven version 0.4.0 runtime without changing overlay, action, or tray-patch behavior.
- Publish native x86_64 packages for Ubuntu 26.04 and Fedora 44.
- Validate clean package installation and native upgrades from version 0.4.0.
- Document the support matrix, package installation, upgrades, removal, defect reports, and private security reports.
- Add draft-first pull requests, protected `main`, and one review-gated Ubuntu and Fedora CI check.
- Keep Fedora runtime support dependent on community packaging and reports.

## 0.4.0

- Add native CPack DEB and RPM generation.
- Add one command to build Ubuntu 26.04 and Fedora 44 x86_64 package candidates.
- Verify package metadata, payload, dependencies, and target architecture.
- Validate package installation, runtime libraries, integrity, and removal in clean target containers.
- Install the MIT license with source and package installs.
- Add a native Synology Drive submenu for one local file or folder.
- Add Get link and Browse previous versions when Synology Drive permits them.
- Keep Synology libraries in a separate action-helper process.
- Add an opt-in command for the Synology Drive tray left-click patch.
- Add status, idempotent apply, and exact restore operations for recognized major-4 executables.
- Add an optional source-install flag. Keep DEB and RPM installation passive.

## 0.2.0

- Support Synology Drive 8.x when its internal client major is 4.
- Stop background status queries for stable paths.
- Poll only recent syncing paths.
- Notify Dolphin only when the visible overlay changes.
- Keep each private Synology library query in a fresh child process.

A stable status change appears after a file-system event or a later Dolphin overlay request.

## 0.1.0

The first proof-of-concept release added isolated Synology Drive status overlays for Dolphin.
