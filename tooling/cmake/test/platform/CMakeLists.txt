cmake_minimum_required(VERSION 3.18)
project(tanh_platform_test NONE)
enable_testing()

include(${CMAKE_CURRENT_LIST_DIR}/../../platform.cmake)

message(STATUS "TANH_OPERATING_SYSTEM=${TANH_OPERATING_SYSTEM} TANH_BINARY_FORMAT=${TANH_BINARY_FORMAT} "
               "TANH_IOS_PLATFORM='${TANH_IOS_PLATFORM}' defs=${TANH_PLATFORM_COMPILE_DEFINITIONS} "
               "modules=${TANH_CMAKE_MODULES_VERSION}")

function(expect var value)
    if(NOT "${${var}}" STREQUAL "${value}")
        message(FATAL_ERROR "${var} = '${${var}}', expected '${value}' (CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME})")
    endif()
endfunction()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    expect(TANH_OPERATING_SYSTEM Linux)
    expect(TANH_BINARY_FORMAT ELF)
    expect(TANH_PLATFORM_COMPILE_DEFINITIONS "THL_PLATFORM_LINUX=1")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    expect(TANH_OPERATING_SYSTEM macOS)
    expect(TANH_BINARY_FORMAT Mach-O)
    expect(TANH_PLATFORM_COMPILE_DEFINITIONS "THL_PLATFORM_MACOS=1")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    expect(TANH_OPERATING_SYSTEM Windows)
    expect(TANH_BINARY_FORMAT PE)
    expect(TANH_PLATFORM_COMPILE_DEFINITIONS "THL_PLATFORM_WINDOWS=1")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    expect(TANH_OPERATING_SYSTEM iOS)
    expect(TANH_BINARY_FORMAT Mach-O)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    expect(TANH_OPERATING_SYSTEM Android)
    expect(TANH_BINARY_FORMAT ELF)
endif()
expect(TANH_IOS_PLATFORM "")
if(NOT TANH_CMAKE_MODULES_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "TANH_CMAKE_MODULES_VERSION not set")
endif()

# A second include is a no-op (include_guard), not a second evaluation.
include(${CMAKE_CURRENT_LIST_DIR}/../../platform.cmake)

add_test(NAME platform-configured COMMAND ${CMAKE_COMMAND} -E true)
