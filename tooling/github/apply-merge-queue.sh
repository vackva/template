#!/usr/bin/env sh
# apply-merge-queue.sh <owner/repo> <required-context>...
#
# Creates or replaces the "main: merge queue + required checks" ruleset on the
# repo's default branch, from merge-queue-ruleset.json next to this script,
# with the given contexts as the required status checks. Needs gh with admin
# access to the repo.
#
# Required contexts must report on every PR head — entry into the queue is
# refused while a required check has not reported. A queue-only workflow must
# therefore carry a PR-head stub (a pull_request trigger whose real jobs skip
# and only the "<name> result" job reports instant success) so its result CAN
# be listed here; an unlisted result does NOT gate the merge — non-required
# merge-group failures are ignored (measured, anira#139). Only genuinely
# advisory checks (e.g. path-filtered clang-tidy) stay unlisted.
set -eu
repo="$1"; shift
[ $# -gt 0 ] || { echo "usage: apply-merge-queue.sh <owner/repo> <required-context>..." >&2; exit 2; }

dir="$(dirname "$0")"
checks="$(printf '%s\n' "$@" | jq -R '{context: .}' | jq -s .)"
body="$(jq --argjson checks "$checks" '(.rules[] | select(.type=="required_status_checks") | .parameters.required_status_checks) = $checks' "$dir/merge-queue-ruleset.json")"

id="$(gh api "repos/$repo/rulesets" -q '.[] | select(.name=="main: merge queue + required checks") | .id' | head -1)"
if [ -n "$id" ]; then
  printf '%s' "$body" | gh api -X PUT "repos/$repo/rulesets/$id" --input - -q '"updated ruleset " + (.id|tostring)'
else
  printf '%s' "$body" | gh api -X POST "repos/$repo/rulesets" --input - -q '"created ruleset " + (.id|tostring)'
fi
