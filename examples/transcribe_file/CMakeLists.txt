# tpl-transcribe <file.wav> [--model <dir>] [--threads <n>] [--repeat <n>]
# Transcribes a 16 kHz mono WAV file and prints the text and the real-time factor.
add_executable(tpl_transcribe main.cpp)
set_target_properties(tpl_transcribe PROPERTIES OUTPUT_NAME tpl-transcribe)
target_link_libraries(tpl_transcribe PRIVATE tpl::stt tpl_dr_wav)
target_compile_options(tpl_transcribe PRIVATE
    $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX,-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror>)
include(${PROJECT_SOURCE_DIR}/cmake/tanh/test-deps.cmake)
tanh_copy_runtime_dlls(tpl_transcribe)
