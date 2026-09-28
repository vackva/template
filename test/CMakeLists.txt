# GoogleTest suites, one executable per component. Binaries are named test_<component>
# and live directly under <build>/test/ — the coverage workflow collects them by that
# name (reusable-coverage.yml: find <build>/test -name 'test_*').
include(${PROJECT_SOURCE_DIR}/cmake/tanh/test-deps.cmake)
tanh_fetch_googletest()

# tpl_add_test(<component> <sources>...)
function(tpl_add_test component)
    set(_target test_${component})
    add_executable(${_target} ${ARGN})
    target_link_libraries(${_target} PRIVATE tpl::dsp GTest::gtest_main)
    set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    tanh_copy_runtime_dlls(${_target})
    gtest_discover_tests(${_target} DISCOVERY_MODE PRE_TEST)
endfunction()

tpl_add_test(dsp
    dsp/test_SmoothedValue.cpp
    dsp/test_Gain.cpp
    dsp/test_OnePoleLowpass.cpp)

# A shared tpl_dsp must export exactly tpl:: — nothing of libstdc++, nothing else.
# (A static archive has no export table; the ci-tests-shared leg runs this.)
if(BUILD_SHARED_LIBS)
    include(${PROJECT_SOURCE_DIR}/cmake/tanh/check-exports.cmake)
    tanh_add_export_check(NAME tpl_dsp_exports LIBRARY tpl_dsp NAMESPACES tpl)
endif()
