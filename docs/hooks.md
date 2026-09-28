# Hooks

Hooks are commands that run on an event. Unlike an instruction in `CLAUDE.md`, the harness (or
git) runs them, so they cannot be skipped or forgotten.

| When | Hook | Defined in | Does |
|---|---|---|---|
| before Claude edits a file | `protect-installed.sh` | `.claude/settings.json` | refuses edits to installed tooling copies |
| after Claude edits a file | `check.sh` (lint → format → typecheck) | tanh-tools plugin | clang-tidy, then clang-format, on that file |
| Claude wants to stop | `build-and-test.sh` | `.claude/settings.json` | builds and tests; failures send Claude back |
| `git push` | `pre-push` | `hooks/tanh/pre-push` | clang-format + clang-tidy on the pushed diff |

## Claude Code hooks

Registered in [`.claude/settings.json`](../.claude/settings.json):

```json
"hooks": {
  "PreToolUse": [{ "matcher": "Edit|Write|MultiEdit",
                   "hooks": [{ "type": "command", "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/protect-installed.sh" }] }],
  "Stop":       [{ "hooks": [{ "type": "command", "command": "\"$CLAUDE_PROJECT_DIR\"/.claude/hooks/build-and-test.sh",
                               "timeout": 600 }] }]
}
```

A hook gets the event as JSON on stdin (`tool_input.file_path`, `stop_hook_active`, …).
**Exit 2** blocks the action and hands stderr to Claude as feedback; exit 0 lets it through.

### PreToolUse: protect-installed.sh

`.clang-*`, `cmake/tanh/*` and `hooks/tanh/*` are copies of `tooling/` ([tooling.md](tooling.md)).
An edit there is refused with a pointer to the source:

```
cmake/tanh/platform.cmake is an installed copy of tooling/cmake/platform.cmake — do not edit it.
Edit tooling/cmake/platform.cmake instead, then run: sh tooling/install.sh cmake
```

### PostToolUse: format and lint (tanh-tools plugin)

`tooling/plugins/tanh-tools/hooks/check.sh` runs `lint.sh` → `format.sh` → `typecheck.sh` in one
process (separate hooks would run concurrently and race on the same file). For C++:

- `clang-tidy --warnings-as-errors='*' -p build/desktop/Debug <file>`, for `.cpp` files that are
  in the compile DB; findings block and go back to Claude
- `clang-format -i <file>`

Python (ruff, pyright) and TS/JS (eslint, prettier, tsc) are handled the same way in other repos.

### Stop: build-and-test.sh

When the working tree has C++ or CMake changes, the hook builds `desktop-debug` and runs
`ctest`. A red build or test blocks the stop, and Claude gets the compiler errors or failing
tests to fix. So a turn does not end on broken code without anyone running the tests.

- nothing changed → nothing runs
- the same tree already passed once → not rebuilt (fingerprint of the diff)
- three failed attempts in a row → Claude may stop and has to report the failure instead of looping

## git pre-push

[`hooks/tanh/pre-push`](../hooks/tanh/pre-push) (installed from `tooling/hooks/`), armed by
`just setup` (`ln -sf ../../hooks/tanh/pre-push .git/hooks/pre-push`; git does not version hooks).
On every push it checks the changed files:

1. `clang-format --dry-run --Werror` on changed sources and headers
2. `clang-tidy` on changed `.cpp` files, using the newest `compile_commands.json` under `build/`
   that knows them

It skips (never blocks) when a tool or compile DB is missing. `TANH_SKIP_FORMAT=1` /
`TANH_SKIP_TIDY=1` skip a step. If you use a global `core.hooksPath`, chain to the repo's
`.git/hooks/pre-push` from there.
