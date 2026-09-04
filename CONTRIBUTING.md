# Contributing

## Report Fedora results

Debian 13 and Fedora 44 packages pass automated package checks. Synology does not publish a Fedora client. If you use a community package, report runtime results or submit a focused pull request.

## Submit a pull request

1. Run the relevant focused tests.
2. Open the pull request as a draft.
3. Request `@codex review`.
4. Resolve all review findings while the pull request is a draft.
5. Run `./ci/run` locally only after the reviews are clean.
6. Mark the pull request ready. This action starts the required GitHub CI check.
7. Merge only after `distribution-gate` passes.

Return the pull request to draft before you push a correction. Repeat review before you mark it ready again. A branch push or draft pull request does not run GitHub CI.

Reviewers inspect source, diffs, and existing test evidence. Reviewers do not run builds, tests, package validation, or CI.
