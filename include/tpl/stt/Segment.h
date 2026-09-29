#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "tpl/Exports.h"

namespace tpl::stt {

/// Sample rate of all positions in Word and Segment.
inline constexpr std::int64_t k_segment_sample_rate = 16000;

/// One word of a segment; positions in 16 kHz samples since the source's last reset().
struct Word {
    std::string m_text;
    std::int64_t m_start_sample = 0;
    std::int64_t m_end_sample = 0;

    friend bool operator==(const Word&, const Word&) = default;
};

/// A stretch of speech between two pauses (at most 30 s in the accurate tier).
struct Segment {
    std::uint64_t m_id = 0;  ///< unique per source; a revision reuses the id
    std::int64_t m_start_sample = 0;
    std::int64_t m_end_sample = 0;
    std::string m_text;
    std::vector<Word> m_words;
    /// false: a provisional hypothesis that a later Segment with the same id replaces
    /// (responsive tier). The accurate tier delivers every id once, final.
    bool m_is_final = true;

    friend bool operator==(const Segment&, const Segment&) = default;
};

/// Hand-off between the streaming speech-to-text backend and its consumers (storage, UI).
/// The backend's StreamingTranscriber implements it; ReplaySegmentSource
/// (tpl/transcript) scripts it for development and tests.
class TPL_API SegmentSource {
public:
    SegmentSource() = default;
    SegmentSource(const SegmentSource&) = delete;
    SegmentSource& operator=(const SegmentSource&) = delete;
    SegmentSource(SegmentSource&&) = delete;
    SegmentSource& operator=(SegmentSource&&) = delete;
    virtual ~SegmentSource();

    /// Non-real-time. Before the first push_audio() and whenever the host's sample rate
    /// or maximum block size changes: sets up the host-rate -> 16 kHz resampler and
    /// allocates every buffer push_audio() needs.
    virtual void prepare(double sample_rate, int max_block_size) = 0;

    /// Audio thread, real-time safe (no allocation, lock or syscall): mono samples at the
    /// prepared rate. Resamples to 16 kHz here, on the audio thread, and hands only 16 kHz
    /// audio on, so the inference thread does nothing but inference. Never blocks; audio
    /// the source cannot keep up with is dropped, not queued without bound.
    virtual void push_audio(std::span<const float> mono) noexcept = 0;

    /// Any one non-audio thread: the next finished or revised segment; false if none.
    virtual bool pop_segment(Segment& out) = 0;

    /// Non-audio thread, may run while push_audio() does: ends the current segment,
    /// makes it available to pop_segment() before returning (may take as long as one
    /// inference) and restarts sample positions at 0. Called at every session boundary.
    virtual void reset() = 0;
};

}  // namespace tpl::stt
