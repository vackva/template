cmake_minimum_required(VERSION 3.25)
project(tanh_test_deps_test CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
include(${CMAKE_CURRENT_LIST_DIR}/../../test-deps.cmake)

tanh_fetch_googletest()
tanh_fetch_googletest()      # idempotent
tanh_fetch_googlebenchmark()

file(WRITE "${CMAKE_BINARY_DIR}/t.cpp" "#include <gtest/gtest.h>\nTEST(Deps, Works) { EXPECT_EQ(1 + 1, 2); }\n")
add_executable(deps_test "${CMAKE_BINARY_DIR}/t.cpp")
target_link_libraries(deps_test PRIVATE gtest_main)
tanh_copy_runtime_dlls(deps_test)
gtest_discover_tests(deps_test)

file(WRITE "${CMAKE_BINARY_DIR}/b.cpp" "#include <benchmark/benchmark.h>\nstatic void BM(benchmark::State& s) { for (auto _ : s) benchmark::DoNotOptimize(1); }\nBENCHMARK(BM);\nBENCHMARK_MAIN();\n")
add_executable(deps_bench "${CMAKE_BINARY_DIR}/b.cpp")
target_link_libraries(deps_bench PRIVATE benchmark::benchmark)
add_test(NAME deps_bench COMMAND deps_bench --benchmark_min_time=1x)
