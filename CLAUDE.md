# CLAUDE.md

Example C++20 library (`tpl_dsp`, namespace `tpl::dsp`: a smoothed gain and a one-pole
low-pass) whose real purpose is the setup around it: how Claude Code works in a tanh-lab
C++ repo — CLAUDE.md files, hooks, a plugin, CMake presets, GoogleTest, sanitizers, CI and
Codecov. Everything is in this repo; nothing is fetched from tanh-tooling or ci-actions.

## Commands

```bash
just build                 # cmake --preset desktop-debug && cmake --build --preset desktop-debug
just test                  # build + ctest --preset desktop-debug
just test-filter 'Gain.*'  # one suite / one test
just format / format-check # clang-format over src include test
just tidy                  # clang-tidy over src test (needs a configured desktop-debug)
just sanitize asan         # asan (+ubsan) | tsan | rtsan presets
just coverage              # instrumented build + llvm-cov report
just tooling-check         # installed tooling copies match tooling/
```

Single test binary: `./build/desktop/Debug/test/test_dsp --gtest_filter='OnePoleLowpass.*'`

## Layout

| Path | What |
|---|---|
| `include/tpl/`, `src/` | the library; `include/tpl/Exports.h` holds `TPL_API` and `TPL_NONBLOCKING` |
| `test/` | GoogleTest suites (own `CLAUDE.md`) |
| `tooling/` | in-repo tanh-tooling: clang configs, CMake modules, git hook, Claude plugin (own `CLAUDE.md`) |
| `.clang-*`, `cmake/tanh/`, `hooks/tanh/` | **installed copies** of `tooling/` — never edit (a hook blocks it) |
| `.github/` | CI: callers, in-repo ci-actions (own `CLAUDE.md`) |
| `.claude/` | settings, project hooks, the `add-processor` skill |

## Rules

- **Naming** (enforced by `.clang-tidy`): `PascalCase` types, `snake_case` functions and
  variables, `m_` members, `k_` constants, `PascalCase` enum values. Files: `PascalCase.h/.cpp`.
- **Real-time safety**: `process()` is `noexcept TPL_NONBLOCKING` and must not allocate, lock,
  log, throw or make syscalls. RealtimeSanitizer enforces it in the `rtsan` preset.
- **Symbol policy**: the library builds with hidden visibility. Every public class or free
  function needs `TPL_API`, or shared builds fail to link. The `tpl_dsp_exports` CTest fails
  if the shared library exports anything outside `tpl::`.
- **CMake**: branch on `TANH_OPERATING_SYSTEM` / `TANH_BINARY_FORMAT`, never `APPLE`/`UNIX`/`WIN32`.
  New presets go in `CMakePresets.json` with matching build and test presets.
- **Every change comes with tests** in `test/`, and an entry in `CHANGELOG.md` under `[Unreleased]`
  when behaviour or API changes.

## What runs automatically

- After every Edit/Write (tanh-tools plugin, `tooling/plugins/tanh-tools/hooks/check.sh`):
  clang-tidy (any finding blocks and is shown to you) then clang-format on the file. The tidy
  step needs `build/desktop/Debug/compile_commands.json`; run `just build` once per checkout.
- Before an edit (`.claude/hooks/protect-installed.sh`): edits to installed tooling copies are refused.
- On stop (`.claude/hooks/build-and-test.sh`): with C++/CMake changes in the tree, desktop-debug
  is built and tested; a failure sends you back to fix it (3 attempts max).
- On `git push` (`hooks/tanh/pre-push`, armed by `just setup`): clang-format + clang-tidy on the pushed diff.
