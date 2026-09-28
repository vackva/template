#!/usr/bin/env bash
# Claude Code status line — mirrors p10k left prompt segments:
# dir | git branch+status | model | context usage

input=$(cat)

ESC=$'\033'

cwd=$(echo "$input" | jq -r '.workspace.current_dir // .cwd')
model=$(echo "$input" | jq -r '.model.display_name // empty')
used_pct=$(echo "$input" | jq -r '.context_window.used_percentage // empty')

# Shorten home directory to ~
home="$HOME"
short_cwd="~${cwd#"$home"}"; [ "${cwd#"$home"}" = "$cwd" ] && short_cwd="$cwd"

# Git info (skip optional locks for real-time safety)
git_part=""
if git -C "$cwd" rev-parse --git-dir > /dev/null 2>&1; then
  branch=$(git -C "$cwd" symbolic-ref --short HEAD 2>/dev/null || git -C "$cwd" rev-parse --short HEAD 2>/dev/null)
  if [ -n "$branch" ]; then
    # Collect status indicators
    status_flags=""
    git_status=$(git -C "$cwd" status --porcelain 2>/dev/null)
    staged=$(echo "$git_status" | grep -c '^[MADRC]' 2>/dev/null)
    unstaged=$(echo "$git_status" | grep -c '^.[MD]' 2>/dev/null)
    untracked=$(echo "$git_status" | grep -c '^??' 2>/dev/null)
    ahead=$(git -C "$cwd" rev-list --count @{u}..HEAD 2>/dev/null || echo 0)
    behind=$(git -C "$cwd" rev-list --count HEAD..@{u} 2>/dev/null || echo 0)

    [ "$staged" -gt 0 ]    && status_flags="${status_flags}+${staged}"
    [ "$unstaged" -gt 0 ]  && status_flags="${status_flags} !${unstaged}"
    [ "$untracked" -gt 0 ] && status_flags="${status_flags} ?${untracked}"
    [ "$ahead" -gt 0 ]     && status_flags="${status_flags} ⇡${ahead}"
    [ "$behind" -gt 0 ]    && status_flags="${status_flags} <${behind}"

    status_flags="${status_flags# }"  # trim leading space
    if [ -n "$status_flags" ]; then
      git_part=" ${ESC}[33m${branch} ${status_flags}${ESC}[0m"
    else
      git_part=" ${ESC}[32m${branch}${ESC}[0m"
    fi
  fi
fi

# Context usage
ctx_part=""
if [ -n "$used_pct" ]; then
  pct_int=$(printf '%.0f' "$used_pct")
  ctx_part=" ctx:${pct_int}%"
fi

# Model
model_part=""
[ -n "$model" ] && model_part=" ${model}"

printf "${ESC}[34m%s${ESC}[0m%s%s%s" \
  "$short_cwd" \
  "$git_part" \
  "$model_part" \
  "$ctx_part"
