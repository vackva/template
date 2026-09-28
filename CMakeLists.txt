cmake_minimum_required(VERSION 3.25)

# Version from the nearest vX.Y.Z tag (0.0.0 without one) — before project().
include(cmake/tanh/git-version.cmake)
tanh_git_version(${CMAKE_CURRENT_SOURCE_DIR})

project(tpl VERSION ${TANH_VERSION_SHORT} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)  # clangd, clang-tidy, the Claude lint hook

option(TPL_WITH_TESTS "Build the GoogleTest suites" ${PROJECT_IS_TOP_LEVEL})
option(TPL_WITH_INSTALL "Generate install rules and the tpl CMake package" ${PROJECT_IS_TOP_LEVEL})
set(TPL_SANITIZERS "" CACHE STRING "Sanitizers for tpl_dsp and its tests, e.g. \"asan;ubsan\" (rtsan, asan, ubsan, tsan, msan, lsan)")

include(cmake/tanh/platform.cmake)
include(cmake/tanh/apple.cmake)
# macOS: put -isysroot into compile_commands.json, or a Homebrew clang-tidy (CI's
# clang_tidy job, the Claude lint hook, the pre-push hook) cannot find <span>.
tanh_apple_sysroot_from_xcrun()
include(cmake/tanh/symbol-policy.cmake)
include(cmake/tanh/sanitizers.cmake)

message(STATUS "tpl ${TANH_VERSION_FULL} on ${TANH_OPERATING_SYSTEM} (${TANH_BINARY_FORMAT}), shared=${BUILD_SHARED_LIBS}")

# ------------------------------------------------------------------------------
# tpl_dsp — the library
# ------------------------------------------------------------------------------
add_library(tpl_dsp
    src/dsp/Gain.cpp
    src/dsp/OnePoleLowpass.cpp)
add_library(tpl::dsp ALIAS tpl_dsp)
target_include_directories(tpl_dsp PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(tpl_dsp PUBLIC ${TANH_PLATFORM_COMPILE_DEFINITIONS})
target_compile_options(tpl_dsp PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX,-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>)

# Hidden visibility + TPL_BUILDING/TPL_STATIC for include/tpl/Exports.h, and an
# export table pinned to namespace tpl (test/ checks the real table).
tanh_apply_symbol_policy(tpl_dsp EXPORT_PREFIX TPL)
tanh_set_export_allowlist(tpl_dsp NAMESPACE tpl)

foreach(_san IN LISTS TPL_SANITIZERS)
    string(TOUPPER "${_san}" _san_upper)
    tanh_add_sanitizer(tpl_dsp ${_san} DEFINE TPL_WITH_${_san_upper})
endforeach()

if(TPL_WITH_INSTALL)
    include(cmake/install.cmake)
endif()

if(TPL_WITH_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
