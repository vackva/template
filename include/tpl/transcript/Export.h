#pragma once

#include <cstdint>
#include <span>
#include <string>

#include "tpl/Exports.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

/// Plain text: one paragraph per segment, separated by blank lines.
[[nodiscard]] TPL_API std::string export_text(std::span<const StoredSegment> segments);

/// Markdown: "# <title>", the start date and time, then the paragraphs.
[[nodiscard]] TPL_API std::string export_markdown(const SessionInfo& session,
                                                  std::span<const StoredSegment> segments);

/// SubRip subtitles: numbered cues with "HH:MM:SS,mmm --> HH:MM:SS,mmm" times since the
/// session started (hours keep counting past 24).
[[nodiscard]] TPL_API std::string export_srt(std::span<const StoredSegment> segments);

/// "HH:MM:SS,mmm" for a time in milliseconds (SRT style); negative times clamp to zero.
[[nodiscard]] TPL_API std::string format_srt_time(std::int64_t ms);

/// "YYYY-MM-DD HH:MM" in the local time zone, e.g. the default session title.
[[nodiscard]] TPL_API std::string format_local_date_time(std::int64_t utc_ms);

}  // namespace tpl::transcript
