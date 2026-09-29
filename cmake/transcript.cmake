# tpl_transcript: sessions and segments of the transcription app — SQLite storage with
# full-text search, the recording service, export and the transcript view's layout index.
# No JUCE: everything here is unit-tested and covered by the coverage gate.

# ------------------------------------------------------------------------------
# SQLite amalgamation (public domain), compiled in with hidden symbols
# ------------------------------------------------------------------------------
enable_language(C)
include(FetchContent)
FetchContent_Declare(sqlite3
    URL https://www.sqlite.org/2026/sqlite-amalgamation-3530400.zip
    URL_HASH SHA3_256=628a44cfe82c66aed1ccbbe85a562d2e33ebe64b3288981ed76285612227934e)
FetchContent_MakeAvailable(sqlite3)

add_library(tpl_sqlite3 STATIC ${sqlite3_SOURCE_DIR}/sqlite3.c)
target_include_directories(tpl_sqlite3 SYSTEM PUBLIC ${sqlite3_SOURCE_DIR})
target_compile_definitions(tpl_sqlite3 PRIVATE
    SQLITE_ENABLE_FTS5
    SQLITE_THREADSAFE=1
    SQLITE_DQS=0
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DEFAULT_FOREIGN_KEYS=1)
# Never exported from tpl_transcript, never visible to a host that ships its own SQLite.
set_target_properties(tpl_sqlite3 PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden)
if(NOT TANH_OPERATING_SYSTEM STREQUAL "Windows")
    target_link_libraries(tpl_sqlite3 PRIVATE ${CMAKE_DL_LIBS})
    find_package(Threads REQUIRED)
    target_link_libraries(tpl_sqlite3 PUBLIC Threads::Threads)
endif()

# ------------------------------------------------------------------------------
# The library
# ------------------------------------------------------------------------------
add_library(tpl_transcript
    src/transcript/Export.cpp
    src/transcript/ParagraphLayout.cpp
    src/transcript/ReplaySegmentSource.cpp
    src/transcript/RowIndex.cpp
    src/transcript/SegmentCache.cpp
    src/transcript/Settings.cpp
    src/transcript/Sqlite.cpp
    src/transcript/TranscriptStore.cpp
    src/transcript/TranscriptionService.cpp)
add_library(tpl::transcript ALIAS tpl_transcript)
target_include_directories(tpl_transcript PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(tpl_transcript PUBLIC ${TANH_PLATFORM_COMPILE_DEFINITIONS})
# C4251: std:: members of exported classes; library and consumers share one toolchain.
target_compile_options(tpl_transcript PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX /wd4251,-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>)
target_link_libraries(tpl_transcript
    PUBLIC tpl::stt
    PRIVATE tpl_sqlite3 nlohmann_json::nlohmann_json)
find_package(Threads REQUIRED)
target_link_libraries(tpl_transcript PUBLIC Threads::Threads)

tanh_apply_symbol_policy(tpl_transcript EXPORT_PREFIX TPL)
tanh_set_export_allowlist(tpl_transcript NAMESPACE tpl)

foreach(_san IN LISTS TPL_SANITIZERS)
    string(TOUPPER "${_san}" _san_upper)
    tanh_add_sanitizer(tpl_transcript ${_san} DEFINE TPL_WITH_${_san_upper})
endforeach()
