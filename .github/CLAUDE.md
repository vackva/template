# .github/ — CI

Two layers, both in this repo:

- **Callers** (`workflows/<name>.yml`): the repo's facts — triggers, matrix files, presets.
- **Logic** (vendored from [tanh-lab/ci-actions](https://github.com/tanh-lab/ci-actions) v0.3.15):
  composite actions in `actions/` and reusable workflows `workflows/reusable-*.yml`, referenced
  as `./.github/actions/<name>` and `./.github/workflows/reusable-<name>.yml`. They run from
  the same commit as the caller, so there is no version pin to bump.

| Workflow | Runs | Required context |
|---|---|---|
| `lint.yml` | clang-format over sources; installed tooling copies == `tooling/` | `lint result` |
| `build_test.yml` | `build_test_matrix.json` legs; PRs run rows with `"pr": true` | `build_test result` |
| `sanitizers.yml` | `sanitizer_matrix.json` (ASan+UBSan, TSan, RTSan) | `sanitizer result` |
| `clang_tidy.yml` | clang-tidy; PRs check changed `.cpp` only, main sweeps | `clang_tidy result` |
| `coverage.yml` | instrumented build → lcov → Codecov; PRs: every changed `src/`/`include/` file >= 80% (`scripts/coverage_gate.py`) | `coverage result` |
| `tooling.yml` | `tooling/cmake/test` on Linux/macOS/Windows, installer round trip | (path-filtered, advisory) |
| `on_tag.yml` | tag `vX.Y.Z`: all of the above in full, then install packages per platform → GitHub Release | — |

Conventions:
- Every workflow ends in one `<name> result` job — the only context branch protection requires
  (matrix leg names change with tiers). `rulesets/main.json` is the ruleset on `main` (PRs only,
  those five checks, branch up to date, no bypass); apply changes with `just protect-main <owner/repo>`.
  A merge queue needs an org-owned repo: `tooling/github/apply-merge-queue.sh`.
- Every check workflow also has `workflow_call`, so `on_tag.yml` reuses it unchanged.
- A matrix row names a preset from `CMakePresets.json`; add the preset before the row.
- Codecov: `codecov.yml` reports patch coverage (target 80%) and a PR comment; the hard per-file
  gate is CI's own. Upload is tokenless OIDC (`id-token: write`, the Codecov GitHub App installed).
- Validate workflow edits with `actionlint` if available.
