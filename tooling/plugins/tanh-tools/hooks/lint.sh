#!/bin/bash
# lint.sh — lint the edited file with the right linter for its type.
#   C/C++/ObjC++ -> clang-tidy · Python -> ruff check · TS/JS -> eslint
# Auto-fixes where the linter supports it, then fails (exit 2) on anything left.
# If the matching tool is missing, prints install instructions and fails (exit 2).
# Non-matching file types are skipped silently (exit 0).
set -uo pipefail

# FILE comes from $1 when invoked by check.sh; otherwise parse the hook JSON on stdin.
FILE="${1:-$(jq -r '.tool_input.file_path // empty' 2>/dev/null)}"
[ -z "$FILE" ] && exit 0
[ -f "$FILE" ] || exit 0

# A JS/TS file outside any Node project (no package.json upward — session scripts,
# scratch files, generated code) has no linter contract to enforce: skip silently,
# the same way the C++ arm skips files absent from the compile DB.
in_node_project() {
  local dir; dir=$(dirname "$FILE")
  while [ "$dir" != "/" ]; do
    [ -f "$dir/package.json" ] && return 0
    dir=$(dirname "$dir")
  done
  return 1
}

fail() { printf '%s\n' "$1" >&2; exit 2; }

node_bin() {
  local name="$1" dir; dir=$(dirname "$FILE")
  while [ "$dir" != "/" ]; do
    if [ -x "$dir/node_modules/.bin/$name" ]; then printf '%s\n' "$dir/node_modules/.bin/$name"; return 0; fi
    dir=$(dirname "$dir")
  done
  command -v "$name" 2>/dev/null
}

case "$FILE" in
  *.cpp|*.mm)
    command -v clang-tidy >/dev/null 2>&1 || fail \
"lint hook: clang-tidy not found.
Install it:
  Fedora/Asahi : sudo dnf install clang-tools-extra
  macOS        : brew install llvm
  Debian/Ubuntu: sudo apt install clang-tidy"
    # Only lint files present in the desktop build's compile DB — otherwise
    # clang-tidy does a header-less parse and emits cascading false positives.
    CCJSON="build/desktop/Debug/compile_commands.json"
    [ -f "$CCJSON" ] || exit 0
    jq -e --arg f "$FILE" 'any(.[]; .file == $f)' "$CCJSON" >/dev/null 2>&1 || exit 0
    OUTPUT=$(clang-tidy --warnings-as-errors='*' -p build/desktop/Debug/ "$FILE" 2>&1) \
      || fail "$OUTPUT"
    ;;
  *.py)
    RUFF="$CLAUDE_PROJECT_DIR/.venv/bin/ruff"
    [ -x "$RUFF" ] || RUFF=$(command -v ruff 2>/dev/null) || RUFF=""
    [ -n "$RUFF" ] || fail \
"lint hook: ruff not found.
Install it:
  uv add --dev ruff   (or: uv tool install ruff)
  pip install ruff"
    "$RUFF" check --fix "$FILE" >/dev/null 2>&1
    OUTPUT=$("$RUFF" check "$FILE" 2>&1) || fail "$OUTPUT"
    ;;
  *.ts|*.tsx|*.js|*.jsx|*.mjs|*.cjs)
    in_node_project || exit 0
    ESLINT=$(node_bin eslint)
    [ -n "$ESLINT" ] || fail \
"lint hook: eslint not found.
Install it in the repo:
  npm i -D eslint   (or: pnpm add -D eslint)"
    "$ESLINT" --fix "$FILE" >/dev/null 2>&1
    OUTPUT=$("$ESLINT" --max-warnings=0 "$FILE" 2>&1)
    if [ $? -ne 0 ]; then
      # Skip gracefully when the file is outside ESLint's include patterns.
      printf '%s' "$OUTPUT" | grep -q "File ignored because of a matching ignore pattern" && exit 0
      fail "$OUTPUT"
    fi
    ;;
esac

exit 0
