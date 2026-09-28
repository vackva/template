#!/bin/bash
# typecheck.sh — type-check the edited file with the right checker for its type.
#   Python -> pyright (scoped to the file, honours pyrightconfig.json)
#   TS/TSX -> tsc --noEmit against the repo's tsconfig.check.json
# If the matching tool is missing, prints install instructions and fails (exit 2).
# Non-matching file types are skipped silently (exit 0).
set -uo pipefail

# FILE comes from $1 when invoked by check.sh; otherwise parse the hook JSON on stdin.
FILE="${1:-$(jq -r '.tool_input.file_path // empty' 2>/dev/null)}"
[ -z "$FILE" ] && exit 0
[ -f "$FILE" ] || exit 0

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
  *.py)
    PYRIGHT="$CLAUDE_PROJECT_DIR/.venv/bin/pyright"
    [ -x "$PYRIGHT" ] || PYRIGHT=$(command -v pyright 2>/dev/null) || PYRIGHT=""
    [ -n "$PYRIGHT" ] || fail \
"typecheck hook: pyright not found.
Install it:
  uv add --dev pyright   (or: uv tool install pyright)
  pip install pyright"
    # Run from project root so pyrightconfig.json is picked up.
    OUTPUT=$(cd "$CLAUDE_PROJECT_DIR" && "$PYRIGHT" "$FILE" 2>&1) || fail "$OUTPUT"
    ;;
  *.ts|*.tsx)
    # Resolve tsc + tsconfig.check.json from the nearest enclosing project.
    DIR=$(dirname "$FILE"); TSC=""; TSCONFIG=""; SEARCH="$DIR"
    while [ "$SEARCH" != "/" ]; do
      if [ -x "$SEARCH/node_modules/.bin/tsc" ] && [ -f "$SEARCH/tsconfig.check.json" ]; then
        TSC="$SEARCH/node_modules/.bin/tsc"; TSCONFIG="$SEARCH/tsconfig.check.json"; break
      fi
      SEARCH=$(dirname "$SEARCH")
    done
    if [ -z "$TSC" ]; then
      # tsc present but no tsconfig.check.json -> nothing to check against; skip.
      [ -n "$(node_bin tsc)" ] && exit 0
      fail \
"typecheck hook: tsc (typescript) not found.
Install it in the repo:
  npm i -D typescript   (or: pnpm add -D typescript)"
    fi
    OUTPUT=$("$TSC" --noEmit --incremental -p "$TSCONFIG" 2>&1) || fail "$OUTPUT"
    ;;
esac

exit 0
