#!/bin/bash
# build-and-test.sh — Stop hook: Claude may not finish a turn with a red build.
#
# When the working tree has C++/CMake changes, build the desktop-debug preset and
# run ctest. On failure the output goes to Claude (exit 2 blocks the stop), which
# then fixes the code and tries to stop again — the edit/compile/test loop runs
# without a human in it.
#
# Cheap by construction:
#   - no C++/CMake change in the working tree -> nothing runs;
#   - a tree already green once (same diff fingerprint) is not rebuilt;
#   - after 3 consecutive red stops Claude is let go, so an unfixable failure
#     cannot loop forever — it reports instead.
set -uo pipefail

cd "$CLAUDE_PROJECT_DIR" || exit 0
INPUT=$(cat)
ACTIVE=$(printf '%s' "$INPUT" | jq -r '.stop_hook_active // false' 2>/dev/null)

PATHS=('*.cpp' '*.h' '*.hpp' 'CMakeLists.txt' '*.cmake' 'CMakePresets.json')
[ -n "$(git status --porcelain -- "${PATHS[@]}" 2>/dev/null)" ] || exit 0

BUILD=build/desktop/Debug
STATE="$BUILD/.claude-stop-gate"
mkdir -p "$BUILD"

fingerprint=$( { git diff HEAD -- "${PATHS[@]}" 2>/dev/null; git ls-files -o --exclude-standard -z -- "${PATHS[@]}" | xargs -0 cat 2>/dev/null; } | git hash-object --stdin)
[ "$(cat "$STATE.green" 2>/dev/null)" = "$fingerprint" ] && exit 0

fails=$(cat "$STATE.fails" 2>/dev/null || echo 0)
[ "$ACTIVE" = "true" ] || fails=0

run() {
  [ -f "$BUILD/CMakeCache.txt" ] || cmake --preset desktop-debug >/dev/null 2>&1 || cmake --preset desktop-debug 2>&1
  cmake --build --preset desktop-debug 2>&1 | grep -E "error|FAILED|warning:" | head -40
  [ "${PIPESTATUS[0]}" -eq 0 ] || return 1
  ctest --preset desktop-debug 2>&1 | tail -30
  return "${PIPESTATUS[0]}"
}

if OUTPUT=$(run); then
  echo "$fingerprint" > "$STATE.green"
  echo 0 > "$STATE.fails"
  exit 0
fi

fails=$((fails + 1))
echo "$fails" > "$STATE.fails"
if [ "$fails" -ge 3 ]; then
  echo "build/tests still failing after $fails attempts — stopping; report the failure to the user." >&2
  echo 0 > "$STATE.fails"
  exit 0
fi

printf 'The desktop-debug build or its tests fail — fix them before finishing (attempt %s/3):\n%s\n' "$fails" "$OUTPUT" >&2
exit 2
