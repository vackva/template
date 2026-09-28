#!/usr/bin/env bash
# The files a pull request changes, from git alone: no token, no API call.
#
# On a pull_request event actions/checkout leaves HEAD on the merge commit GitHub
# built for the run: its first parent is the base branch, its second the pull
# request's head. The diff between the first parent and HEAD is what the pull
# request changes against its base as it is now.
#
# Every doubt resolves to a full sweep, the safe direction: another event, a
# checkout that is not that merge commit, parents that cannot be fetched, a
# changed file that matches the sweep pattern, a pattern grep rejects.
#
# Environment:
#   CF_EVENT_NAME     github.event_name
#   CF_HEAD_SHA       github.event.pull_request.head.sha
#   CF_SWEEP_PATTERN  extended regex; a changed file that matches forces the sweep (optional)
#   GITHUB_OUTPUT     receives files (newline-separated), sweep (true|false), reason
set -u

emit() {  # emit <sweep> <reason> <files>
    local delimiter="CHANGED_FILES_EOF_$$_${RANDOM}"
    {
        echo "sweep=$1"
        echo "reason=$2"
        echo "files<<${delimiter}"
        [ -n "$3" ] && printf '%s\n' "$3"
        echo "${delimiter}"
    } >> "${GITHUB_OUTPUT}"
}

sweep() {
    echo "changed-files: $1 - full sweep."
    emit true "$1" ""
    exit 0
}

[ "${CF_EVENT_NAME:-}" = "pull_request" ] || sweep "the event is '${CF_EVENT_NAME:-}', not pull_request"
git rev-parse --is-inside-work-tree > /dev/null 2>&1 || sweep "the workspace is not a git checkout"

# The default checkout has depth 1 and therefore no parents: fetch one more level of
# this commit. It goes through the credentials actions/checkout persisted.
if ! git rev-parse --quiet --verify 'HEAD^2' > /dev/null; then
    git fetch --quiet --depth=2 origin "$(git rev-parse HEAD)" > /dev/null 2>&1 || true
fi
git rev-parse --quiet --verify 'HEAD^2' > /dev/null ||
    sweep "HEAD is not a merge commit whose parents are available (check out the merge ref, fetch-depth: 2 saves the extra fetch)"
[ "$(git rev-parse 'HEAD^2')" = "${CF_HEAD_SHA:-}" ] ||
    sweep "HEAD is not the pull request's merge commit (its second parent is not the pull request's head)"

# --no-renames: both sides of a rename are listed, so the old path of a renamed build
# file still meets the sweep pattern. Deleted paths stay in the list for the same
# reason; a consumer drops what no longer exists.
FILES=$(git diff --name-only --no-renames 'HEAD^1' HEAD) || sweep "git diff failed"
[ -n "${FILES}" ] || sweep "the pull request changes no file"

if [ -n "${CF_SWEEP_PATTERN:-}" ]; then
    HIT=$(printf '%s\n' "${FILES}" | grep -E -m1 -- "${CF_SWEEP_PATTERN}")
    case $? in
        0) sweep "'${HIT}' matches the sweep pattern" ;;
        1) ;;
        *) sweep "the sweep pattern is not a valid extended regex" ;;
    esac
fi

COUNT=$(printf '%s\n' "${FILES}" | wc -l | tr -d ' ')
echo "changed-files: ${COUNT} changed files, no sweep trigger."
emit false "${COUNT} changed files" "${FILES}"
