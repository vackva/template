# tpl_stt: offline speech-to-text with nvidia/parakeet-tdt-0.6b-v3 (int8 ONNX, exported by
# scripts/export-parakeet). ONNX Runtime comes from anira (cmake/anira.cmake), so the
# offline library and the later anira streaming path share one runtime per process.

# ------------------------------------------------------------------------------
# Model location
# ------------------------------------------------------------------------------
set(TPL_STT_MODEL_NAME "parakeet-tdt-0.6b-v3-int8")
set(TPL_STT_MODEL_DIR "${PROJECT_SOURCE_DIR}/models/${TPL_STT_MODEL_NAME}" CACHE PATH
    "Exported model directory the tests use and the stt_model install component copies")

# System-wide location the installer and `cmake --install <build> --component stt_model`
# put the model; Transcriber's default_model_dir() returns the same path.
if(TANH_OPERATING_SYSTEM STREQUAL "macOS")
    set(_stt_model_root "/Library/Application Support/tpl/models")
elseif(TANH_OPERATING_SYSTEM STREQUAL "Windows")
    set(_stt_model_root "C:/ProgramData/tpl/models")
else()
    set(_stt_model_root "/usr/local/share/tpl/models")
endif()
set(TPL_STT_MODEL_INSTALL_DIR "${_stt_model_root}/${TPL_STT_MODEL_NAME}" CACHE PATH
    "Absolute system-wide model directory (default_model_dir())")

# ------------------------------------------------------------------------------
# The library
# ------------------------------------------------------------------------------
add_library(tpl_stt
    src/stt/OnnxParakeet.cpp
    src/stt/TdtGreedyDecoder.cpp
    src/stt/Transcriber.cpp
    src/stt/Vocabulary.cpp)
add_library(tpl::stt ALIAS tpl_stt)
target_include_directories(tpl_stt PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_compile_definitions(tpl_stt
    PUBLIC ${TANH_PLATFORM_COMPILE_DEFINITIONS}
    PRIVATE TPL_STT_MODEL_INSTALL_DIR="${TPL_STT_MODEL_INSTALL_DIR}")
# C4251: std:: members of exported classes; library and consumers share one toolchain.
target_compile_options(tpl_stt PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX /wd4251,-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>)
target_link_libraries(tpl_stt PRIVATE anira::onnxruntime)

tanh_apply_symbol_policy(tpl_stt EXPORT_PREFIX TPL)
tanh_set_export_allowlist(tpl_stt NAMESPACE tpl)

foreach(_san IN LISTS TPL_SANITIZERS)
    string(TOUPPER "${_san}" _san_upper)
    tanh_add_sanitizer(tpl_stt ${_san} DEFINE TPL_WITH_${_san_upper})
endforeach()

# Only on request (`cmake --install <build> --component stt_model`, needs admin rights):
# a plain `cmake --install` — the release packages — never writes outside its prefix.
install(DIRECTORY "${TPL_STT_MODEL_DIR}/"
    DESTINATION "${TPL_STT_MODEL_INSTALL_DIR}"
    COMPONENT stt_model
    EXCLUDE_FROM_ALL
    FILES_MATCHING PATTERN "*.onnx" PATTERN "*.txt" PATTERN "*.json")

# ------------------------------------------------------------------------------
# dr_wav (MIT-0) — WAV reading for the example and the tests, never for the library
# ------------------------------------------------------------------------------
if(TPL_WITH_EXAMPLES OR TPL_WITH_TESTS)
    include(FetchContent)
    FetchContent_Declare(dr_libs
        URL https://github.com/mackron/dr_libs/archive/dfe8377631000664666519fdb83da193fd8037f4.tar.gz
        URL_HASH SHA256=4654acb029f4f2a43ac2edb60c4cb09f40615b4b5bee9709954f910cb979e5fd)
    FetchContent_MakeAvailable(dr_libs)
    add_library(tpl_dr_wav INTERFACE)
    target_include_directories(tpl_dr_wav SYSTEM INTERFACE ${dr_libs_SOURCE_DIR})
endif()

if(TPL_WITH_EXAMPLES)
    add_subdirectory(examples/transcribe_file)
endif()
