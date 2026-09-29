# CLAUDE.md

Example C++20 library (`tpl_dsp`, namespace `tpl::dsp`: a smoothed gain and a one-pole
low-pass; `tpl_stt`, namespace `tpl::stt`: offline speech-to-text with Parakeet over anira's
ONNX Runtime, see `docs/stt.md`) whose real purpose is the setup around it: how Claude Code works in a tanh-lab
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
just model                 # git lfs pull the speech-to-text model
```

Single test binary: `./build/desktop/Debug/test/test_dsp --gtest_filter='OnePoleLowpass.*'`

## Layout

| Path | What |
|---|---|
| `include/tpl/`, `src/` | the library; `include/tpl/Exports.h` holds `TPL_API` and `TPL_NONBLOCKING` |
| `test/` | GoogleTest suites, `test_<component>` executables; `test/data/stt/` clips + golden transcripts |
| `models/` | exported Parakeet model and Silero VAD; `*.onnx` in Git LFS, `manifest.json` keys the CI cache |
| `scripts/export-parakeet/` | uv script: NeMo -> int8 ONNX export, golden transcripts |
| `third_party/anira` | anira v2.3.0 submodule (ONNX Runtime) — added before `cmake/tanh`, see `cmake/anira.cmake` |
| `examples/` | `tpl-transcribe` |
| `include/tpl/transcript/`, `src/transcript/` | `tpl_transcript`: SQLite store, recording service, export, UI layout logic (no JUCE) |
| `apps/transcriber/` | JUCE 9 plugin + standalone (`TPL_WITH_APP`, on in the desktop presets); see `docs/transcriber-app.md` |
| `tooling/` | in-repo tanh-tooling: clang configs, CMake modules, git hook, Claude plugin |
| `.clang-*`, `cmake/tanh/`, `hooks/tanh/` | **installed copies** of `tooling/` — never edit (a hook blocks it) |
| `.github/` | CI: caller workflows, vendored ci-actions (`actions/`, `reusable-*.yml`), matrices, ruleset |
| `.claude/` | settings, project hooks, the `add-processor` skill |
| `docs/` | one page per config (presets, clang tools, hooks, CI, …) — update it when a config changes |

## Rules

- **`main` only changes through pull requests** (GitHub ruleset, no bypass): work on a branch,
  push it, open a PR. Required: lint, build_test, sanitizers, clang_tidy, coverage — and every
  changed file under `src/`/`include/` needs >= 80% line coverage, so new code comes with tests.
- **Naming** (enforced by `.clang-tidy`): `PascalCase` types, `snake_case` functions and
  variables, `m_` members, `k_` constants, `PascalCase` enum values. Files: `PascalCase.h/.cpp`.
- **Real-time safety**: `process()` is `noexcept TPL_NONBLOCKING` and must not allocate, lock,
  log, throw or make syscalls. RealtimeSanitizer enforces it in the `rtsan` preset.
- **`tpl_stt` is not real-time safe**: loading and `transcribe()` allocate and take up to seconds;
  never call them from an audio callback. Its tests need the Git LFS model (`just model`).
  The one real-time entry point is `SegmentSource::push_audio()` (resample + lock-free ring).
- **JUCE strings**: non-ASCII literals go through `theme::text()` (`juce::String::fromUTF8`);
  `juce::String(const char*)` asserts on anything but ASCII.
- **Sanitizers on macOS**: use Homebrew LLVM (see `docs/stt.md`); Apple clang 17's ASan/TSan
  runtimes hang at start-up on macOS 26.6.
- **Symbol policy**: the library builds with hidden visibility. Every public class or free
  function needs `TPL_API`, or shared builds fail to link. The `tpl_dsp_exports` CTest fails
  if the shared library exports anything outside `tpl::`.
- **CMake**: branch on `TANH_OPERATING_SYSTEM` / `TANH_BINARY_FORMAT`, never `APPLE`/`UNIX`/`WIN32`.
  New presets go in `CMakePresets.json` with matching build and test presets.
- **Every change comes with tests** in `test/`, and an entry in `CHANGELOG.md` under `[Unreleased]`
  when behaviour or API changes.

## Tests

- One executable per component, `test_<component>`, registered with `tpl_add_test()` in
  `test/CMakeLists.txt`; one file per class, `test/<component>/test_<Class>.cpp`, inside an
  anonymous namespace. The `test_` name is load-bearing (the coverage job collects it).
- Test behaviour, not implementation: measured responses, exact endpoints, edge cases (zero,
  negative, above Nyquist, infinities) and `reset()`. `EXPECT_NEAR` with a stated tolerance.
- Deterministic and parallel-safe: no sleeps, no unseeded randomness, no files outside the build
  tree — the suite also runs under ASan, TSan and RTSan.
- GoogleTest comes from `tanh_fetch_googletest()`; do not add another FetchContent for it.

## Tooling and CI

- `tooling/` is the source of truth for `.clang-*`, `cmake/tanh/`, `hooks/tanh/`: edit there, run
  `sh tooling/install.sh <clang|cmake|hooks>`, commit source and copy together. CMake module
  changes: `just tooling-test`. Plugin changes: bump `version` in its `plugin.json`.
- CI callers (`.github/workflows/<name>.yml`) hold only triggers, matrix and presets; the logic is
  in `.github/actions/` and `reusable-*.yml`, referenced as `./.github/...`. Every workflow ends in
  a `<name> result` job — those are the required checks. A matrix row names a preset; add the
  preset first.
- Details for humans live in `docs/` (one page per config).

## What runs automatically

- After every Edit/Write (tanh-tools plugin, `tooling/plugins/tanh-tools/hooks/check.sh`):
  clang-tidy (any finding blocks and is shown to you) then clang-format on the file. The tidy
  step needs `build/desktop/Debug/compile_commands.json`; run `just build` once per checkout.
- Before an edit (`.claude/hooks/protect-installed.sh`): edits to installed tooling copies are refused.
- On stop (`.claude/hooks/build-and-test.sh`): with C++/CMake changes in the tree, desktop-debug
  is built and tested; a failure sends you back to fix it (3 attempts max).
- On `git push` (`hooks/tanh/pre-push`, armed by `just setup`): clang-format + clang-tidy on the pushed diff.
