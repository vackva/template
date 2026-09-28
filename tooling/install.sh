#!/usr/bin/env sh
# install.sh — copy the shared config files from tooling/ into the repo, verbatim.
#
#   sh tooling/install.sh <family>...            write the files of each family
#   sh tooling/install.sh --check <family>...    CI: exit non-zero if any installed file differs
#
# In-repo variant of tanh-tooling's installer: instead of fetching a pinned tag
# from GitHub, the source of truth is this directory, in the same commit. The
# installed copies (.clang-*, cmake/tanh/, hooks/tanh/) exist because the tools
# look for them at fixed places — clang-format walks up for .clang-format, the
# build includes cmake/tanh/*.cmake. `--check` is what keeps them honest: CI
# (lint.yml, tooling-config job) fails when an installed copy was edited by hand
# instead of here.
#
# Families are the subdirectories that carry a `manifest` (clang, cmake, hooks).
# A manifest is an sh fragment:
#   FILES="a b c"        files of the family, relative to its directory
#   DEST="cmake/tanh/%s" where each file lands, relative to the repo root (%s = file name)
#   MODE="755"           optional chmod applied after install (scripts/hooks)
set -eu

SRC="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SRC/.." && pwd)"
cd "$ROOT"

check=0
if [ "${1:-}" = "--check" ]; then check=1; shift; fi
[ $# -gt 0 ] || { echo "usage: tooling/install.sh [--check] <family>...   (clang, cmake, hooks)" >&2; exit 2; }

drift=0
for family in "$@"; do
  [ -f "$SRC/$family/manifest" ] || { echo "unknown family: $family (no tooling/$family/manifest)" >&2; exit 1; }
  FILES=""; DEST=""; MODE=""
  . "$SRC/$family/manifest"
  [ -n "$FILES" ] && [ -n "$DEST" ] || { echo "bad manifest: tooling/$family/manifest" >&2; exit 1; }
  for f in $FILES; do
    dest="$(printf '%s' "$DEST" | sed "s|%s|$f|")"
    [ -f "$SRC/$family/$f" ] || { echo "missing: tooling/$family/$f" >&2; exit 1; }
    if [ "$check" -eq 1 ]; then
      if [ ! -f "$dest" ] || ! cmp -s "$SRC/$family/$f" "$dest"; then echo "out of date: $dest"; drift=1; fi
    else
      mkdir -p "$(dirname "$dest")"
      cp "$SRC/$family/$f" "$dest"
      [ -z "$MODE" ] || chmod "$MODE" "$dest"
      echo "wrote $dest"
    fi
  done
  # A family that owns a directory (DEST has a path component) owns it entirely:
  # anything else in there is not from tooling/ and is flagged in --check.
  case "$DEST" in
    */*)
      dir="$(dirname "$DEST")"
      if [ "$check" -eq 1 ] && [ -d "$dir" ]; then
        for existing in "$dir"/*; do
          [ -f "$existing" ] || continue
          name="$(basename "$existing")"
          case " $FILES " in *" $name "*) ;; *) echo "not from tooling/: $existing"; drift=1;; esac
        done
      fi;;
  esac
done

if [ "$check" -eq 1 ]; then
  [ "$drift" -eq 0 ] || { echo "edit tooling/ instead, then run: sh tooling/install.sh $*" >&2; exit 1; }
  echo "installed tooling files up to date: $*"
fi
