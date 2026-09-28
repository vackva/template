# CI

Files: `.github/workflows/`, `.github/actions/`, `.github/*_matrix.json`,
[`.github/rulesets/main.json`](../.github/rulesets/main.json).

## Two layers

- **Callers**, `workflows/<name>.yml`: this repo's facts. Triggers, matrix file, presets.
- **Logic**, `workflows/reusable-*.yml` and `actions/*`: vendored from
  [tanh-lab/ci-actions](https://github.com/tanh-lab/ci-actions). Referenced locally
  (`uses: ./.github/actions/cmake-build`), so they always run from the same commit as the caller.

| Action | Does |
|---|---|
| `setup-cpp-build-tools` | Linux: LLVM clang 20 + clang-format/tidy from apt.llvm.org, symlinked as `clang`; macOS/Windows: ninja (+ LLVM) |
| `cmake-build` | `cmake --preset X` + build, with sccache; configure output kept in `configure.log` |
| `cmake-test` | `ctest --preset X -j<cores>` |
| `clang-format-check` | `clang-format --dry-run --Werror` over directories |
| `clang-tidy-check` | clang-tidy with warnings as errors; optionally only a PR's changed files |
| `changed-files` | a PR's changed files from git (no token), or "sweep" when unsure |
| `preset-binary-dir` | a preset's `binaryDir`, following `inherits` |

## Workflows

| Workflow | Pull request | `main` / tag | Required check |
|---|---|---|---|
| `lint.yml` | clang-format; installed tooling copies == `tooling/` | same | `lint result` |
| `build_test.yml` | matrix rows with `"pr": true` | every row | `build_test result` |
| `sanitizers.yml` | ASan+UBSan, TSan, RTSan | same | `sanitizer result` |
| `clang_tidy.yml` | changed `.cpp` files | all files | `clang_tidy result` |
| `coverage.yml` | coverage + per-file 80% gate | coverage | `coverage result` |
| `tooling.yml` | only when `tooling/` changes | same | — |
| `on_tag.yml` | — | tag `vX.Y.Z`: everything, then release | — |

Every workflow ends in one `<name> result` job that is green only if all its jobs are. Branch
protection requires those, not the individual legs, because leg names change with the matrix.

## Matrices

[`build_test_matrix.json`](../.github/build_test_matrix.json), one row per leg:

```json
{ "name": "Windows-x86_64-shared", "os": "windows-2025", "preset": "windows-msvc-tests-shared", "pr": true, "vcvars": "x64" }
```

| Field | Meaning |
|---|---|
| `name` | job name; its prefix (`Linux`, `macOS`, `Windows`) picks the tool setup |
| `os` | runner |
| `preset` | configure/build/test preset ([cmake.md](cmake.md#presets)) |
| `pr` | `true`: also runs on pull requests; others only on `main`, tags, manual runs |
| `vcvars` | MSVC environment (Ninja needs `cl.exe` from a vcvars shell) |

[`sanitizer_matrix.json`](../.github/sanitizer_matrix.json): `{ name, preset }` per sanitizer.

## Branch protection

[`rulesets/main.json`](../.github/rulesets/main.json) on `main`:

- changes only through pull requests, merged by hand; no direct push, no force push, no deletion
- the five `<name> result` checks must pass, on a branch that is up to date with `main`
- review threads must be resolved; nobody can bypass, repo admins included

```sh
just protect-main <owner>/<repo>    # create or update the ruleset (gh, admin rights)
```

A merge queue (`tooling/github/apply-merge-queue.sh`) needs an organization-owned repo; the
workflows already listen to `merge_group`.

## Releases

```sh
git tag v0.1.0 && git push origin v0.1.0
```

`on_tag.yml` calls every check workflow in full (all matrix rows, clang-tidy over all files),
then builds install packages (Linux x86_64/arm64, macOS arm64, Windows x64; shared and static),
zips `cmake --install` of each, and attaches them to a GitHub Release. A tag with a `-`
(`v0.2.0-rc.1`) becomes a pre-release. The version inside comes from the tag
(`tanh_git_version`).
