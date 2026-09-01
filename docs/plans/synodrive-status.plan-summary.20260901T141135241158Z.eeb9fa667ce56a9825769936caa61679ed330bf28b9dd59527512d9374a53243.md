<proposed_plan>
# Roadmap to v1.0 Production Readiness

## Summary

Create `docs/roadmap.md` with permanent `SDD-NNN` task identifiers, and link it from the README. Keep the roadmap inactive until implementation starts; then activate one task at a time.

Production readiness means:

- Automated x86_64 builds and tests on Ubuntu 26.04 LTS and Fedora KDE 44 containers.
- Installable DEB and RPM release assets.
- Synology Drive Client 8.x compatibility with strict private-ABI checks.
- Automated package install and uninstall validation.
- Safe context-menu actions if the private API spike succeeds.
- Documented support boundaries, security reporting, and manual integration checks.

## Prior Plan Reconciliation

- Record: `.agent/plan-history/plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`
  - Status: superseded
  - Scope: The exact 8.0.2 version gate and exclusions for packaging, broader versions, and context menus.
  - Reason: The user explicitly requested those capabilities for production readiness.
  - Replacement: Preserve process isolation, status values 0–5, fail-closed behavior, and the ban on redistributing Synology binaries. Add the corrective roadmap work below.

- Record: `docs/engineering-task.plan-summary.20260901T045809241273Z.51b36e3d6aa8d69e9a469850388d30f92b0b84e6d6c1ef3bf054ead2e5f403b9.md`
  - Status: superseded
  - Scope: The same exclusions and exact version gate.
  - Reason: This is the identical sibling record of the superseded plan.
  - Replacement: The roadmap below becomes authoritative for unfinished production work.

`docs/engineering-task.md` remains historical. The completed overlay implementation in `docs/plans/synodrive-status.md` remains authoritative for its retained safety contracts.

## Roadmap Contents

### Active

None while this roadmap is being accepted.

### Planned next

| ID | Task | Completion gate |
|---|---|---|
| `SDD-002` | Add distro CI | GitHub Actions builds and runs `ctest --test-dir build --output-on-failure` in pinned `ubuntu:26.04` and `fedora:44` job containers on pushes and pull requests. CI uses the existing fake helper and never downloads Synology software. |
| `SDD-003` | Support Drive 8.x | Replace the Debian-only exact package-version check with the installed client’s internal `INFO` metadata. Accept internal major 4, corresponding to public Drive 8.x, only when icon-overlay ABI 15, the expected library, symbols, structure, and status range also match. Malformed metadata, other majors, ABI changes, or missing symbols continue to fail closed. |
| `SDD-004` | Produce DEB and RPM packages | Use CMake’s native [CPack DEB](https://cmake.org/cmake/help/latest/cpack_gen/deb.html) and [CPack RPM](https://cmake.org/cmake/help/latest/cpack_gen/rpm.html) generators. Publish one `synodrive-dolphin` package per format for x86_64. Let the generators calculate shared-library dependencies. Do not bundle or require a repository-provided Synology package. |
| `SDD-005` | Test package lifecycle | In disposable Ubuntu and Fedora containers, install each package, verify the executable, plugin, license, metadata, permissions, and shared-library resolution, and then uninstall it. Confirm that no owned files remain and that no Synology binaries entered the package. Add an upgrade test after the first packaged release exists. |
| `SDD-006` | Automate tagged releases | For `vX.Y.Z` tags, require the tag and CMake project version to match, rerun all checks, build both packages, generate `SHA256SUMS`, create GitHub provenance attestations, and attach everything to the GitHub release. GitHub supports both [matrix jobs](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations) and [artifact attestations](https://docs.github.com/en/actions/how-tos/secure-your-work/use-artifact-attestations/use-artifact-attestations). Use minimum workflow permissions. |
| `SDD-007` | Prove context-menu integration | Build a bounded prototype against the installed private helper. It must list and invoke “Get link” and “Browse previous versions” from a separate process, handle one local selection, and return within 250 ms. No Synology code may load into Dolphin. Record the private ABI and discard the prototype if either action cannot be invoked reliably. |

### Planned later

| ID | Task | Completion gate |
|---|---|---|
| `SDD-008` | Ship safe context-menu actions | Proceed only if `SDD-007` succeeds. Add a native [KAbstractFileItemActionPlugin](https://api.kde.org/kabstractfileitemactionplugin.html) in the `kf6/kfileitemaction` namespace. Show only “Get link” and “Browse previous versions” for one eligible local file or directory. A timeout, unsupported selection, or helper failure produces no menu action and never blocks or crashes Dolphin. Include the plugin in both packages. |
| `SDD-009` | Complete the v1.0 gate | Update README, install, and usage documentation with the support matrix and package commands. Add a focused bug-report form and `SECURITY.md`; request Fedora runtime reports and encourage fixes through pull requests. Complete a real Ubuntu desktop smoke test covering overlay states, live transitions, Dolphin restart, context actions when available, and uninstall. Run final correctness and documentation reviews before tagging `v1.0.0`. |

### Accepted residual risk

- Synology exposes no supported Dolphin API. Private helper changes can break integration despite version and ABI checks.
- Containers prove builds and package lifecycle, not live overlays or account-backed actions.
- Synology officially supports Ubuntu Linux; Fedora relies on [community Synology Drive packaging](https://github.com/EmixamPP/synology-drive). RPM runtime support must be labeled accordingly until contributor reports establish a verified matrix.
- A failed context-menu spike does not block v1.0. Move `SDD-008` to Declined and document the limitation.

### Declined for v1.0

- Hosted APT or DNF repositories and project GPG key management.
- ARM packages.
- Selective-sync, lock, unlock, pause, or resume menu actions.
- Static service menus that appear for unrelated files.
- Loading Synology libraries directly into Dolphin.
- A new diagnostics command; the existing `synodrive-status <absolute-path>` command remains the troubleshooting interface.

### Completed

- `SDD-001`: Published the MIT-licensed `v0.1.0` proof of concept with isolated overlay status integration.

## Public Contracts and Acceptance

- Package name: `synodrive-dolphin`.
- Supported architecture: x86_64.
- Automated package targets: Ubuntu 26.04 LTS and Fedora KDE 44.
- Compatibility policy: public Synology Drive 8.x, represented internally as 4.x, with icon-overlay ABI 15 and exact symbol checks.
- Existing status CLI and NUL-framed `--stdio` protocol remain compatible.
- Release distribution: GitHub DEB/RPM assets, checksums, and attestations only.
- Production readiness is complete when `SDD-002` through `SDD-007` and `SDD-009` are complete, with `SDD-008` either completed or formally declined from the spike evidence.
- Each non-trivial roadmap task receives its own ExecPlan under `docs/plans/SDD-NNN-*.md` when activated.

<!-- cpk-plan-spec: docs/roadmap.md -->
<!-- cpk-plan-spec: docs/plans/synodrive-status.md -->
<!-- cpk-plan-spec: docs/engineering-task.md -->
</proposed_plan>