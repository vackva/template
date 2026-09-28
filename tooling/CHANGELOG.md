# Changelog

All notable changes to the shared tooling are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).
Consumers (tanh-lib, anira) pin a release tag in their config-check workflows
and install with `install.sh <family>...`; the internal
`TANH_CMAKE_MODULES_VERSION` tracks the **cmake family** and only moves when a
cmake module changes.

**Tagging checklist (mono-version)**: a `vX.Y.Z` tag also publishes the npm and
PyPI packages, so every tag bumps `install.sh`'s default `REF`,
`js/package.json` and `python/pyproject.toml` to X.Y.Z in the release commit.

## [Unreleased]

## [0.2.8] - 2026-09-02

### Changed

- `cmake/symbol-policy.cmake`: the comment on `tanh_apply_symbol_policy` no longer
  names tanh-lib's `THL_DECL_EXPORT` / `THL_DECL_IMPORT` macros or
  `tanh/core/ExportMacros.h`; it describes the selector library-neutrally
  (`<P>_BUILDING` → the platform's export decoration, else the import decoration).
  Each library owns its export selector: tanh-lib and anira ship self-contained
  export headers, and tanh-lib's `ExportMacros.h` is a deprecated forwarding shim.
  Comment-only, no behaviour change; the cmake-family stamp moves to 0.2.8 because
  the module file changed and a consumer's drift check compares file contents.
- `cmake/README.md`: the export-header example is self-contained (platform switch
  inlined) instead of including `tanh/core/ExportMacros.h`.

## [0.2.7] - 2026-09-02

### Changed

- `cmake/git-version.cmake`: `tanh_git_version` is pre-release aware. A tag such as
  `v3.0.0-alpha.1` yields `TANH_VERSION_SHORT = 3.0.0` (digits only, as `project()`
  and `find_package()` require; the old behaviour handed them `3.0.0-alpha.1` and
  the configure failed) while `TANH_VERSION_FULL` keeps the whole describe string.
  Two new outputs, `TANH_VERSION_PRERELEASE` (`alpha.1`, empty on a release tag)
  and `TANH_VERSION_DISTANCE` (commits past the nearest tag, `0` on the tag), let a
  consumer derive an ABI or pre-release ordinal from the tag. A new optional
  `MATCH <glob>...` argument (`git describe --match`) lets a long-lived branch that
  also reaches an older release line name itself after its own tags only. The
  module test covers every case; the cmake-family stamp moves to 0.2.7. The no-tag
  fallback (`0.0.0+g<hash>`) now excludes tags explicitly, so an annotated tag that
  `MATCH` rejected cannot leak into it.

### Fixed

- `cmake/test/git-version`: the not-a-checkout probe passed in CI only because that
  checkout carries no tags, and failed in a local clone whose build tree lies inside
  the tagged repository; it now sets `GIT_CEILING_DIRECTORIES` for the probe.

## [0.2.6] - 2026-09-01

### Fixed

- `plugins/tanh-tools` 0.1.5: the JS/TS lint and format hooks skip files outside
  any Node project (no `package.json` upward — session scripts, scratch files)
  instead of failing with "eslint/prettier not found". Same contract as the C++
  arm, which already skips files absent from the compile DB; a file inside a
  Node project still demands its tools. Typecheck already resolved per-project.

## [0.2.5] - 2026-09-01

### Added

- `hooks/pre-push`: a clang-format gate ahead of the existing clang-tidy one.
  It covers every changed `.cpp`/`.h`/`.hpp`/`.mm` (clang-tidy only ever saw
  `.cpp`, because it needs a compile DB), needs no build directory, and runs
  first because a formatting rejection is the cheapest kind to fix. Degrades to
  a skip when clang-format or `.clang-format` is missing, the way the tidy gate
  does; `TANH_SKIP_FORMAT=1` skips it explicitly. The rejection message names
  the `clang-format -i` command that fixes it. Motivated by an anira merge
  queue rejecting a push on two over-long lines the local hook had passed.

## [0.2.4] - 2026-09-01

### Added

- `github/merge-queue-ruleset.json` + `github/apply-merge-queue.sh`: the shared
  merge-queue configuration (queue on the default branch, required status
  checks passed as arguments). Applied per repo once via
  `sh github/apply-merge-queue.sh <owner/repo> <required-context>...`; anira
  and tanh-lib use the same style. Required contexts must report on every PR
  head — queue entry is refused while a required check has not reported.

- `cmake/platform.cmake`: `tanh_detect_emscripten()` — WASM/EMSDK_VERSION and
  the .js executable suffix when the compiler is em++ (moved from anira; the
  compile flags stay the caller's).

## [0.2.3] - 2026-09-01

### Added

- `cmake/apple.cmake`: `tanh_apple_default_architectures()` — default
  CMAKE_OSX_ARCHITECTURES when unset (iOS: arm64; macOS: the host arch) and
  mirror a single-arch macOS selection into CMAKE_SYSTEM_PROCESSOR for
  arch-keyed asset pickers (moved from anira's CMakeLists).

## [0.2.2] - 2026-08-31

### Fixed

- Mono-version alignment: `js/package.json` and `python/pyproject.toml` still
  said 0.1.5, so the v0.2.0/v0.2.1 tags' npm publish failed ("cannot publish
  over previously published versions") — those tags shipped no packages. This
  release aligns all three version sources and records the tagging checklist
  above.

## [0.2.1] - 2026-08-31

### Fixed

- `install.sh`'s default `REF` now matches the release it ships in — the v0.2.0
  copy still defaulted to v0.1.5, so a plain `curl ... | sh` fetched the old
  family set and could not find `hooks/`. (Release rule going forward: bumping
  the default REF is part of tagging.)

## [0.2.0] - 2026-08-31

### Added

- New `hooks` family: `hooks/tanh/pre-push`, a diff-based clang-tidy gate — it
  tidies the `.cpp` files a push changes against the newest
  `compile_commands.json` under `build/`, ignores TUs the compile DB does not
  know (out-of-scope sources), and degrades to a skip when clang-tidy or a
  compile DB is missing (`TANH_SKIP_TIDY=1` skips explicitly). Activation is
  per developer — git does not version hooks: symlink it as
  `.git/hooks/pre-push`, or chain from a `core.hooksPath` pre-push (pass stdin
  through). Manifests gain an optional `MODE` for executable installs.
- This changelog.

The `clang` and `cmake` families are byte-identical to v0.1.5 —
`TANH_CMAKE_MODULES_VERSION` stays `0.1.5`, so bumping the pin causes no cmake
churn and no cross-check warning against a tanh-lib fetched at 0.1.5.

## [0.1.5] and earlier

Untracked here — see the git history.
