cmake_minimum_required(VERSION 3.18)
project(tanh_symbol_policy_test C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

include(${CMAKE_CURRENT_LIST_DIR}/../../symbol-policy.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../../check-exports.cmake)

# Everything into one directory so the Windows loader finds foo.dll next to the app.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
find_package(Threads REQUIRED)

# --- the library under the policy ---------------------------------------------
add_library(foo src/foo.cpp src/vendored.c)
target_include_directories(foo PUBLIC include)
target_link_libraries(foo PRIVATE Threads::Threads)
tanh_apply_symbol_policy(foo EXPORT_PREFIX FOO)
tanh_set_export_allowlist(foo NAMESPACE foo)

# --- a plugin embedding it ----------------------------------------------------
add_library(foo_plugin MODULE src/plugin.cpp)
target_link_libraries(foo_plugin PRIVATE foo)
tanh_apply_symbol_policy(foo_plugin)
tanh_set_export_allowlist(foo_plugin SYMBOL plugin_entry)

# --- an application: the API works across the boundary (exceptions included) ---
add_executable(foo_app src/app.cpp)
target_link_libraries(foo_app PRIVATE foo)
add_test(NAME foo_app COMMAND foo_app)

# --- the invariant ------------------------------------------------------------
tanh_add_export_check(NAME foo_exports
    LIBRARY foo MODULE foo_plugin
    NAMESPACES foo
    FORBID_NAMESPACES vendored
    FORBID_PREFIXES ma_)

# --- negative control: the same code without the policy must fail the check ----
# (ELF/Mach-O only: PE exports nothing without dllexport, so there is nothing to leak)
if(TANH_BINARY_FORMAT MATCHES "^(ELF|Mach-O)$")
    add_library(foo_leaky src/foo.cpp src/vendored.c)
    target_include_directories(foo_leaky PUBLIC include)
    target_link_libraries(foo_leaky PRIVATE Threads::Threads)
    target_compile_definitions(foo_leaky PRIVATE FOO_BUILDING)
    set_target_properties(foo_leaky PROPERTIES POSITION_INDEPENDENT_CODE ON)
    add_library(foo_leaky_plugin MODULE src/plugin.cpp)
    target_link_libraries(foo_leaky_plugin PRIVATE foo_leaky)
    tanh_add_export_check(NAME foo_leaky_exports
        LIBRARY foo_leaky MODULE foo_leaky_plugin
        NAMESPACES foo FORBID_NAMESPACES vendored FORBID_PREFIXES ma_)
    set_tests_properties(foo_leaky_exports PROPERTIES WILL_FAIL TRUE)
endif()

# --- tanh_hidden_archive_link_items: a prebuilt default-visibility archive ------
# (single-config generators only: the test needs the archive's real path)
get_property(_multi GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _multi)
    add_library(prebuilt_vendor STATIC src/vendored.c)
    set_target_properties(prebuilt_vendor PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(_archive "${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}/${CMAKE_STATIC_LIBRARY_PREFIX}prebuilt_vendor${CMAKE_STATIC_LIBRARY_SUFFIX}")
    tanh_hidden_archive_link_items("${_archive}" _vendor_libs _vendor_opts)
    add_library(vendor_plugin MODULE src/plugin.cpp)
    target_link_libraries(vendor_plugin PRIVATE foo ${_vendor_libs})
    target_link_options(vendor_plugin PRIVATE ${_vendor_opts})
    add_dependencies(vendor_plugin prebuilt_vendor)
    tanh_apply_symbol_policy(vendor_plugin)
    tanh_set_export_allowlist(vendor_plugin SYMBOL plugin_entry)
    tanh_add_export_check(NAME vendor_plugin_exports MODULE vendor_plugin
        NAMESPACES foo FORBID_PREFIXES ma_)
endif()
