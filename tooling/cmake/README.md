# `cmake/` — shared CMake modules

The build logic that every tanh-lab C++ repository used to copy — platform detection,
the symbol-export policy, git versioning, sanitizers, test dependencies, Apple
defaults, CPack, install RPATHs, binary data — maintained here once and consumed as
**verbatim committed copies** in each repo's `cmake/tanh/`. Same mechanics as the
clang configs: pinned tag, installer, drift check in CI.

## Consumer

```sh
curl -fsSL https://raw.githubusercontent.com/tanh-lab/tanh-tooling/vX.Y.Z/install.sh | sh -s -- clang cmake
git add cmake/tanh .clang-* && git commit -m "tooling: pin tanh-tooling vX.Y.Z"
```
```yaml
# .github/workflows/<ci>.yml — fails when cmake/tanh/ or .clang-* drift from the pin
jobs:
  tooling-config:
    uses: tanh-lab/tanh-tooling/.github/workflows/config-check.yml@vX.Y.Z
    with:
      ref: vX.Y.Z
      family: "clang cmake"
```
```cmake
# CMakeLists.txt
include(cmake/tanh/git-version.cmake)
tanh_git_version(${CMAKE_CURRENT_SOURCE_DIR})
project(mylib VERSION ${TANH_VERSION_SHORT})
include(cmake/tanh/platform.cmake)
include(cmake/tanh/symbol-policy.cmake)
add_library(mylib ...)
tanh_apply_symbol_policy(mylib EXPORT_PREFIX MYLIB)
tanh_set_export_allowlist(mylib NAMESPACE mylib)
```

`cmake/tanh/` is owned by the pin: never edit it, never add files to it (`--check`
flags both). Changes go here, then a new tag, then every consumer re-runs the
installer. **Every project in one build must pin the same tag** — when anira fetches
tanh-lib, both copies of the modules are loaded; the version stamp
(`modules-version.cmake`) warns on a mismatch, but the last definition would win
silently otherwise. During development, `TANH_TOOLING_SRC=<checkout>` installs from
disk instead of a tag.

## Modules

| Module | Provides |
|---|---|
| `modules-version.cmake` | `TANH_CMAKE_MODULES_VERSION`; warns when two releases meet in one build |
| `platform.cmake` | `TANH_OPERATING_SYSTEM` (Linux, macOS, iOS, Android, Windows, Emscripten), `TANH_BINARY_FORMAT` (ELF, Mach-O, PE, Wasm), `TANH_IOS_PLATFORM` (DEVICE, SIMULATOR), `TANH_PLATFORM_COMPILE_DEFINITIONS` (`THL_PLATFORM_*=1`). Most linker decisions key on the format, not the OS — `--exclude-libs` vs `-load_hidden` vs `dllexport` — and `APPLE`/`UNIX` are exactly the ambiguous spellings of that |
| `symbol-policy.cmake` | `tanh_apply_symbol_policy(<t> [EXPORT_PREFIX <P>] [NO_GC_SECTIONS])` — hidden visibility, `<P>_BUILDING`/`<P>_STATIC`, `-fno-gnu-unique`, sections + link-time GC, `/wd4251`; `tanh_set_export_allowlist(<t> [NAMESPACE <ns>…] [SYMBOL <glob>…])` — generated version script / `-exported_symbols_list`; `tanh_hidden_archive_link_items(<archive> <libs> <opts>)` — link a prebuilt archive without exporting it |
| `check-exports.cmake` | `tanh_add_export_check(NAME … [LIBRARY <t>] [MODULE <t>] NAMESPACES … [FORBID_NAMESPACES …] [FORBID_PREFIXES …] [ALLOW_REGEX …] [TOLERATE_REGEX_PE …])` — a CTest that diffs the real export table (`nm`/`dumpbin`) against the allowlist; the same file is the script it runs |
| `git-version.cmake` | `tanh_git_version(<dir> [MATCH <glob>…])` → `TANH_VERSION_SHORT` (digits only: a pre-release tag `v3.0.0-alpha.1` gives `3.0.0`), `TANH_VERSION_FULL` (the describe string, `3.0.0-alpha.1-3-gabc123`), `TANH_VERSION_PRERELEASE` (`alpha.1`), `TANH_VERSION_DISTANCE` (commits past the tag) — before `project()`; `MATCH` restricts the tags a branch names itself after |
| `sanitizers.cmake` | `tanh_add_sanitizer(<t> rtsan\|asan\|ubsan\|tsan\|msan\|lsan [DEFINE <name>] [IGNORELIST <file>])` |
| `test-deps.cmake` | `tanh_fetch_googletest([VERSION])`, `tanh_fetch_googlebenchmark([VERSION])`, `tanh_copy_runtime_dlls(<t>)` |
| `apple.cmake` | `tanh_apple_deployment_target([MACOS v] [IOS v])`, `tanh_apple_sysroot_from_xcrun()`, `tanh_ios_disable_code_signing(<t>…)`, `tanh_ios_test_bundle(<t> BUNDLE_ID_PREFIX <p> [DEVELOPMENT_TEAM <id>])` |
| `ios.toolchain.cmake` | toolchain file: `-DCMAKE_TOOLCHAIN_FILE=cmake/tanh/ios.toolchain.cmake -DTANH_IOS_PLATFORM=SIMULATOR\|DEVICE [-DTANH_IOS_TEST_RUNNER_DIR=<dir>]` |
| `package.cmake` | `tanh_cpack_debian(VENDOR … CONTACT … RUNTIME_DESCRIPTION … DEV_DESCRIPTION … [DEPS_COMPONENTS …])` — runtime/dev/deps Debian packages, ends with `include(CPack)` |
| `install-helpers.cmake` | `tanh_set_install_rpath(<t>… [EXTRA_ELF_PATHS …] [EXTRA_APPLE_PATHS …])` |
| `binary-data.cmake` + `bin2cpp.cmake` | `tanh_add_binary_data(<t> NAMESPACE <ns> HEADER_NAME <h> SOURCES <files>…)` |

Each module's header comment is the reference: inputs it reads, outputs it sets, the
CMake minimum, and whether it may run before `project()` or inside a package config.
Background on the symbol policy: `memory-bridge/guides/about_libraries.md`.

### The export header of a library

Every tanh-lab library has a self-contained `<lib>/…/Exports.h` (tanh-lib:
`tanh/core/Exports.h`, anira: `anira/abi/export.h`) defining `<LIB>_API` as a selector
on the defines the policy sets, with the platform spelling inlined — no include-dir
dependency on another library's header:

```cpp
#if defined(MYLIB_STATIC)          // static archive: no decoration, ever
#define MYLIB_API
#elif defined(MYLIB_BUILDING)      // set only while compiling the library itself
#if defined(_WIN32)
#define MYLIB_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define MYLIB_API __attribute__((visibility("default")))
#else
#define MYLIB_API
#endif
#else
#if defined(_WIN32)
#define MYLIB_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define MYLIB_API __attribute__((visibility("default")))
#else
#define MYLIB_API
#endif
#endif
```

GCC/Clang get `visibility("default")` on both sides on purpose: the library is compiled
with `-fvisibility=hidden`, so the attribute is what puts the API into the export
table, and vague-linkage entities a consumer instantiates from the headers keep
default visibility and are coalesced with the library's copies instead of becoming
private duplicates. (`tanh/core/ExportMacros.h` is a deprecated forwarding shim.)

## Rules for a module

1. One concern per file, `include_guard(GLOBAL)`, and a header comment listing inputs,
   outputs, the CMake minimum, and whether it may be included before `project()` /
   inside a package config. First line after the guard: `include(modules-version.cmake)`.
2. `TANH_`/`tanh_` for everything public, `_tanh_` for internals. No project name anywhere —
   anira reading `TANH_OPERATING_SYSTEM` is no odder than it using `thl::Buffer`.
3. Outputs are plain variables in the caller's scope (or `PARENT_SCOPE` results) — never
   `CACHE`, never `CMAKE_<LANG>_FLAGS`, never global properties. Behaviour that applies to
   a target is a function taking the target, so the consumer decides what it applies to.
   Documented exceptions: options of fetched dependencies (`FetchContent` needs the cache),
   `CMAKE_OSX_DEPLOYMENT_TARGET`/`CMAKE_OSX_SYSROOT` (read from the cache at generate
   time), and bookkeeping properties (`CTEST_TARGETS_ADDED`, the version stamp).
4. Inputs are CMake-provided facts only (`CMAKE_SYSTEM_NAME`, `CMAKE_OSX_SYSROOT`,
   `EMSCRIPTEN`, target properties). A sibling module is included explicitly via
   `${CMAKE_CURRENT_LIST_DIR}/<sibling>.cmake` — that is what lets a package config
   install and include the same files.
5. Every module has a test project under `cmake/test/<module>/`, run through
   `ctest --build-and-test` by the driver; `cmake-test.yml` runs it on ubuntu, macos and
   windows before anything is tagged.
6. Minimum CMake 3.18 (`test-deps.cmake`: 3.25 for `FetchContent … SYSTEM`).

## Testing

```sh
cmake -S cmake/test -B cmake/test/build
ctest --test-dir cmake/test/build --output-on-failure
# a specific compiler, offline dependency sources:
cmake -S cmake/test -B cmake/test/build \
  "-DTANH_TEST_BUILD_OPTIONS=-DCMAKE_CXX_COMPILER=g++;-DCMAKE_C_COMPILER=gcc;-DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=/path/googletest"
```

## Releasing

The mono-versioned tag (README, "Releasing"). The release commit bumps the default
`REF` in `install.sh` and `clang/install.sh`, the `default:` of both check workflows,
and `TANH_CMAKE_MODULES_VERSION` in `cmake/modules-version.cmake` to the new tag.
