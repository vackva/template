#include "tpl/stt/Vocabulary.h"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace tpl::stt {

namespace {

constexpr std::string_view k_word_marker = "\xE2\x96\x81";  // U+2581 LOWER ONE EIGHTH BLOCK

/// Decodes the UTF-8 code point at `text[pos]`; malformed bytes decode as themselves.
char32_t code_point_at(std::string_view text, std::size_t pos) {
    const auto byte = static_cast<unsigned char>(text[pos]);
    auto continuation = [&](std::size_t offset) {
        return pos + offset < text.size()
                   ? static_cast<char32_t>(static_cast<unsigned char>(text[pos + offset]) & 0x3Fu)
                   : char32_t{0};
    };
    if (byte < 0x80u) { return byte; }
    if ((byte & 0xE0u) == 0xC0u) {
        return (static_cast<char32_t>(byte & 0x1Fu) << 6u) | continuation(1);
    }
    if ((byte & 0xF0u) == 0xE0u) {
        return (static_cast<char32_t>(byte & 0x0Fu) << 12u) | (continuation(1) << 6u) |
               continuation(2);
    }
    if ((byte & 0xF8u) == 0xF0u) {
        return (static_cast<char32_t>(byte & 0x07u) << 18u) | (continuation(1) << 12u) |
               (continuation(2) << 6u) | continuation(3);
    }
    return byte;
}

/// Approximates Python's Unicode `\w` for the scripts Parakeet emits: ASCII letters, digits
/// and '_' plus any non-ASCII code point outside the Latin-1 and general punctuation blocks.
bool is_word_char(char32_t c) {
    if (c < 0x80u) {
        return (c >= U'a' && c <= U'z') || (c >= U'A' && c <= U'Z') || (c >= U'0' && c <= U'9') ||
               c == U'_';
    }
    const bool latin1_punctuation = c <= 0xBFu || c == 0xD7u || c == 0xF7u;
    const bool general_punctuation = c >= 0x2000u && c <= 0x206Fu;
    return !latin1_punctuation && !general_punctuation;
}

std::string replace_word_markers(std::string_view token) {
    std::string out;
    out.reserve(token.size());
    std::size_t pos = 0;
    while (pos < token.size()) {
        if (token.substr(pos, k_word_marker.size()) == k_word_marker) {
            out += ' ';
            pos += k_word_marker.size();
        } else {
            out += token[pos++];
        }
    }
    return out;
}

}  // namespace

Vocabulary::Vocabulary(std::vector<std::string> tokens) : m_tokens(std::move(tokens)) {
    if (m_tokens.empty()) { throw std::invalid_argument("Vocabulary: no tokens"); }
}

Vocabulary Vocabulary::load(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) { throw std::runtime_error("Vocabulary: cannot open " + path.string()); }
    std::vector<std::string> tokens;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') { line.pop_back(); }
        const auto separator = line.rfind(' ');
        std::size_t id = 0;
        // Check the separator before forming id_begin: npos + 1 would overflow the pointer.
        const bool well_formed = separator != std::string::npos && separator != 0 && [&] {
            const char* id_begin = line.data() + separator + 1;
            const char* id_end = line.data() + line.size();
            return std::from_chars(id_begin, id_end, id).ptr == id_end;
        }();
        if (!well_formed || id != tokens.size()) {
            throw std::runtime_error("Vocabulary: malformed line " +
                                     std::to_string(tokens.size() + 1) + " in " + path.string());
        }
        tokens.push_back(line.substr(0, separator));
    }
    if (tokens.empty()) { throw std::runtime_error("Vocabulary: " + path.string() + " is empty"); }
    return Vocabulary(std::move(tokens));
}

std::int32_t Vocabulary::blank_id() const noexcept {
    return static_cast<std::int32_t>(m_tokens.size() - 1);
}

std::string_view Vocabulary::token(std::int32_t id) const {
    if (id < 0 || static_cast<std::size_t>(id) >= m_tokens.size()) {
        throw std::out_of_range("Vocabulary: token id " + std::to_string(id) + " out of range");
    }
    return m_tokens[static_cast<std::size_t>(id)];
}

std::string Vocabulary::decode(std::span<const std::int32_t> ids) const {
    std::string joined;
    for (const auto id : ids) { joined += replace_word_markers(token(id)); }

    // onnx-asr: re.sub(r"\A\s|\s\B|(\s)\b", ...) — a space survives only when a word
    // character follows it and it is not the first character.
    std::string text;
    text.reserve(joined.size());
    for (std::size_t pos = 0; pos < joined.size(); ++pos) {
        if (joined[pos] != ' ') {
            text += joined[pos];
            continue;
        }
        const bool keep =
            pos != 0 && pos + 1 < joined.size() && is_word_char(code_point_at(joined, pos + 1));
        if (keep) { text += ' '; }
    }
    return text;
}

}  // namespace tpl::stt
