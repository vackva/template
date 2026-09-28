# Agentic C++ template

An example C++20 repo set up for developing with Claude Code: CLAUDE.md files, hooks, a
plugin, CMake presets, GoogleTest, sanitizers, clang-format/clang-tidy, CI and Codecov.

It follows the setup of tanh-lab's projects ([anira](https://github.com/anira-project/anira),
tanh-lib, cosmos), with one difference: **everything lives in this repo**.
[tanh-tooling](https://github.com/tanh-lab/tanh-tooling) is vendored into `tooling/`,
and [tanh-lab/ci-actions](https://github.com/tanh-lab/ci-actions) into `.github/`. Nothing is
fetched from a pinned tag, so the whole setup can be read, changed and tested in one place.

The library is small on purpose: `tpl::dsp::Gain` (a smoothed gain) and
`tpl::dsp::OnePoleLowpass`, both real-time safe.

## Quick start

```sh
brew install cmake ninja llvm just jq   # or the apt equivalents; clang-format/clang-tidy come with llvm
just setup                              # arms the pre-push hook, configures build/desktop/Debug
just test
claude                                  # trust the folder; then: /plugin install tanh-tools@tanh-tooling
```

## How the pieces fit

```
you / Claude edit a file
  │
  ├─ PreToolUse   .claude/hooks/protect-installed.sh   refuses edits to installed tooling copies
  ├─ PostToolUse  tanh-tools plugin: check.sh          clang-tidy, then clang-format, on that file
  ├─ Stop         .claude/hooks/build-and-test.sh      builds + runs ctest; red means Claude keeps working
  │
git push
  └─ hooks/tanh/pre-push                               clang-format + clang-tidy on the pushed diff
  │
CI (.github/workflows)
  ├─ lint         clang-format; installed copies == tooling/
  ├─ build_test   Linux/macOS/Windows × shared/static/gcc (PRs: a subset)
  ├─ sanitizers   ASan+UBSan, TSan, RTSan
  ├─ clang_tidy   changed files on PRs, full sweep on main
  ├─ coverage     llvm-cov → Codecov (a metric, never a gate)
  └─ tooling      tests of the CMake modules in tooling/cmake on 3 OSes
```

### Claude Code

| File | Role |
|---|---|
| `CLAUDE.md` | commands, layout, rules (naming, real-time safety, symbol policy), what runs automatically |
| `test/CLAUDE.md`, `tooling/CLAUDE.md`, `.github/CLAUDE.md` | loaded when Claude works in that directory |
| `.claude/settings.json` | local plugin marketplace (`./tooling`), enabled plugins, permissions, project hooks |
| `tooling/plugins/tanh-tools/` | the plugin: format/lint hooks, `dsp-reviewer` agent, `crossplatform-audio` skill, GitHub MCP, clangd LSP |
| `.claude/skills/add-processor/` | project skill: add a processor end to end (header, source, tests, CMake, changelog, review) |

The GitHub MCP server in the plugin reads `GITHUB_TOKEN` from the environment
(`export GITHUB_TOKEN="$(gh auth token)"`). See the tanh-tooling README for details.

### CMake

- `CMakePresets.json`: `desktop-debug` (dev; its `compile_commands.json` feeds clangd and the
  hooks), `ci-tests-{shared,static,gcc}`, `windows-msvc-tests-shared`, `ci-tests-coverage`,
  `desktop-tests-{asan,tsan,rtsan}`.
- `cmake/tanh/` (installed from `tooling/cmake/`): git-tag versioning, platform detection,
  symbol policy with an export allowlist, sanitizers, GoogleTest fetch.
- Shared builds export only `tpl::` symbols; the `tpl_dsp_exports` CTest checks the real
  export table.

### Codecov

`coverage.yml` uploads through tokenless OIDC. To turn it on: install the
[Codecov GitHub App](https://github.com/apps/codecov) on the org/repo. `codecov.yml` turns
status checks and PR comments off.

### Branch protection

Every workflow ends in a single `<name> result` job. Require those:

```sh
sh tooling/github/apply-merge-queue.sh <owner>/<repo> \
  "lint result" "build_test result" "sanitizer result" "clang_tidy result" "coverage result"
```

## Changing the shared tooling

Edit `tooling/`, never the installed copies (`.clang-*`, `cmake/tanh/`, `hooks/tanh/`), then
run `just tooling-install`. `just tooling-check` (and CI) fails on drift. See `tooling/CLAUDE.md`.
