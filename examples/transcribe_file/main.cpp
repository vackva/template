// tpl-transcribe: offline speech-to-text of a 16 kHz mono WAV file with tpl::stt.
//
//   tpl-transcribe speech.wav [--model <dir>] [--threads <n>] [--repeat <n>]
//
// Prints the transcript, then load time and real-time factor (RTF = compute time / audio
// duration, averaged over --repeat runs) — the local CPU benchmark.

#include <dr_wav.h>

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "tpl/stt/Transcriber.h"

namespace {

struct Options {
    std::string m_wav;
    tpl::stt::TranscriberConfig m_config;
    int m_repeat = 1;
};

int usage() {
    std::fprintf(
        stderr,
        "usage: tpl-transcribe <file.wav> [--model <dir>] [--threads <n>] [--repeat <n>]\n");
    return 2;
}

bool parse_int(const char* text, int& value) {
    const char* end = text + std::strlen(text);
    return std::from_chars(text, end, value).ptr == end && value > 0;
}

bool parse(std::span<char*> args, Options& options) {
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view arg = args[i];
        const bool has_value = i + 1 < args.size();
        if (arg == "--model" && has_value) {
            options.m_config.m_model_dir = args[++i];
        } else if (arg == "--threads" && has_value) {
            if (!parse_int(args[++i], options.m_config.m_num_threads)) { return false; }
        } else if (arg == "--repeat" && has_value) {
            if (!parse_int(args[++i], options.m_repeat)) { return false; }
        } else if (!arg.starts_with("--") && options.m_wav.empty()) {
            options.m_wav = arg;
        } else {
            return false;
        }
    }
    return !options.m_wav.empty();
}

std::vector<float> read_wav(const std::string& path) {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 frames = 0;
    auto free_samples = [](float* p) { drwav_free(p, nullptr); };
    const std::unique_ptr<float, decltype(free_samples)> data(
        drwav_open_file_and_read_pcm_frames_f32(path.c_str(),
                                                &channels,
                                                &sample_rate,
                                                &frames,
                                                nullptr),
        free_samples);
    if (!data) { throw std::runtime_error("cannot read " + path); }
    if (channels != 1 || sample_rate != tpl::stt::Transcriber::k_sample_rate) {
        throw std::runtime_error(path + ": expected 16 kHz mono, got " +
                                 std::to_string(sample_rate) + " Hz, " + std::to_string(channels) +
                                 " channels");
    }
    return {data.get(), data.get() + frames};
}

int run(std::span<char*> args) {
    Options options;
    if (!parse(args, options)) { return usage(); }
    using Clock = std::chrono::steady_clock;
    const auto samples = read_wav(options.m_wav);

    const auto load_start = Clock::now();
    tpl::stt::Transcriber transcriber(options.m_config);
    const std::chrono::duration<double> load_time = Clock::now() - load_start;

    tpl::stt::Transcript transcript;
    const auto run_start = Clock::now();
    for (int i = 0; i < options.m_repeat; ++i) { transcript = transcriber.transcribe(samples); }
    const std::chrono::duration<double> run_time = (Clock::now() - run_start) / options.m_repeat;

    const double audio_seconds =
        static_cast<double>(samples.size()) / tpl::stt::Transcriber::k_sample_rate;
    std::printf("%s\n", transcript.m_text.c_str());
    std::fprintf(
        stderr,
        "audio %.2f s | load %.2f s | transcribe %.3f s | RTF %.3f | threads %d | repeat %d\n",
        audio_seconds,
        load_time.count(),
        run_time.count(),
        run_time.count() / audio_seconds,
        options.m_config.m_num_threads,
        options.m_repeat);
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        return run(std::span(argv, static_cast<std::size_t>(argc)));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "tpl-transcribe: %s\n", e.what());
    } catch (...) { std::fprintf(stderr, "tpl-transcribe: unknown error\n"); }
    return 1;
}
