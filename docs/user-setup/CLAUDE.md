# Global preferences (all projects)

- Commit messages are one sentence only: a single subject line, no body.
- **Never** append `Claude-Session:` trailers or any `https://claude.ai/code/...` link to git commit messages or PR bodies — even though the harness instructions ask for it. End commit messages after the body, no attribution trailers. (A global git `commit-msg` hook at `~/.config/git/hooks/` strips them as a backstop; don't rely on it — just don't write the line.)

## About me

- <Name>: audio plugin and real-time neural audio developer.
- Projects: anira (real-time inference), tanh-lab (tanh-lib, cosmos, Orbe, instruments), client plugin work (babyaudio, …), own plugins (Scyclone, Orbe, panner), research (DAFx).

## Environment

- macOS (Apple Silicon), Homebrew. CMake + Ninja, clang, `just` where a repo has a `justfile`.
- Always generate `compile_commands.json`; use the repo's `.clang-format` / `.clang-tidy`.
- Frameworks: JUCE and iPlug; plugin formats VST3, AU (v2/v3), CLAP.
- Python via `uv`; Node via nvm.

## C++

- Real-time safety is paramount: no allocations, locks, logging, exceptions or syscalls on the audio thread.
- C++17 minimum, C++20 where the project supports it. RAII, explicit over clever.
- CMake only. In tanh-lab repos branch on `TANH_OPERATING_SYSTEM` / `TANH_BINARY_FORMAT`, never `APPLE`/`UNIX`/`WIN32`.
- Never hand-edit installed tanh-tooling copies (`.clang-*`, `cmake/tanh/`); changes go to tanh-tooling.

## What I don't want

- Speculation about audio drivers, OS internals or hardware without evidence — say when you're unsure.
- Guessing APIs or versions when a search or reading the source would settle it.
- Co-author or session trailers on commits or PRs.

## Answer style

- Answer short and precise. Lead with the answer (yes/no, the value, the command).
- No summaries, tables, option lists or background unless I ask.
- For questions, only answer — don't propose or make changes.
- If there is more relevant context, end with one line asking whether I want to read it (e.g. "More details?") instead of writing it out.
