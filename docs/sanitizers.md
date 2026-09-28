# Sanitizers

Files: `desktop-tests-*` presets in [`CMakePresets.json`](../CMakePresets.json),
`cmake/tanh/sanitizers.cmake`, [`.github/sanitizer_matrix.json`](../.github/sanitizer_matrix.json).

A sanitizer is compiler instrumentation that turns a class of silent bug into a crash with a
report. The test suite is the input: whatever the tests execute gets checked.

| Preset | Sanitizers | Finds |
|---|---|---|
| `desktop-tests-asan` | ASan + UBSan | out-of-bounds, use-after-free, leaks; signed overflow, bad shifts, misaligned access |
| `desktop-tests-tsan` | TSan | data races between threads |
| `desktop-tests-rtsan` | RTSan | allocation, locks and blocking syscalls inside real-time functions |

```sh
just sanitize asan      # = cmake/ctest with --preset desktop-tests-asan
just sanitize rtsan
```

## How it is wired

The preset sets one cache variable:

```json
{ "name": "desktop-tests-asan", "cacheVariables": { "TPL_SANITIZERS": "asan;ubsan" } }
```

`CMakeLists.txt` applies each entry to the library:

```cmake
foreach(_san IN LISTS TPL_SANITIZERS)
    string(TOUPPER "${_san}" _san_upper)
    tanh_add_sanitizer(tpl_dsp ${_san} DEFINE TPL_WITH_${_san_upper})
endforeach()
```

`tanh_add_sanitizer` adds `-fsanitize=…` to compile **and** link options as `PUBLIC`, so every
test executable linking `tpl_dsp` gets the runtime too, and defines `TPL_WITH_ASAN` etc. for the
code.

## RealtimeSanitizer and `TPL_NONBLOCKING`

RTSan only checks functions marked `[[clang::nonblocking]]`. The library spells that
`TPL_NONBLOCKING` (in `include/tpl/Exports.h`), active only when `TPL_WITH_RTSAN` is defined:

```cpp
void process(std::span<float> block) noexcept TPL_NONBLOCKING;   // after noexcept, on declaration and definition
```

If `process()` (or anything it calls) allocates, locks or makes a blocking syscall during a
test, the test aborts with a stack trace. That is the real-time rule from `CLAUDE.md`, enforced
at runtime. RTSan needs Clang 20 or newer.

## In CI

`sanitizers.yml` runs the three presets on Linux with LLVM clang 20. The environment makes
findings fatal:

```
ASAN_OPTIONS=detect_leaks=1
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1     # UBSan would otherwise print and carry on
TSAN_OPTIONS=halt_on_error=1:second_deadlock_stack=1
```

## Locally on macOS

The presets use `clang++` from `PATH`, which on a Mac is usually Apple clang. Apple clang has
no RTSan (and an ASan build hung on the machine this template was made on). Put Homebrew LLVM first
(`export PATH="$(brew --prefix llvm)/bin:$PATH"`) or rely on CI.
