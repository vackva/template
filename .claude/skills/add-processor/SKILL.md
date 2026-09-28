---
name: add-processor
description: Add a new DSP processor to tpl_dsp end to end — public header, source, GoogleTest suite, CMake wiring, changelog. Use when the user asks for a new filter, effect, gain stage, oscillator or any other audio processor in this repo.
---

# Add a processor to tpl_dsp

Follow the shape of `Gain` / `OnePoleLowpass`; read both before writing.

1. **Header** `include/tpl/dsp/<Name>.h`
   - `class TPL_API <Name>` in `namespace tpl::dsp`, include `tpl/Exports.h`.
   - Lifecycle: `prepare(double sample_rate) noexcept` (not real-time) →
     `process(std::span<float> block) noexcept TPL_NONBLOCKING` → `reset() noexcept`.
   - Doxygen `///` on the class and every public member: units, ranges, what clamps.
2. **Source** `src/dsp/<Name>.cpp`, added to `add_library(tpl_dsp …)` in `CMakeLists.txt`.
   - `process()` must not allocate, lock, log or throw. Precompute coefficients in
     `prepare()`/setters, never per sample if avoidable.
   - Smooth user-facing parameters with `SmoothedValue`.
3. **Tests** `test/dsp/test_<Name>.cpp`, added to `tpl_add_test(dsp …)` in `test/CMakeLists.txt`
   (see `test/CLAUDE.md`): at least one behavioural test (a measured response, not just
   "does not crash"), one edge-case test (zero / extreme parameters), and one for `reset()`.
4. **Changelog** — entry under `## [Unreleased]` in `CHANGELOG.md`.
5. **Verify** — `just test`; then ask the `dsp-reviewer` agent to review the new files
   for real-time safety and numerical issues, and address its 🔴 findings.

The Stop hook rebuilds and runs the suite when you finish; a red build sends you back.
