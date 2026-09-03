# Add an opt-in tray left-click patch

This ExecPlan is the durable specification for SDD-015. Maintain it according to `/home/mbeutler/.codex/PLANS.md` until review closure.

## Purpose / Big Picture

Synology Drive Client does nothing when the user left-clicks its tray icon on Linux. The Windows path calls `SysTray::showStyledMenu()` for the same action.

Version 0.4.0 adds a separate command that lets the user inspect, apply, and restore a small Linux binary patch. Installation does not apply the patch unless a source-build user explicitly enables the install option.

## Progress

- [x] (2026-09-02) Prove the repeatable instruction layout in the official major-4 client archive.
- [x] (2026-09-02) Accept the opt-in CLI and source-install option design.
- [x] (2026-09-03) Implement the patch manager and focused tests.
- [x] (2026-09-03) Add the source-install opt-in and passive-install tests.
- [x] (2026-09-03) Update 0.4.0 packages and documentation.
- [x] (2026-09-03) Pass local CI, package validation, trace closure, and native review.
- [x] (2026-09-02) Complete the live apply and restore field tests. A normal tray left click opened the styled menu.

## Surprises & Discoveries

- Observation: Every inspected client from 1.0.3 through 4.2.0 exports both tray methods and ignores the normal `Trigger` activation reason.
  Evidence: Static inspection used official packages from the Synology archive.
- Observation: Every inspected build from 3.5.0 through 4.2.0 has the same normalized 96-byte activation handler and 134-byte styled-menu function.
  Evidence: The instruction pattern occurs exactly once in each inspected binary.
- Observation: The current per-user executable exactly matches the official 4.0.3-17892 package.
  Evidence: Their SHA-256 hashes are identical.
- Observation: A seven-byte edit can redirect only normal left-click behavior and retain the MiddleClick and other activation paths.
  Evidence: The edit replaces the Trigger terminal block and retargets MiddleClick to the old epilogue.
- Observation: Passive DEB and RPM installation and removal preserve recognized patched and unpatched client fixtures.
  Evidence: `./ci/package` passed both container lifecycle checks on 2026-09-03.
- Observation: The first trace and native reviews found four bounded gaps before field testing.
  Evidence: Tests now isolate duplicate symbols and preserve rejected-target mode. The command re-reads target bytes before rename, and generated install code safely stores unusual absolute home paths.

## Decision Log

- Decision: Deliver `synodrive-tray-patch` as a dedicated public command.
  Rationale: Patch management must not change the existing status stream protocol.
- Decision: Treat `apply` as sufficient user consent.
  Rationale: A second confirmation adds no protection to an explicit command.
- Decision: Add `SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL`, default `OFF`, for source builds only.
  Rationale: This provides the requested install-time choice without package-manager scripts.
- Decision: Recognize an exact symbol and instruction template instead of executable hashes.
  Rationale: The same structure occurs across supported major-4 builds, while unknown layouts still fail closed.
- Decision: Restore the known seven original bytes without a backup file.
  Rationale: A Synology update can make a saved full binary stale, while the accepted template defines the inverse exactly.
- Decision: Use an adjacent temporary file and atomic rename.
  Rationale: Validation and write failures before rename must leave the executable unchanged.
- Decision: Do not stop or restart Synology Drive Client.
  Rationale: The user owns the visible desktop action and can restart after the file changes.

## Outcomes & Retrospective

The command lets users apply and restore recognized tray patches. After a client restart, a normal tray left click opens the styled menu.

## Context and Orientation

`src/synodrive_compatibility.cpp` parses an explicit Synology `INFO` path and accepts internal major 4. The patch manager reuses this owner with the per-user portable installation.

The target is `$HOME/.SynologyDrive/SynologyDrive.app/bin/cloud-drive-ui`. Its metadata is `$HOME/.SynologyDrive/SynologyDrive.app/INFO`.

`CMakeLists.txt` owns executable installation and CPack metadata. `ci/validate-package` and `ci/validate-package-lifecycle` own the complete DEB and RPM inventories. The existing overlay and context-action paths are unchanged.

## Product Boundary

Apply [Scope boundaries](</home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md>).

The patch command, source-install hook, package inventory, and user documentation compose SDD-015. Synology's process and styled-menu implementation are opaque.

Supported normal use is one x86_64 Linux user with KDE 6 and Synology Drive public 8.x with internal major 4. The executable must also match the recognized ELF symbols and instruction template.

The task excludes Wayland popup placement, a GUI, arbitrary target arguments, process control, update migration, automatic uninstall restoration, other architectures, hosted CI, publication, and Synology redistribution.

## Interfaces

Install `/usr/bin/synodrive-tray-patch`. Accept only:

    synodrive-tray-patch status
    synodrive-tray-patch apply
    synodrive-tray-patch restore

`status` prints `patched\n` or `unpatched\n`. `apply` and `restore` print the resulting state. A mutation also prints one prefixed restart advisory to stderr. Idempotent operations print no advisory.

Exit 0 means a recognized state or successful transition. Exit 1 means an unsupported target or operational failure. Exit 2 means invalid syntax and prints:

    usage: synodrive-tray-patch status|apply|restore

Add the CMake option `SYNODRIVE_APPLY_TRAY_PATCH_ON_INSTALL`, default `OFF`. When enabled, configuration captures a nonempty absolute `HOME`. The install hook calls the built production command with that captured home. It rejects `DESTDIR` before it invokes the command.

DEB and RPM installation never invokes the command. They contain no maintainer script, prompt, or trigger.

## Scenario Proof

Apply [Scenario discrimination](</home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md>), [Owner composition](</home/mbeutler/.agents/skills/design-preflight/references/owner-composition.md>), and [Full-set results](</home/mbeutler/.agents/skills/design-preflight/references/full-set-results.md>).

### B1 — Supported target

The production command resolves only the fixed paths below `HOME`. It requires valid internal-major-4 metadata, a regular non-symlink ELF64 little-endian x86-64 `ET_DYN` target, unique expected symbols, safe section bounds, a 96-byte activation handler, and one recognized instruction template.

A paired wrong-major, wrong-ELF, missing-symbol, malformed, truncated, or changed-template target must fail without changing its bytes or mode.

### B2 — Explicit opt-in

Only `apply` and a source install configured with the option enabled can create patched state. `status`, the default source install, package creation, and DEB or RPM installation and removal preserve both recognized patched and unpatched seeds.

### B3 — States and idempotence

`status` is read-only. Applying an original target changes it to patched. Applying it again is a no-op. Restoring a patched target recreates the exact original bytes. Restoring original or already restored state is a no-op.

### B4 — Exact transformation

Resolve the exact mangled `SysTray::iconActivated(QSystemTrayIcon::ActivationReason)` and `SysTray::showStyledMenu()` symbols from `.symtab`.

Apply changes seven bytes. Replace the Trigger terminal block at activation-handler offset `0x56` with `leave; jmp rel32 showStyledMenu`. Change the MiddleClick branch displacement at offset `0x1b` so that it reaches the retained epilogue at offset `0x5c`. Restore writes the exact inverse bytes. All unmodified bytes must match the accepted template.

### B5 — Atomic transition

Create an exclusive temporary regular file in the target directory. Write the complete replacement, preserve uid, gid, and mode, sync it, rename it over the target, then sync the directory. Remove the temporary file after every pre-rename failure.

Rename is the commit point. A failure before it leaves the target unchanged. If directory sync fails afterward, report that the target changed but durability was not confirmed and tell the user to run `status`.

### B6 — Source installation

With the option off, source installation never inspects or changes a user target. With it on, configuration captures `HOME=A`; installation with `HOME=B` still applies only to A. Unset, empty, relative, or staged homes fail without mutation.

### B7 — Package lifecycle

Both 0.4.0 packages contain the current five files plus `/usr/bin/synodrive-tray-patch`, each exactly once. They contain no Synology file or dependency and no Debian maintainer file, RPM script, or trigger. Installation and removal preserve paired patched and unpatched user targets.

### B8 — Live result

After the user explicitly applies the patch and restarts Synology Drive Client, static verification must show that Trigger targets the exact styled-menu symbol. One normal tray left click must display that menu.

After restore and restart, the executable must match its original bytes and the added dispatch must be absent. Popup placement is not an acceptance condition.

## Plan of Work

First, implement the bounds-checked ELF inspection and exact reversible transformation in one C++17 command. Reuse the compatibility parser and add no dependency.

Then add focused fixture tests and the optional CMake source-install path. Exercise the release binary with temporary `HOME` directories. Restrict test-only controls to deterministic write-failure injection.

Next, change the project and package candidate to 0.4.0. Expand complete package inventory and lifecycle checks. Update only the documents that own the changed command, setup, compatibility, and release facts.

Finally, run local validation, trace closure, and native correctness review. Leave this task Active until the user completes both live transitions.

## Concrete Steps

Configure and build from the repository root:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build -R tray-patch --output-on-failure

Run the complete local gates:

    ctest --test-dir build --output-on-failure
    ./ci/run
    ./ci/package
    git diff --check

Do not run the patch command against the installed live client during automated validation.

## Validation and Acceptance

The focused test uses generated ELF fixtures and the actual release executable. It proves B1 through B6, including the exact seven-byte diff, exact inverse, output, exit status, idempotence, path selection, and commit-point results.

Package-command and container lifecycle checks prove B7 against complete records. They must reject later extra files and forbidden scripts or triggers.

B8 requires explicit user field testing. Static inspection plus the visible menu is the exact-call oracle; no debugger attachment or automatic foreground window is required.

After substantive documentation is final, run boundary trace closure and native review. Record field-test evidence before moving SDD-015 to Completed.

## Idempotence and Recovery

`apply` and `restore` are repeatable. Unknown and partial states fail closed. No operation retries.

A Synology client update can replace the patch. Run `status`, then apply again only if the new executable is recognized. Restore before uninstalling this project. If the command is already removed, reinstall the project or update Synology Drive Client.

## Artifacts and Notes

Keep detailed command results in `.agent/test-results/SDD-015-tray-patch.md`. Do not stage that file.

Do not retain downloaded Synology packages, extracted binaries, or generated package candidates in Git.

## Interfaces and Dependencies

Use C++17, `<elf.h>`, and standard POSIX file operations. Add no third-party dependency and do not load a Synology library.

The release command accepts no target path or test override. Tests select fixtures through normal `HOME` resolution. A separately compiled test target can inject only external write failures.

## Prior Plan Reconciliation

Record: `.agent/plan-history/plan-summary.20260902T053027990715Z.455303d1f1a965144aa3e84aac88b9517fc5d07047146d702e4ed915255e5f87.md`
Status: superseded
Scope: unreleased 0.3.0 release identity only
Reason: The user selected 0.4.0 for SDD-015, and 0.3.0 was never tagged.
Replacement: Fold the unreleased packaging and context-action work into 0.4.0 with the tray patch.

The byte-identical later duplicate has the same resolution. All other accepted compatibility, isolation, package, local-CI, and publication decisions remain carried.
