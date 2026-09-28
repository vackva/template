# test/ — GoogleTest suites

- One executable per component: `test_<component>` (e.g. `test_dsp`), registered with
  `tpl_add_test(<component> <sources>…)` in `test/CMakeLists.txt`. The `test_` prefix and the
  `<build>/test/` location are load-bearing: CI's coverage job collects binaries by that name.
- One file per class: `test/<component>/test_<Class>.cpp`, suites named after the class,
  everything inside an anonymous namespace.
- `gtest_discover_tests` turns every `TEST` into its own CTest entry, so `ctest -R 'Gain\.'`
  and `--gtest_filter` both work.
- Test behaviour, not implementation: measure the response (gain at a frequency, ramp
  monotonicity, exact endpoint), cover edge cases (zero, negative, above Nyquist, infinities),
  and `reset()`. Floats: `EXPECT_FLOAT_EQ` for exact endpoints, `EXPECT_NEAR` with a stated
  tolerance otherwise.
- No sleeps, no randomness without a fixed seed, no files outside the build tree — the suite
  runs under ASan/UBSan/TSan/RTSan and in parallel (`ctest -j`).
- GoogleTest comes from `tanh_fetch_googletest()` (`cmake/tanh/test-deps.cmake`); do not add a
  second FetchContent for it.
