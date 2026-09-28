# Claude Code: project setup

The committed, team-wide half of the Claude Code setup. The personal half (`~/.claude`) is in
[claude-setup.md](claude-setup.md).

| File | Role |
|---|---|
| [`CLAUDE.md`](../CLAUDE.md) | loaded in every session: commands, layout, rules, what runs automatically |
| [`test/CLAUDE.md`](../test/CLAUDE.md), [`tooling/CLAUDE.md`](../tooling/CLAUDE.md), [`.github/CLAUDE.md`](../.github/CLAUDE.md) | loaded when Claude works in that directory |
| [`.claude/settings.json`](../.claude/settings.json) | plugins, permissions, project hooks |
| [`.claude/hooks/`](../.claude/hooks/) | protect-installed and build-and-test ([hooks.md](hooks.md)) |
| [`.claude/skills/add-processor/`](../.claude/skills/add-processor/SKILL.md) | project skill |
| [`tooling/plugins/tanh-tools/`](../tooling/plugins/tanh-tools/) | the lab plugin |
| `.claude/settings.local.json` | your own overrides; gitignored |

## CLAUDE.md files

The root file is short and concrete: the commands (`just test`, one test binary with a
filter), the layout, and the rules that tools enforce (naming, real-time safety, symbol policy,
PR-only `main`, 80% coverage). It also lists what runs automatically, so Claude expects the
hooks instead of being surprised by them.

Subdirectory files hold rules that only matter there: test conventions in `test/`, "edit the
source, then reinstall" in `tooling/`, the workflow conventions in `.github/`. They load only
when Claude reads files in that directory, which keeps the root file small.

## settings.json

```json
{
  "extraKnownMarketplaces": {
    "tanh-tooling": { "source": { "source": "directory", "path": "./tooling" } }
  },
  "enabledPlugins": {
    "tanh-tools@tanh-tooling": true,
    "clangd-lsp@claude-plugins-official": true
  },
  "permissions": {
    "allow": ["Bash(cmake:*)", "Bash(ctest:*)", "Bash(just:*)", "Bash(clang-format:*)", "Bash(clang-tidy:*)", "…"]
  },
  "hooks": { "PreToolUse": ["…protect-installed.sh"], "Stop": ["…build-and-test.sh"] }
}
```

- **Marketplace**: the plugin comes from `./tooling` in this repo instead of GitHub. After
  trusting the folder, install it once: `/plugin install tanh-tools@tanh-tooling`.
- **Permissions**: build, test and lint commands run without a prompt; everything else still asks.

## The tanh-tools plugin

| Part | What |
|---|---|
| hooks | format / lint / typecheck after every edit ([hooks.md](hooks.md)) |
| agent `dsp-reviewer` | reviews DSP and real-time code: allocations and locks on the audio thread, denormals, filter stability, parameter smoothing |
| skill `crossplatform-audio` | CoreAudio, AAudio/Oboe, PipeWire, Bluetooth routing, real-time rules |
| MCP `github` | GitHub API; reads `GITHUB_TOKEN` from the environment (`export GITHUB_TOKEN="$(gh auth token)"`) |
| dependency `clangd-lsp` | go-to-definition and diagnostics from `compile_commands.json` |

After changing the plugin, bump `version` in `plugin.json`; Claude Code keeps the cached copy
otherwise.

## The add-processor skill

`/add-processor` (or Claude picks it when you ask for a new filter or effect) walks the whole
change: header with `TPL_API` and the `prepare`/`process`/`reset` lifecycle, source, tests,
CMake wiring, changelog, then a `dsp-reviewer` pass. It is the pattern for any repeated
multi-file task: write the checklist once, and every run follows it.

## The loop this gives

1. You ask for a change.
2. Every edit is tidied and formatted immediately; tidy findings go straight back to Claude.
3. When Claude wants to finish, the Stop hook builds and runs the tests; red sends it back.
4. You push a branch and open a PR; CI runs the same checks on three OSes, plus sanitizers and
   the per-file coverage gate. `main` only takes a green PR.
