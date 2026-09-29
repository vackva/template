#include "tpl/transcript/Export.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <span>
#include <string>

#include "tpl/transcript/Types.h"

namespace tpl::transcript {

namespace {

std::string paragraphs(std::span<const StoredSegment> segments) {
    std::string out;
    for (const auto& segment : segments) {
        if (segment.m_text.empty()) { continue; }
        if (!out.empty()) { out += "\n\n"; }
        out += segment.m_text;
    }
    if (!out.empty()) { out += '\n'; }
    return out;
}

}  // namespace

std::string export_text(std::span<const StoredSegment> segments) {
    return paragraphs(segments);
}

std::string export_markdown(const SessionInfo& session, std::span<const StoredSegment> segments) {
    std::string out =
        "# " + session.m_title + "\n\n_" + format_local_date_time(session.m_started_utc_ms) + "_\n";
    const auto body = paragraphs(segments);
    if (!body.empty()) { out += "\n" + body; }
    return out;
}

std::string export_srt(std::span<const StoredSegment> segments) {
    std::string out;
    int cue = 0;
    for (const auto& segment : segments) {
        if (segment.m_text.empty()) { continue; }
        if (cue > 0) { out += '\n'; }
        out += std::to_string(++cue) + '\n' + format_srt_time(segment.m_start_ms) + " --> " +
               format_srt_time(segment.m_end_ms) + '\n' + segment.m_text + '\n';
    }
    return out;
}

std::string format_srt_time(std::int64_t ms) {
    if (ms < 0) { ms = 0; }
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(),
                  buffer.size(),
                  "%02lld:%02lld:%02lld,%03lld",
                  static_cast<long long>(ms / 3'600'000),
                  static_cast<long long>((ms / 60'000) % 60),
                  static_cast<long long>((ms / 1000) % 60),
                  static_cast<long long>(ms % 1000));
    return buffer.data();
}

std::string format_local_date_time(std::int64_t utc_ms) {
    const auto seconds = static_cast<std::time_t>(utc_ms / 1000);
    std::tm local{};
#if defined(THL_PLATFORM_WINDOWS)
    localtime_s(&local, &seconds);
#else
    localtime_r(&seconds, &local);
#endif
    std::array<char, 32> buffer{};
    std::strftime(buffer.data(), buffer.size(), "%Y-%m-%d %H:%M", &local);
    return buffer.data();
}

}  // namespace tpl::transcript
