# Release notes

## 0.3.0 (unreleased)

- Add native CPack DEB and RPM generation.
- Add one command to build Ubuntu 26.04 and Fedora 44 x86_64 package candidates.
- Verify package metadata, payload, dependencies, and target architecture.
- Validate package installation, runtime libraries, integrity, and removal in clean target containers.
- Install the MIT license with source and package installs.
- Add a native Synology Drive submenu for one local file or folder.
- Add Get link and Browse previous versions when Synology Drive permits them.
- Keep Synology libraries in a separate action-helper process.

## 0.2.0

- Support Synology Drive 8.x when its internal client major is 4.
- Stop background status queries for stable paths.
- Poll only recent syncing paths.
- Notify Dolphin only when the visible overlay changes.
- Keep each private Synology library query in a fresh child process.

A stable status change appears after a file-system event or a later Dolphin overlay request.

## 0.1.0

The first proof-of-concept release added isolated Synology Drive status overlays for Dolphin.
