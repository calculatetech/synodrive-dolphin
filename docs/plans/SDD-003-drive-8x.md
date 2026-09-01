# Support Synology Drive 8.x through internal client metadata

This ExecPlan is a living specification until review closure. Maintain it in accordance with `/home/mbeutler/.codex/PLANS.md`.

## Purpose / Big Picture

The current extension accepts only Synology Drive package version `8.0.2-17889`. After this change, it accepts the complete public 8.x family when the installed client reports internal major version 4. The existing private-library safety gates remain in effect.

A user can confirm the change with `synodrive-status ABSOLUTE_PATH`. A supported major-4 installation returns the existing status. Missing, malformed, or unsupported metadata returns the existing compatibility error.

## Decision Log

- Decision: Read `/opt/Synology/SynologyDrive/INFO` instead of a distribution package-manager version.
  Rationale: The installed Debian client owns this file. The accepted Fedora repack preserves the same `/opt/Synology/SynologyDrive` tree.
- Decision: Parse the complete INFO file with C++17 standard-library code.
  Rationale: This keeps the compatibility owner distribution-independent and adds no dependency.
- Decision: Accept numeric `major_version` 4 and ignore valid unrelated fields.
  Rationale: Minor, mini, and build changes do not restrict the accepted support family.
- Decision: Fail closed for ambiguous or malformed INFO syntax anywhere in the file.
  Rationale: A partial or ambiguous file cannot prove the installed client version.
- Decision: Keep all CLI, stream, plugin, process-isolation, ABI, symbol, structure, and status contracts unchanged.
  Rationale: SDD-003 changes only version selection.

## Outcomes & Retrospective

The CLI reads the top-level installed-client INFO file and accepts internal major 4. The same parser governs one-shot, stream, and plugin requests.

The implementation has no package-manager command or new runtime dependency. A complete-file parse prevents a valid prefix from hiding malformed or duplicate metadata.

## Context and Orientation

`src/synodrive-status.cpp` owns the compatibility gate and both CLI entry paths. One-shot mode calls `query` directly. Stream mode calls the same function in a fresh child process for each NUL-terminated request.

`tests/test_cli.py` creates the fake Synology overlay tree and exercises both CLI paths. `tests/test_plugin.cpp` composes the real test CLI with the Dolphin overlay plugin. `CMakeLists.txt` builds separate release and test CLI targets from the same source.

The task uses branch `task/SDD-003-drive-8x`. Its base branch is `main`, and its base commit is `f95eaeb45bbbe4f2f679c25da469d582e7b5d3b3`. The task uses the existing checkout as one isolated writable branch.

## Prior Plan Reconciliation

The following immutable records apply:

- The `20260901T045809` central record and its `docs/engineering-task` sibling established the helper, status range, and isolation requirements. Its exact-version and no-plugin limits are superseded.
- The `20260901T141135` central record and its `docs/engineering-task`, `docs/plans/synodrive-status`, and `docs/roadmap` siblings established SDD-003. Its internal-major-4 policy is carried.
- The `20260901T153200` central record and its `docs/engineering-task`, `docs/plans/SDD-002-local-ci`, `docs/plans/synodrive-status`, and `docs/roadmap` siblings changed only CI and release scheduling. All SDD-003 decisions are carried.
- The `20260901T163218` central record and its specification siblings are the accepted SDD-003 implementation plan. This ExecPlan carries that plan without a scope change.

`docs/engineering-task.md` remains historical. `docs/plans/synodrive-status.md` remains the completed source for inherited CLI, stream, plugin, and isolation behavior.

## Product Boundary

Apply [Scope boundaries](</home/mbeutler/.agents/skills/design-preflight/references/scope-boundaries.md>).

The authoritative source is SDD-003 in `docs/roadmap.md`. The INFO source, parser, and compatibility gate compose with this task. The existing CLI grammar, stream framing, fresh-child isolation, status provider, and overlay plugin are opaque owners. DEB and RPM packaging, context menus, new diagnostics, and future major-5 support remain deferred or excluded.

The supported model is one local x86_64 KDE 6 user. The installed client has a normal readable top-level INFO file, ABI 15, the required runtime and helper, the observed symbols and structure, and raw statuses 0 through 5. Ubuntu 26.04 and Fedora KDE 44 are the automated targets.

Injected permission errors, read races, hostile file replacement, multiple users, direct private-library loading in Dolphin, and Synology downloads are outside this task.

## Scenario Proof

Apply [Scenario discrimination](</home/mbeutler/.agents/skills/design-preflight/references/scenario-discrimination.md>), [Owner composition](</home/mbeutler/.agents/skills/design-preflight/references/owner-composition.md>), and [Full-set results](</home/mbeutler/.agents/skills/design-preflight/references/full-set-results.md>).

| Boundary | Discriminator and contrast | Required result and runnable path |
| --- | --- | --- |
| B1 Metadata owner | Top-level INFO major 4 versus disagreeing deeper INFO files and unusable package metadata. | One-shot returns `synced\n` with exit 0. Stream returns `synced\0`. A release build uses the literal top-level path. |
| B2 Metadata validity | Canonical INFO versus missing, malformed, ambiguous, or later-duplicate metadata. | One-shot returns exit 1, empty stdout, and a diagnostic. Stream returns `error\0`. The parser scans through EOF. |
| B3 Supported family | Major 4 with changed minor, mini, and build values versus major 3 or 5. | Major 4 reaches the helper. Other majors reach the existing failure terminals. |
| B4 Overlay ABI | ABI 15 versus ABI 14 with all other inputs fixed. | ABI 15 reaches the helper. ABI 14 fails through both CLI paths. |
| B5 Runtime | Exact `libnautilus-extension.so.4` versus a missing runtime. | The expected runtime reaches the helper. A missing runtime fails through both CLI paths. |
| B6 Helper | Exact `15/lib/plugin-cb-4.so` versus that file absent. | The expected helper returns a status. Its absence fails through both CLI paths. |
| B7 Symbols | Both exact symbols versus one missing symbol. | The complete helper returns a status. The incomplete helper fails through both CLI paths. |
| B8 Structure | Keep `enable` fixed and vary the second `file_status` field. | Status output follows the second integer in one-shot and stream modes. |
| B9 Status range | Raw values 0 through 5 versus raw 6. | Values 0 through 5 return the six existing names. Raw 6 reaches the existing failure terminals. |

The real plugin-to-CLI composition and distinct child-process checks remain direct regressions for opaque inherited behavior.

## Plan of Work

Implement one semantic subtask, `SDD-003-A`. Its observable outcome is acceptance of internal major 4 through the existing CLI and plugin paths. Its primary owner is `src/synodrive-status.cpp`.

Replace `installedVersion` with a small parser for the top-level INFO file. The parser trims ASCII whitespace, accepts blank and comment-only lines, and validates every remaining line. A normal line is a nonempty section header or a nonempty key followed by `=` inside a section. Unknown valid keys and sections do not affect support.

Require one case-sensitive `[Version]` section and one case-sensitive `major_version` key. Parse the complete value as unsigned decimal. Accept leading zeros. Reject signs, empty values, trailing data, overflow, duplicates, malformed input, read errors, and all numeric values other than 4. Do not return success before EOF.

Use the fixed production path `/opt/Synology/SynologyDrive/INFO`. Under `SYNODRIVE_STATUS_TESTING` only, use `SYNODRIVE_STATUS_TEST_INFO` as the complete fixture path. Remove the shell command and exact package-version constant.

Update the CLI fixture to write complete INFO variants and exercise B1 through B9 in both modes. Pass the release binary to the same test. In disposable root-owned CI containers, exercise the literal production INFO path with disagreeing deeper files and a fake `.so.4` runtime through `LD_LIBRARY_PATH`. Add an explicit missing-helper case.

Update the plugin fixture to provide a complete INFO file through the test-only path. Do not change provider or plugin production code.

Update current user documentation to describe public Drive 8.x, internal major 4, INFO troubleshooting, ABI 15, and private-API risk. Keep exact `8.0.2-17889` statements only where they describe historical evidence.

The checkpoint review boundary is the complete SDD-003 candidate and its direct interaction with the inherited CLI and plugin contracts. Do not create a checkpoint commit before focused validation and clean review.

## Concrete Steps

From the repository root, configure and validate the fast loop:

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ctest --test-dir build --output-on-failure

Run the complete distribution gate:

    ./ci/run

Run repository checks:

    git diff --check
    rg -n '8\.0\.2-17889|dpkg-query|SYNODRIVE_STATUS_TEST_VERSION' README.md docs src tests CMakeLists.txt

Classify each documentation match as current policy or historical evidence. Remove all stale current-policy matches.

## Validation and Acceptance

The CLI test must prove every B1 through B9 contrast through one-shot and stream modes. The plugin test must prove that a supported major-4 fixture still produces the existing asynchronous overlay result.

The release binary in each disposable distribution container must read the literal top-level INFO path. A deeper major-4 file cannot override a top-level unsupported major. A deeper unsupported major cannot override a top-level major 4.

The release binary must contain the exact top-level INFO path and `.so.4` runtime name. It must not contain `dpkg-query`, the test INFO variable, deeper INFO paths, or an alternate runtime name.

On the development host, run the release binary against the installed client. The recorded synced-root path must return `synced`. An outside-root path must return `unknown`.

After documentation is final, run fresh trace closure for B1 through B9. Then run native correctness review against the staged tree and task base. Apply review closure only after the final review is clean.

## Idempotence and Recovery

Builds and fixture tests are repeatable. The release-path fixture writes `/opt/Synology/SynologyDrive` only in a marked disposable container. It must stop if that path already exists.

If a command fails, stop and record the error in `.agent/test-results/SDD-003.md`. Do not retry automatically or modify unrelated system state.

## Interfaces and Dependencies

The public CLI and stream interfaces do not change. One-shot compatibility errors remain exit 1 with empty stdout and a diagnostic. Stream compatibility errors remain `error\0`.

The only new internal interface is `SYNODRIVE_STATUS_TEST_INFO` in the test build. The implementation uses C++17 standard-library file and numeric parsing. It adds no library dependency.
