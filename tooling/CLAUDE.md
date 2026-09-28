# tooling/ — in-repo tanh-tooling

Vendored from [tanh-tooling](https://github.com/tanh-lab/tanh-tooling) v0.2.8 (C++ families
and the Claude plugin; the Python and JS families are left out). This directory is the
**source of truth**; the repo root holds installed copies because the tools look there.

| Family | Source | Installed to | Used by |
|---|---|---|---|
| `clang` | `clang/clang-format`, `clang-tidy`, `clangd` | `.clang-format`, `.clang-tidy`, `.clangd` | editors, hooks, CI lint |
| `cmake` | `cmake/*.cmake` (+ `cmake/test/`) | `cmake/tanh/` | `CMakeLists.txt` |
| `hooks` | `hooks/pre-push` | `hooks/tanh/pre-push` | git (`just setup` links it) |
| plugin | `plugins/tanh-tools/`, `.claude-plugin/marketplace.json` | — (loaded in place) | Claude Code, via `.claude/settings.json` |

## Changing a family

1. Edit the file here, never the installed copy (a PreToolUse hook refuses those edits).
2. `sh tooling/install.sh <family>` and commit source + copy together.
3. CMake module changes: run the module tests, `just tooling-test`
   (`tooling/cmake/test/`, one standalone project per module; CI runs them on Linux,
   macOS and Windows via `.github/workflows/tooling.yml`).
4. Follow the module rules in `cmake/README.md` (one concern per file, `tanh_` prefix,
   no CACHE outputs, a test project per module).

## Changing the plugin

Agents, skills, hooks and the MCP config live in `plugins/tanh-tools/`. Bump `version` in
`plugins/tanh-tools/.claude-plugin/plugin.json` on every change, otherwise Claude Code
keeps its cached copy; then `/plugin marketplace update tanh-tooling` or restart.
The C++ lint step in `hooks/lint.sh` reads `build/desktop/Debug/compile_commands.json` —
keep that path in sync with the `desktop-debug` preset.
