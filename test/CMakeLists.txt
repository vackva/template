# GoogleTest suites, one executable per component. Binaries are named test_<component>
# and live directly under <build>/test/ — the coverage workflow collects them by that
# name (reusable-coverage.yml: find <build>/test -name 'test_*').
include(${PROJECT_SOURCE_DIR}/cmake/tanh/test-deps.cmake)
tanh_fetch_googletest()

# tpl_add_test(<component> SOURCES <sources>... [LIBS <libraries>...])
function(tpl_add_test component)
    cmake_parse_arguments(PARSE_ARGV 1 _arg "" "" "SOURCES;LIBS")
    set(_target test_${component})
    add_executable(${_target} ${_arg_SOURCES})
    target_link_libraries(${_target} PRIVATE ${_arg_LIBS} GTest::gtest_main)
    set_target_properties(${_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    tanh_copy_runtime_dlls(${_target})
    gtest_discover_tests(${_target} DISCOVERY_MODE PRE_TEST)
endfunction()

tpl_add_test(dsp
    SOURCES
        dsp/test_SmoothedValue.cpp
        dsp/test_Gain.cpp
        dsp/test_OnePoleLowpass.cpp
    LIBS tpl::dsp)

if(TPL_WITH_STT)
    tpl_add_test(stt
        SOURCES
            stt/TestAudio.cpp
            stt/test_Resampler.cpp
            stt/test_SpscRing.cpp
            stt/test_StreamingTranscriber.cpp
            stt/test_TdtGreedyDecoder.cpp
            stt/test_Transcriber.cpp
            stt/test_VadSegmenter.cpp
            stt/test_Vocabulary.cpp
            stt/test_Words.cpp
        LIBS tpl::stt tpl_dr_wav nlohmann_json::nlohmann_json)
    target_compile_definitions(test_stt PRIVATE
        TPL_STT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/stt"
        TPL_STT_TEST_MODEL_DIR="${TPL_STT_MODEL_DIR}"
        TPL_STT_TEST_VAD_MODEL="${TPL_STT_VAD_DIR}/silero_vad.onnx"
        TPL_STT_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
endif()

if(TPL_WITH_STT)
    tpl_add_test(transcript
        SOURCES
            transcript/test_Export.cpp
            transcript/test_ParagraphLayout.cpp
            transcript/test_ReplaySegmentSource.cpp
            transcript/test_RowIndex.cpp
            transcript/test_SegmentCache.cpp
            transcript/test_Settings.cpp
            transcript/test_TranscriptStore.cpp
            transcript/test_TranscriptionService.cpp
        LIBS tpl::transcript)
    target_compile_definitions(test_transcript PRIVATE
        TPL_TRANSCRIPT_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}")
endif()

# A shared tpl_dsp must export exactly tpl:: — nothing of libstdc++, nothing else.
# (A static archive has no export table; the ci-tests-shared leg runs this.)
if(BUILD_SHARED_LIBS)
    include(${PROJECT_SOURCE_DIR}/cmake/tanh/check-exports.cmake)
    tanh_add_export_check(NAME tpl_dsp_exports LIBRARY tpl_dsp NAMESPACES tpl)
    if(TPL_WITH_STT)
        tanh_add_export_check(NAME tpl_stt_exports LIBRARY tpl_stt NAMESPACES tpl)
        tanh_add_export_check(NAME tpl_transcript_exports LIBRARY tpl_transcript NAMESPACES tpl)
    endif()
endif()
