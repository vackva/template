# Tests

Files: [`test/CMakeLists.txt`](../test/CMakeLists.txt), `test/dsp/test_*.cpp`,
the "Tests" section of [`CLAUDE.md`](../CLAUDE.md) (the conventions Claude follows).

## GoogleTest through CTest

```cmake
include(${PROJECT_SOURCE_DIR}/cmake/tanh/test-deps.cmake)
tanh_fetch_googletest()                      # FetchContent, pinned version, warnings off

tpl_add_test(dsp                             # -> executable test_dsp in <build>/test/
    SOURCES
        dsp/test_SmoothedValue.cpp
        dsp/test_Gain.cpp
        dsp/test_OnePoleLowpass.cpp
    LIBS tpl::dsp)
```

`tpl_add_test` links `LIBS` and `GTest::gtest_main` and calls `gtest_discover_tests`, so
every `TEST()` is its own CTest entry. `test_stt` (speech-to-text) needs the model from Git
LFS, see [stt.md](stt.md#tests).

```sh
ctest --preset desktop-debug                 # everything
ctest --preset desktop-debug -R 'Gain\.'     # by name (regex)
./build/desktop/Debug/test/test_dsp --gtest_filter='OnePoleLowpass.*'
```

The `test_` prefix and the `<build>/test/` location matter: the coverage job collects the
binaries by that name ([coverage.md](coverage.md)).

## What a good test here looks like

- Measures behaviour: the gain of the filter at 20 Hz and at 5 kHz, a ramp that rises every
  sample and ends exactly on target. "Does not crash" is not a test.
- Covers edges: zero, negative, above Nyquist, `-INFINITY`.
- Deterministic: no sleeps, no unseeded randomness, no files outside the build tree. The same
  suite runs under ASan, TSan and RTSan and in parallel.

## Export check

For a shared build, `test/CMakeLists.txt` also registers `tpl_dsp_exports`:

```cmake
tanh_add_export_check(NAME tpl_dsp_exports LIBRARY tpl_dsp NAMESPACES tpl)
```

It reads the real export table of `libtpl_dsp` (`nm` / `dumpbin`) and fails if anything outside
`tpl::` is exported. It catches a missing `TPL_API` (then a symbol is absent) and leaked
dependencies (then foreign symbols appear).

## Who runs the tests

| When | What |
|---|---|
| Claude stops with C++ changes | Stop hook: build + `ctest --preset desktop-debug` ([hooks.md](hooks.md)) |
| `just test` | same, by hand |
| Pull request | `build_test` on Linux, macOS, Windows ([ci.md](ci.md)) |
| Pull request | the same suite under sanitizers and with coverage |
