#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace tpl::stt::test {

/// Paths baked in by test/CMakeLists.txt.
std::filesystem::path data_dir();
std::filesystem::path model_dir();
std::filesystem::path binary_dir();

/// Reads a 16 kHz mono WAV file; fails the calling test (ADD_FAILURE) on another format.
std::vector<float> read_wav_16k_mono(const std::filesystem::path& path);

/// Levenshtein distance between two sequences (substitutions + deletions + insertions).
template <typename T>
std::size_t edit_distance(const std::vector<T>& reference, const std::vector<T>& hypothesis) {
    std::vector<std::size_t> row(hypothesis.size() + 1);
    for (std::size_t j = 0; j <= hypothesis.size(); ++j) { row[j] = j; }
    for (std::size_t i = 1; i <= reference.size(); ++i) {
        std::size_t diagonal = row[0];
        row[0] = i;
        for (std::size_t j = 1; j <= hypothesis.size(); ++j) {
            const std::size_t above = row[j];
            row[j] = std::min({row[j] + 1,
                               row[j - 1] + 1,
                               diagonal + (reference[i - 1] == hypothesis[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[hypothesis.size()];
}

/// Word error rate of `hypothesis` against `reference` after lowercasing (ASCII and
/// German umlauts) and dropping punctuation: (substitutions + deletions + insertions) /
/// reference words.
double word_error_rate(const std::string& reference, const std::string& hypothesis);

}  // namespace tpl::stt::test
