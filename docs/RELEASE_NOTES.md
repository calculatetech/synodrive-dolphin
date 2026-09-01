# Release notes

## 0.2.0

- Support Synology Drive 8.x when its internal client major is 4.
- Stop background status queries for stable paths.
- Poll only recent syncing paths.
- Notify Dolphin only when the visible overlay changes.
- Keep each private Synology library query in a fresh child process.

A stable status change appears after a file-system event or a later Dolphin overlay request.

## 0.1.0

The first proof-of-concept release added isolated Synology Drive status overlays for Dolphin.
