# Claude Code setup

Claude Code reads its configuration from two layers:

- **User level** (`~/.claude/`): your personal preferences. They apply in every repo and are not shared.
- **Project level** (this repo: `CLAUDE.md`, `.claude/`, `tooling/plugins/`): the team's rules. They are committed and everyone who clones the repo gets them.

When the two layers disagree, the project wins over the user, and a subdirectory `CLAUDE.md` wins over the root one.
`.claude/settings.local.json` is for your own project overrides and is gitignored.

## What matters, in order

| # | Piece | Where | Why it matters |
|---|---|---|---|
| 1 | `CLAUDE.md` | `~/.claude/CLAUDE.md`, `./CLAUDE.md` | Loaded into every session. Commands, rules and "what I don't want" go here. This is the biggest lever. |
| 2 | Hooks | `.claude/settings.json`, plugins | The harness runs them, not the model, so they always happen: format and tidy after an edit, build and test on stop. Use a hook for anything that must never be skipped. |
| 3 | Skills and plugins | `.claude/skills/`, `tooling/plugins/`, `~/.claude/skills/` | Reusable workflows (`/add-processor`, `/explain-pr`) and bundles of hooks, agents and MCP servers. |
| 4 | `settings.json` | `~/.claude/`, `.claude/` | Model, effort, permissions, enabled plugins and marketplaces. |
| 5 | Auto-memory | `~/.claude/projects/<path-key>/memory/` | Claude writes these itself (a `MEMORY.md` index plus one file per fact). They are tied to the checkout path and are not shared, so move a rule the team needs into `CLAUDE.md`. |
| 6 | Statusline and notifications | `~/.claude/settings.json` | Shows context usage and pings you when Claude waits for input. |

## User level: example setup

[`user-setup/`](user-setup/) holds a working user-level setup to copy from:

| File | Install to | What it does |
|---|---|---|
| [`CLAUDE.md`](user-setup/CLAUDE.md) | `~/.claude/CLAUDE.md` | About me, toolchain, C++ real-time rules, "what I don't want", commit and answer style |
| [`settings.json`](user-setup/settings.json) | `~/.claude/settings.json` | Model and effort, attribution trailers turned off, plugins, marketplaces, statusline, input notification |
| [`statusline-command.sh`](user-setup/statusline-command.sh) | `~/.claude/statusline-command.sh` | `~/path branch +staged !unstaged ?untracked model ctx:NN%` |
| [`skills/explain-pr/`](user-setup/skills/explain-pr/SKILL.md) | `~/.claude/skills/explain-pr/` | `/explain-pr [PR or base]` explains a branch in execution order, with `file:line` references and real-time risks |
| [`commit-msg`](user-setup/commit-msg) | `~/.config/git/hooks/` with `git config --global core.hooksPath ~/.config/git/hooks` | Strips `Claude-Session:` trailers as a backstop, then chains to the repo's own hook |

```sh
mkdir -p ~/.claude/skills
cp docs/user-setup/CLAUDE.md docs/user-setup/statusline-command.sh ~/.claude/   # then rewrite "About me" in CLAUDE.md
cp -r docs/user-setup/skills/explain-pr ~/.claude/skills/
# merge docs/user-setup/settings.json into ~/.claude/settings.json instead of overwriting it
```

Plugins from the settings:

- `clangd-lsp`: go-to-definition and diagnostics through `compile_commands.json`.
- `audio-plugin-validators` (iPlug3): auval, pluginval, the VST3 validator, clap-validator and codesign checks as skills.
- `tanh-tooling`: the marketplace is registered but the plugin is **not** enabled globally. Its hook formats and lints every file Claude edits, in every repo, and fails when ruff or prettier are missing. Repos that want it enable it in their own `.claude/settings.json`, as this repo does.

## Further patterns

- **Sync `~/.claude` through a dotfiles repo.** Stow `CLAUDE.md`, `settings.json`, the statusline, plans and per-project memory from git. A plugin hook pulls on `SessionStart` and commits and pushes on `SessionEnd`, so another machine sees the same memory. Memory only syncs when the checkout path is the same on every machine. This is worth doing once you work on more than one machine. Gotchas:
  - Run `mkdir -p ~/.claude/projects/<key>` before `stow`. Otherwise session transcripts get folded into the repo.
  - Push from a detached process, because Claude Code cancels a `SessionEnd` hook still running at exit.
  - Plugin updates only arrive after a version bump in `plugin.json`.
- **Your own plugin marketplace.** Put your personal skills, hooks and MCP servers into one plugin in a git repo, and register it with `autoUpdate: true`. The lab-wide one is `tanh-lab/tanh-tooling`.
- **`defaultMode: auto` plus an allowlist** (`cmake`, `ctest`, `git`, `grep`, …) instead of skipping permission prompts entirely.

## Project level: this repo

See [claude-project.md](claude-project.md): the CLAUDE.md files, `.claude/settings.json`, the
tanh-tools plugin, the hooks and the `add-processor` skill.
