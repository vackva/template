# Changelog

All notable changes to this project are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versions follow git tags (`vX.Y.Z`).

## [Unreleased]

### Added

- `tpl::dsp::Gain` (smoothed gain, dB helper), `tpl::dsp::OnePoleLowpass`, `tpl::dsp::SmoothedValue`.
- GoogleTest suites, export-table check, sanitizer and coverage presets.
- In-repo tanh-tooling (`tooling/`) and tanh-lab/ci-actions (`.github/actions`, `reusable-*.yml`).
- Install rules and a `tpl` CMake package (`find_package(tpl)`, target `tpl::dsp`).
- `on_tag.yml`: a `vX.Y.Z` tag runs every check in full and publishes per-platform packages.
- Pull-request coverage gate: every changed `src/`/`include/` file needs >= 80% line coverage.
- Claude Code setup: CLAUDE.md files, tanh-tools plugin via a local marketplace, protect/stop hooks, `add-processor` skill.
