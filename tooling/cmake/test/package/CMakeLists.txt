cmake_minimum_required(VERSION 3.18)
project(tanh_package_test VERSION 2.1.0 LANGUAGES CXX)
enable_testing()
include(${CMAKE_CURRENT_LIST_DIR}/../../package.cmake)

file(WRITE "${CMAKE_BINARY_DIR}/lib.cpp" "int f() { return 1; }\n")
file(WRITE "${CMAKE_BINARY_DIR}/LICENSE" "license\n")
file(WRITE "${CMAKE_BINARY_DIR}/README.md" "readme\n")
add_library(pkg_lib "${CMAKE_BINARY_DIR}/lib.cpp")
install(TARGETS pkg_lib LIBRARY DESTINATION lib COMPONENT runtime ARCHIVE DESTINATION lib COMPONENT dev)

tanh_cpack_debian(
    VENDOR "tanh-lab" CONTACT "dev@example.org"
    RUNTIME_DESCRIPTION "a library" DEV_DESCRIPTION "headers for the library"
    LICENSE_FILE "${CMAKE_BINARY_DIR}/LICENSE" README_FILE "${CMAKE_BINARY_DIR}/README.md"
    DEPS_COMPONENTS extra)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/CPackConfig.cmake")
    message(FATAL_ERROR "include(CPack) did not run")
endif()
file(READ "${CMAKE_BINARY_DIR}/CPackConfig.cmake" _cfg)
foreach(_needle "libtanh_package_test2" "tanh-lab" "dev@example.org" "a library")
    if(NOT _cfg MATCHES "${_needle}")
        message(FATAL_ERROR "CPackConfig.cmake lacks '${_needle}'")
    endif()
endforeach()
add_test(NAME package-configured COMMAND ${CMAKE_COMMAND} -E true)
