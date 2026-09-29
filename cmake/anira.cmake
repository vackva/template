# anira v2.3.0 (third_party/anira submodule) with only the ONNX Runtime backend; tpl_stt
# links its anira::onnxruntime target.
#
# Included from the top-level CMakeLists.txt BEFORE the repo's own cmake/tanh modules:
# anira and the tanh-lib it fetches carry tanh-tooling 0.1.5 modules, and CMake functions
# are global, so whichever copy is included last wins. Adding anira first lets it configure
# with the modules it was written for, and the repo's newer copies then take over for tpl's
# own targets. (modules-version.cmake still warns about the mismatch until anira pins the
# same tanh-tooling tag.)

# Plain variables shadow anira's option() defaults (CMP0077 NEW).
set(ANIRA_WITH_ONNXRUNTIME ON)
set(ANIRA_WITH_LIBTORCH OFF)
set(ANIRA_WITH_LITERT OFF)
set(ANIRA_WITH_TFLITE OFF)
set(ANIRA_WITH_EXECUTORCH OFF)
set(ANIRA_WITH_TESTS OFF)
set(ANIRA_WITH_EXAMPLES OFF)
set(ANIRA_WITH_INSTALL OFF)
set(ANIRA_WITH_LOGGING OFF)

if(NOT EXISTS ${PROJECT_SOURCE_DIR}/third_party/anira/CMakeLists.txt)
    message(FATAL_ERROR "third_party/anira is empty: run `git submodule update --init`")
endif()
add_subdirectory(${PROJECT_SOURCE_DIR}/third_party/anira ${PROJECT_BINARY_DIR}/_deps/anira EXCLUDE_FROM_ALL SYSTEM)

