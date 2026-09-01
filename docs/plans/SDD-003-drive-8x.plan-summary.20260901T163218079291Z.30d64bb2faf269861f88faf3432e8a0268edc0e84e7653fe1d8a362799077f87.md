<proposed_plan>
# SDD-003 — Support Synology Drive 8.x

## Summary

- Make SDD-003 the only Active roadmap task when implementation starts. Create `docs/plans/SDD-003-drive-8x.md` with the frozen boundaries below.
- Replace the Debian package-version check with the installed client’s top-level `INFO` metadata.
- Accept internal client major `4`, regardless of minor, mini, or build values.
- Preserve the existing CLI, stream protocol, Dolphin plugin, process isolation, ABI 15, libraries, symbols, structure, and status mappings.
- After validation and review, move SDD-003 to Completed. Leave SDD-004 first in Planned next.

Prior plan reconciliation:

- The original exact-version and no-plugin limits are superseded.
- The accepted production roadmap carries the SDD-003 major-4 policy.
- Later CI and release-plan changes did not alter SDD-003.
- Keep the completed status-overlay ExecPlan historical. The new SDD-003 ExecPlan records the exact-version supersession.

## Implementation and Interfaces

- Change the compatibility owner in `src/synodrive-status.cpp`.
  - Read `/opt/Synology/SynologyDrive/INFO` with C++17 standard-library code.
  - Remove `dpkg-query`, the exact `8.0.2-17889` constant, and the raw version test override.
  - Add no package-manager adapter, INI dependency, fallback source, cache, or semantic-version abstraction.
  - Under `SYNODRIVE_STATUS_TESTING` only, allow `SYNODRIVE_STATUS_TEST_INFO` to select a complete fixture file. Compile this override out of the release binary.

- Parse the complete file before accepting it.
  - Trim ASCII whitespace and CRLF.
  - Permit blank lines and comment-only lines beginning with `#` or `;`.
  - Require all other lines to be a valid nonempty section header or a nonempty `key=value` inside a section.
  - Ignore well-formed unrelated sections and keys.
  - Require exactly one case-sensitive `[Version]` section and one case-sensitive `major_version` key.
  - Parse the complete value as unsigned decimal. Permit leading zeros, but reject signs, trailing data, empty values, and overflow.
  - Accept only numeric value `4`.
  - Reject missing or unreadable files, malformed lines anywhere, duplicate `[Version]` sections, duplicate major keys, and other majors.
  - Scan through EOF before success so a valid prefix cannot hide a later duplicate.

- Leave these private compatibility gates unchanged:
  - `current` resolves to ABI directory `15`.
  - `libnautilus-extension.so.4` loads.
  - `15/lib/plugin-cb-4.so` exists and loads.
  - Both observed mangled symbols resolve.
  - `IconOverlayInfo` remains two ordered `int` fields.
  - Only raw statuses `0` through `5` are accepted.
  - Each stream request uses a fresh query child, and Dolphin never loads Synology code.

Public interfaces do not change. One-shot compatibility failures remain exit `1` with empty stdout and a diagnostic. Stream failures remain `error\0`. The only new interface is the test-build-only INFO path variable.

## Boundary and Scenario Proof

| Boundary | Required contrast and terminal proof |
| --- | --- |
| B1 — Metadata owner | Top-level INFO major 4 succeeds while deeper client/overlay metadata disagrees; neither `dpkg-query` nor another INFO file controls the result. Prove one-shot and stream paths. |
| B2 — Metadata validity | Canonical metadata succeeds; missing files, malformed lines, missing fields, invalid numbers, and later duplicates fail through both paths. Full-file uniqueness rules apply. |
| B3 — Supported family | Major 4 with changed minor/mini/build values succeeds; majors 3 and 5 fail. This must reject an exact-build implementation. |
| B4 — Overlay ABI | ABI 15 succeeds; ABI 14 fails with all other inputs fixed. |
| B5 — Runtime identity | `libnautilus-extension.so.4` succeeds; a missing expected runtime fails. |
| B6 — Helper identity | The exact ABI-15 helper succeeds; removing only `plugin-cb-4.so` fails. |
| B7 — Symbols | The complete helper succeeds; an otherwise loadable helper missing one required symbol fails. |
| B8 — Structure | Hold `enable` constant and vary the second `file_status` field; results must follow the second field. |
| B9 — Status range | Values 0–5 map to the six existing names; raw 6 fails closed. |

Testing changes:

- Replace raw version strings in the CLI and plugin fixtures with complete INFO files.
- Parameterize the CLI test so B1–B9 run through both one-shot and `--stdio` entry points without a Cartesian product.
- Add the currently missing isolated helper-absent test.
- Retain the real plugin-to-CLI composition and distinct-query-child checks as opaque regressions. Do not modify plugin production code.
- Add a release-binary check for disposable Ubuntu and Fedora containers:
  - Use the literal `/opt/Synology/SynologyDrive/INFO` path without the test override.
  - Pair top-level major 4/deeper major 3 with top-level major 3/deeper major 4.
  - Supply only a fake `libnautilus-extension.so.4` through `LD_LIBRARY_PATH`.
  - Exercise one-shot and stream terminals.
- Inspect the release binary for the exact top-level INFO path and `.so.4` runtime. Reject `dpkg-query`, test-only variables, deeper INFO paths, and alternate runtime names.
- Run a production-binary host smoke against the installed major-4 metadata, the existing synced-root path, and an outside-root path. A new live transition is not required.

Validation commands:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
./ci/run
git diff --check
```

Also classify remaining `8.0.2-17889` and `dpkg-query` documentation matches as either intentional history or stale current policy. Complete fresh B1–B9 trace closure and correctness review before marking the roadmap task complete.

## Documentation, Research, and Assumptions

- Update the README, installation guide, usage guide, and current-policy portions of the reconnaissance report.
  - Describe support as public Synology Drive 8.x through internal major 4.
  - Keep ABI 15 and private-API limitations prominent.
  - Replace the Debian-only troubleshooting command with inspection of the top-level INFO file.
  - Preserve exact `8.0.2-17889` statements only where they describe historical evidence or SDD-001.

Research selected the top-level INFO file over the deeper cloudstation copy, overlay ABI JSON, and distribution package-manager queries. The current community [Fedora package specification](https://github.com/EmixamPP/synology-drive/blob/main/synology-drive.spec) copies the complete `/opt/Synology/SynologyDrive` tree, so the same path is viable for both accepted distributions. Synology’s [client release notes](https://www.synology.com/en-global/releaseNote/SynologyDriveClient) do not establish a public INFO schema; therefore, this remains a private compatibility boundary.

Assumptions and exclusions:

- Confidence is high for the current Ubuntu installation and accepted Fedora repack.
- INFO path or format drift intentionally fails closed.
- Silent private structure changes that preserve ABI and symbol identities remain accepted residual risk.
- Permission injection, read races, hostile replacement, future major 5, packaging, context menus, new diagnostics, and plugin redesign are outside SDD-003.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
<!-- cpk-plan-spec: docs/plans/SDD-003-drive-8x.md -->
</proposed_plan>