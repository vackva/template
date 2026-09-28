# Shared tooling (`tooling/`)

`tooling/` is [tanh-tooling](https://github.com/tanh-lab/tanh-tooling) (v0.2.8) inside this
repo: the configs and CMake modules every tanh-lab C++ repo shares. In the real setup each repo
fetches them from a pinned tag; here they are local, so you can read and change everything in
one place.

| Family | Source | Installed to |
|---|---|---|
| `clang` | `tooling/clang/{clang-format,clang-tidy,clangd}` | `.clang-format`, `.clang-tidy`, `.clangd` ([clang-tools.md](clang-tools.md)) |
| `cmake` | `tooling/cmake/*.cmake` | `cmake/tanh/` ([cmake.md](cmake.md)) |
| `hooks` | `tooling/hooks/pre-push` | `hooks/tanh/pre-push` ([hooks.md](hooks.md)) |
| plugin | `tooling/plugins/tanh-tools/` | not copied; loaded by Claude Code ([claude-project.md](claude-project.md)) |

## Why copies

The tools look in fixed places: clang-format walks up from the file to find `.clang-format`,
CMake includes `cmake/tanh/…`. So the files have to exist there, and `tooling/install.sh`
copies them byte for byte:

```sh
sh tooling/install.sh clang cmake hooks            # write the copies   (just tooling-install)
sh tooling/install.sh --check clang cmake hooks    # fail on any drift  (just tooling-check)
```

`--check` fails when a copy differs from its source, or when `cmake/tanh/` or `hooks/tanh/`
contain a file that is not from `tooling/`. CI's `lint` workflow runs it, and a Claude hook
refuses edits to the copies. Change the source, reinstall, commit both.

Each family has a `manifest` (which files, where they go, file mode):

```sh
# tooling/cmake/manifest
FILES="modules-version.cmake platform.cmake symbol-policy.cmake …"
DEST="cmake/tanh/%s"
```

## CMake modules

| Module | Provides |
|---|---|
| `git-version` | `tanh_git_version()`: project version from the nearest `vX.Y.Z` tag |
| `platform` | `TANH_OPERATING_SYSTEM`, `TANH_BINARY_FORMAT`, `THL_PLATFORM_*` defines |
| `symbol-policy` | hidden visibility, `<P>_BUILDING` / `<P>_STATIC`, export allowlist |
| `check-exports` | CTest over the real export table |
| `sanitizers` | `tanh_add_sanitizer(<target> asan\|ubsan\|tsan\|rtsan\|…)` |
| `test-deps` | `tanh_fetch_googletest()`, `tanh_fetch_googlebenchmark()` |
| `apple`, `ios.toolchain` | deployment target, sysroot, iOS toolchain |
| `install-helpers`, `package` | install RPATH, Debian packages |
| `binary-data`, `bin2cpp` | embed files as C++ arrays |

Full reference: [`tooling/cmake/README.md`](../tooling/cmake/README.md). Every module has a test
project in `tooling/cmake/test/`:

```sh
just tooling-test     # CI: tooling.yml on Linux, macOS and Windows when tooling/ changes
```

## GitHub

`tooling/github/`: the merge-queue ruleset and its apply script (org-owned repos only). This
repo's own ruleset is `.github/rulesets/main.json` ([ci.md](ci.md#branch-protection)).
