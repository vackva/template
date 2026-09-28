# clang-format, clang-tidy, clangd

Files: [`.clang-format`](../.clang-format), [`.clang-tidy`](../.clang-tidy), [`.clangd`](../.clangd).
They are **installed copies** of `tooling/clang/`: edit the source there and run
`just tooling-install` ([tooling.md](tooling.md)). Every tool finds its file by walking up from
the source file, which is why the copies live at the repo root.

## .clang-format — how code looks

Google style with these changes:

```yaml
BasedOnStyle: Google
ColumnLimit: 100
IndentWidth: 4
AccessModifierOffset: -4     # public:/private: flush with the class
InsertBraces: true           # braces on every if/for, even one-liners
BinPackArguments: false      # one argument per line once a call wraps
BinPackParameters: false
BreakConstructorInitializers: BeforeComma
AllowShortFunctionsOnASingleLine: Inline
```

Runs: after every edit by Claude (hook), `just format`, `pre-push` on the pushed files,
CI `lint` (`clang-format --dry-run --Werror` over `src/ include/ test/`).

Versions differ in details: CI uses clang-format 20. If your local version formats
differently, CI decides.

## .clang-tidy — what code may do

```yaml
Checks: >
  clang-analyzer-*, bugprone-*, performance-*, modernize-*, misc-*,
  readability-identifier-naming,
  -bugprone-easily-swappable-parameters, -performance-enum-size,
  -modernize-use-trailing-return-type, -modernize-use-nodiscard,
  -misc-non-private-member-variables-in-classes
HeaderFilterRegex: "^.*/(src|include|examples)/.*"      # report on our headers…
ExcludeHeaderFilterRegex: "^(/usr/|/opt/|.*/build/|.*/_deps/|.*/modules/).*"   # …not on fetched ones
```

The naming rules are enforced here, not by convention:

| Entity | Style | Example |
|---|---|---|
| class, struct, enum | `CamelCase` | `OnePoleLowpass` |
| function, method, variable, parameter | `lower_case` | `set_cutoff`, `sample_rate` |
| member | `m_` + `lower_case` | `m_coefficient` |
| constexpr / global / static constant | `k_` + `lower_case` | `k_ramp_seconds` |
| enum value | `CamelCase` | `Linear` |
| macro | `UPPER_CASE` | `TPL_API` |

Every run uses `--warnings-as-errors='*'`: a finding is a failure, not a suggestion.
`misc-include-cleaner` and `misc-const-correctness` are the ones you will meet most (include
what you use, `const` what you don't modify).

clang-tidy needs the real compile flags, so it reads `build/desktop/Debug/compile_commands.json`
(`-p build/desktop/Debug`). Configure `desktop-debug` once per checkout (`just build`).
Runs: after every edit by Claude (hook, `.cpp` files in the compile DB), `just tidy`,
`pre-push`, CI `clang_tidy` (changed files on PRs, all files on `main`).

On macOS with Homebrew clang-tidy and Apple clang, the compile DB must contain `-isysroot`,
or clang-tidy cannot find `<span>`; `tanh_apple_sysroot_from_xcrun()` in `CMakeLists.txt`
does that.

## .clangd — the language server

```yaml
CompileFlags:
  CompilationDatabase: build/desktop/Debug/   # same compile DB as clang-tidy
Index:
  Background: Build                           # index the whole project in the background
```

clangd gives your editor (and Claude, through the `clangd-lsp` plugin) go-to-definition,
find-references and live diagnostics, including clang-tidy findings as you type, because it
reads `.clang-tidy` too. Without the compile DB it guesses flags and reports nonsense errors.
