#!/bin/bash
# protect-installed.sh — PreToolUse(Edit|Write|MultiEdit): refuse edits to the
# installed copies of tooling/ (.clang-*, cmake/tanh/, hooks/tanh/).
#
# Those files are byte-for-byte copies that CI's tooling-config job checks
# against tooling/. An edit here would be reverted by the next install and fail
# CI until then, so Claude is pointed at the source instead. Exit 2 blocks the
# tool call and hands stderr to Claude.
set -uo pipefail

FILE=$(jq -r '.tool_input.file_path // empty' 2>/dev/null)
[ -z "$FILE" ] && exit 0
REL="${FILE#"$CLAUDE_PROJECT_DIR"/}"

case "$REL" in
  .clang-format|.clang-tidy|.clangd) family=clang; src="tooling/clang/${REL#.}" ;;
  cmake/tanh/*)                      family=cmake; src="tooling/cmake/${REL#cmake/tanh/}" ;;
  hooks/tanh/*)                      family=hooks; src="tooling/hooks/${REL#hooks/tanh/}" ;;
  *) exit 0 ;;
esac

cat >&2 <<MSG
$REL is an installed copy of $src — do not edit it.
Edit $src instead, then run: sh tooling/install.sh $family
(CI's tooling-config job fails when an installed copy differs from tooling/.)
MSG
exit 2
