# CMake and presets

Files: [`CMakeLists.txt`](../CMakeLists.txt), [`CMakePresets.json`](../CMakePresets.json),
[`cmake/install.cmake`](../cmake/install.cmake), `cmake/tanh/` (shared modules, see [tooling.md](tooling.md)).

## CMakeLists.txt

```cmake
include(cmake/tanh/git-version.cmake)
tanh_git_version(${CMAKE_CURRENT_SOURCE_DIR})      # before project(): version from the nearest vX.Y.Z tag
project(tpl VERSION ${TANH_VERSION_SHORT} LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)              # clangd, clang-tidy and the Claude hooks need it

include(cmake/tanh/platform.cmake)                 # TANH_OPERATING_SYSTEM, TANH_BINARY_FORMAT
include(cmake/tanh/apple.cmake)
tanh_apple_sysroot_from_xcrun()                    # macOS: -isysroot into the compile DB (Homebrew clang-tidy)

tanh_apply_symbol_policy(tpl_dsp EXPORT_PREFIX TPL) # hidden visibility, TPL_BUILDING / TPL_STATIC
tanh_set_export_allowlist(tpl_dsp NAMESPACE tpl)    # shared lib exports only tpl::
```

| Option | Default | Effect |
|---|---|---|
| `TPL_WITH_TESTS` | on when top-level | builds `test/` |
| `TPL_WITH_INSTALL` | on when top-level | install rules + `tpl` CMake package |
| `TPL_SANITIZERS` | empty | e.g. `asan;ubsan`, applied to `tpl_dsp` and everything linking it ([sanitizers.md](sanitizers.md)) |
| `BUILD_SHARED_LIBS` | off | shared or static `tpl_dsp` / `tpl_stt` |
| `TPL_WITH_STT` | on | `tpl_stt` and anira (`third_party/anira`, ONNX Runtime) ([stt.md](stt.md)) |
| `TPL_WITH_EXAMPLES` | on when top-level | `examples/` (`tpl-transcribe`) |
| `TPL_STT_MODEL_DIR` / `TPL_STT_MODEL_INSTALL_DIR` | `models/parakeet-…` / system-wide | model the tests use / where `--component stt_model` installs it |

Branch on `TANH_OPERATING_SYSTEM` / `TANH_BINARY_FORMAT`, not `APPLE` / `UNIX` / `WIN32`:
`APPLE` is true for macOS and iOS, `UNIX` for Linux, macOS and Android. Most linker questions
depend on the binary format (ELF, Mach-O, PE), not the OS.

## Presets

A preset is a named configuration: generator, compiler, build directory, cache variables.
CI, IDEs, `just` and the hooks all use the same names, so "it works in CI" and "it works
locally" mean the same build.

```sh
cmake --preset desktop-debug          # configure
cmake --build --preset desktop-debug  # build
ctest --preset desktop-debug          # test
```

| Preset | Build dir | Used by | What it is |
|---|---|---|---|
| `desktop-debug` | `build/desktop/Debug` | you, clangd, Claude hooks, CI clang-tidy | Debug. Its `compile_commands.json` is what `.clangd` and the lint hook read, so keep the path. |
| `desktop-release` | `build/desktop/Release` | you | Release |
| `ci-tests-shared` / `ci-tests-static` | `build/ci/{shared,static}` | CI build_test, on_tag | Release, shared / static lib |
| `ci-tests-gcc` | `build/ci/gcc` | CI build_test | GCC instead of clang |
| `windows-msvc-tests-{shared,static}` | `build/ci/msvc-*` | CI build_test, on_tag | MSVC `cl` under Ninja (needs a vcvars shell) |
| `ci-tests-coverage` | `build/ci/coverage` | CI coverage, `just coverage` | `-fprofile-instr-generate -fcoverage-mapping` ([coverage.md](coverage.md)) |
| `desktop-tests-{asan,tsan,rtsan}` | `build/sanitizers/*` | CI sanitizers, `just sanitize` | sets `TPL_SANITIZERS` ([sanitizers.md](sanitizers.md)) |

All inherit the hidden `base` preset (Ninja, `clang`/`clang++`, compile DB on). Each configure
preset has a build and a test preset of the same name; the hidden `test-base` turns on
`outputOnFailure` and fails when no test is found.

`clang++` is whatever comes first on `PATH`: LLVM clang 20 on the CI runners, Apple clang on a
Mac unless Homebrew LLVM is first.

### Adding a preset

Add the configure, build and test preset together; CI matrix rows ([ci.md](ci.md)) refer to
presets by name.

## Install and package

`cmake/install.cmake` installs the headers, `tpl_dsp` and a relocatable package:

```cmake
find_package(tpl CONFIG REQUIRED)
target_link_libraries(app PRIVATE tpl::dsp)
```

```sh
cmake --install build/ci/shared --prefix /tmp/tpl
cmake -S consumer -B consumer/build -DCMAKE_PREFIX_PATH=/tmp/tpl
```

A version tag zips exactly this per platform ([ci.md](ci.md#releases)).

## justfile

`just` recipes are shortcuts over the presets (`just build`, `just test`, `just sanitize tsan`,
`just coverage`, …) plus a few non-CMake tasks (`just setup`, `just tooling-check`).
`just --list` shows them all. Nothing in CI depends on `just`. `just model` and
`just install-model` fetch and install the speech-to-text model ([stt.md](stt.md)).
