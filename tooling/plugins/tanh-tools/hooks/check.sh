#!/bin/bash
# check.sh — the single PostToolUse(Write|Edit) dispatcher. Runs format / lint /
# typecheck on the edited file IN ORDER, within one process.
#
# Why one script instead of three separate hook entries: Claude Code runs the hooks
# registered under a matcher concurrently. lint.sh and format.sh both rewrite the
# file, so as separate hooks they race — whichever writes last wins, and a file that
# needs both a lint --fix and a reformat ends up with a non-deterministic result.
# Sequencing them here removes the race and pins the order: lint --fix first, then
# the formatter gets the last word on whitespace (per Astral's ruff guidance), then
# typecheck.
set -uo pipefail

FILE=$(jq -r '.tool_input.file_path // empty' 2>/dev/null)
[ -z "$FILE" ] && exit 0
[ -f "$FILE" ] || exit 0

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Each step takes the file path as $1. A step that fails (exit 2, with install
# instructions or tool output on stderr) aborts the rest and surfaces to Claude.
"$DIR/lint.sh"      "$FILE" || exit $?
"$DIR/format.sh"    "$FILE" || exit $?
"$DIR/typecheck.sh" "$FILE" || exit $?
exit 0
