# Test driver for the tanh-tooling CMake modules. Each subdirectory is a standalone
# project configured, built and tested through `ctest --build-and-test`, so a module
# is exercised exactly the way a consumer uses it.
#
#   cmake -S cmake/test -B cmake/test/build && ctest --test-dir cmake/test/build --output-on-failure
#
# Compilers come from the environment (CC/CXX) or TANH_TEST_BUILD_OPTIONS
# (e.g. -DTANH_TEST_BUILD_OPTIONS="-DCMAKE_CXX_COMPILER=g++;-DCMAKE_C_COMPILER=gcc").
cmake_minimum_required(VERSION 3.18)
project(tanh_cmake_modules_tests NONE)
enable_testing()

set(TANH_TEST_BUILD_OPTIONS "" CACHE STRING "Extra -D options for the module test projects")

# tanh_module_test(<name> <subdir> [<-Doption>...])
function(tanh_module_test name subdir)
    set(_gen --build-generator "${CMAKE_GENERATOR}")
    if(CMAKE_GENERATOR_PLATFORM)
        list(APPEND _gen --build-generator-platform "${CMAKE_GENERATOR_PLATFORM}")
    endif()
    if(CMAKE_GENERATOR_TOOLSET)
        list(APPEND _gen --build-generator-toolset "${CMAKE_GENERATOR_TOOLSET}")
    endif()
    add_test(NAME ${name}
        COMMAND ${CMAKE_CTEST_COMMAND} --build-and-test
            "${CMAKE_CURRENT_SOURCE_DIR}/${subdir}" "${CMAKE_CURRENT_BINARY_DIR}/${name}"
            ${_gen}
            --build-noclean
            --build-config Release
            --build-options -DCMAKE_BUILD_TYPE=Release ${TANH_TEST_BUILD_OPTIONS} ${ARGN}
            --test-command ${CMAKE_CTEST_COMMAND} -C Release --output-on-failure)
endfunction()

tanh_module_test(platform platform)
tanh_module_test(symbol-policy-shared symbol-policy -DBUILD_SHARED_LIBS=ON)
tanh_module_test(symbol-policy-static symbol-policy -DBUILD_SHARED_LIBS=OFF)
tanh_module_test(git-version git-version)
tanh_module_test(sanitizers sanitizers)
tanh_module_test(test-deps test-deps)
tanh_module_test(apple apple)
tanh_module_test(package package)
tanh_module_test(install-helpers install-helpers)
tanh_module_test(binary-data binary-data)

# platform.cmake refuses to guess before project(): script mode has no CMAKE_SYSTEM_NAME.
add_test(NAME platform-requires-project
    COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/../platform.cmake")
set_tests_properties(platform-requires-project PROPERTIES WILL_FAIL TRUE)
