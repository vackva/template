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
- `tpl::stt::Transcriber`: offline speech-to-text with nvidia/parakeet-tdt-0.6b-v3 (int8 ONNX,
  25 languages) over anira's ONNX Runtime; `tpl::stt::Vocabulary`, `tpl::stt::tdt_greedy_decode`,
  `tpl::stt::default_model_dir()`.
- `scripts/export-parakeet`: NeMo -> int8 ONNX export and the golden test transcripts; the model
  lives in `models/` via Git LFS, CI restores it through `.github/actions/models-cache`.
- `tpl-transcribe` example (`examples/transcribe_file`): transcribes a WAV file, prints the RTF.
- anira v2.3.0 as a submodule (`third_party/anira`); options `TPL_WITH_STT`, `TPL_WITH_EXAMPLES`.

### Changed

- `tpl_add_test()` takes `SOURCES` and `LIBS`; coverage covers every `libtpl_*` library.
