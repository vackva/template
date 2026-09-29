#include "TestAudio.h"

#include <dr_wav.h>
#include <gtest/gtest.h>

#include <cctype>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace tpl::stt::test {

std::filesystem::path data_dir() {
    return TPL_STT_TEST_DATA_DIR;
}
std::filesystem::path model_dir() {
    return TPL_STT_TEST_MODEL_DIR;
}
std::filesystem::path binary_dir() {
    return TPL_STT_TEST_BINARY_DIR;
}

std::vector<float> read_wav_16k_mono(const std::filesystem::path& path) {
    unsigned int channels = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 frames = 0;
    auto free_samples = [](float* p) { drwav_free(p, nullptr); };
    const std::unique_ptr<float, decltype(free_samples)> data(
        drwav_open_file_and_read_pcm_frames_f32(path.string().c_str(),
                                                &channels,
                                                &sample_rate,
                                                &frames,
                                                nullptr),
        free_samples);
    if (!data || channels != 1 || sample_rate != 16000) {
        ADD_FAILURE() << "cannot read " << path << " as 16 kHz mono (" << sample_rate << " Hz, "
                      << channels << " channels)";
        return {};
    }
    return {data.get(), data.get() + frames};
}

namespace {

std::vector<std::string> normalized_words(const std::string& text) {
    // UTF-8 upper-case umlauts -> lower case; everything that is not a letter, digit or
    // a multi-byte character becomes a word separator.
    std::string lowered;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        if (c == 0xC3 && i + 1 < text.size()) {
            auto next = static_cast<unsigned char>(text[i + 1]);
            if (next == 0x84 || next == 0x96 || next == 0x9C) {  // Ä Ö Ü
                next = static_cast<unsigned char>(next + 0x20);
            }
            lowered += static_cast<char>(c);
            lowered += static_cast<char>(next);
            ++i;
        } else if (c >= 0x80) {
            lowered += static_cast<char>(c);
        } else if (std::isalnum(c) != 0) {
            lowered += static_cast<char>(std::tolower(c));
        } else {
            lowered += ' ';
        }
    }
    std::istringstream stream(lowered);
    std::vector<std::string> words;
    for (std::string word; stream >> word;) { words.push_back(word); }
    return words;
}

}  // namespace

double word_error_rate(const std::string& reference, const std::string& hypothesis) {
    const auto ref = normalized_words(reference);
    const auto hyp = normalized_words(hypothesis);
    if (ref.empty()) { return hyp.empty() ? 0.0 : 1.0; }
    return static_cast<double>(edit_distance(ref, hyp)) / static_cast<double>(ref.size());
}

}  // namespace tpl::stt::test
