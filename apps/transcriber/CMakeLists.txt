# Transcriber: JUCE plugin (VST3, AU) + standalone app on tpl_transcript and tpl_stt.
# The processor owns the recording (backend, service, store); the editor is only a view.

include(FetchContent)

# JUCE 9 (AGPLv3 / commercial JUCE licence).
FetchContent_Declare(juce
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/9.0.3.tar.gz
    URL_HASH SHA256=a81e5508b8a0efa483917794ebeaff56aed3075730c405a947734413f40c1aba)
FetchContent_MakeAvailable(juce)

set(_formats VST3 Standalone)
if(TANH_OPERATING_SYSTEM STREQUAL "macOS")
    list(APPEND _formats AU)
endif()

juce_add_plugin(TplTranscriber
    COMPANY_NAME "tpl"
    COMPANY_WEBSITE "https://github.com/vackva/template"
    BUNDLE_ID "com.tpl.transcriber"
    PLUGIN_MANUFACTURER_CODE Tpl0
    PLUGIN_CODE Tscr
    PRODUCT_NAME "Transcriber"
    DESCRIPTION "Speech-to-text transcripts of what passes through"
    VERSION ${PROJECT_VERSION}
    FORMATS ${_formats}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Transcriber listens to the microphone to transcribe speech."
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(TplTranscriber PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SessionSidebar.cpp
    Source/SettingsPanel.cpp
    Source/Theme.cpp
    Source/TranscriptView.cpp)

target_compile_definitions(TplTranscriber PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
# Last-resort model location for development builds (the repository's models/ folder).
target_compile_definitions(TplTranscriber PRIVATE
    TPL_TRANSCRIBER_DEV_MODEL_DIR="${TPL_STT_MODEL_DIR}"
    TPL_TRANSCRIBER_DEV_VAD_MODEL="${TPL_STT_VAD_DIR}/silero_vad.onnx")

target_link_libraries(TplTranscriber
    PRIVATE
        tpl::transcript
        tpl::stt
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
