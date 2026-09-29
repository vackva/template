#include "tpl/transcript/TranscriptionService.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#include "tpl/stt/Segment.h"
#include "tpl/transcript/Export.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

namespace {

std::int64_t system_now_utc_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::int64_t samples_to_ms(std::int64_t samples) {
    return samples * 1000 / stt::k_segment_sample_rate;
}

}  // namespace

TranscriptionService::Listener::~Listener() = default;

TranscriptionService::TranscriptionService(stt::SegmentSource& source,
                                           TranscriptStore& store,
                                           ServiceConfig config)
    : m_source(source), m_store(store), m_config(std::move(config)) {
    if (!m_config.m_now_utc_ms) { m_config.m_now_utc_ms = system_now_utc_ms; }
    m_worker = std::thread([this] { run(); });
}

TranscriptionService::~TranscriptionService() {
    try {
        stop();
        {
            const std::scoped_lock lock(m_mutex);
            m_quit = true;
        }
        m_wake.notify_all();
        m_worker.join();
    } catch (...) {  // NOLINT(bugprone-empty-catch): a destructor must not throw
        // Locking or joining failed (std::system_error); nothing sensible is left to do.
    }
}

void TranscriptionService::start(const std::string& language) {
    {
        const std::scoped_lock lock(m_mutex);
        m_pending.push_back({.m_command = Command::Start, .m_language = language});
    }
    m_wake.notify_all();
}

void TranscriptionService::stop() {
    // The audio thread stops pushing right away; the worker then flushes the source.
    m_recording.store(false, std::memory_order_release);
    {
        const std::scoped_lock lock(m_mutex);
        m_pending.push_back({.m_command = Command::Stop, .m_language = {}});
    }
    m_wake.notify_all();
}

std::optional<SessionId> TranscriptionService::active_session() const {
    const std::scoped_lock lock(m_mutex);
    return m_session;
}

void TranscriptionService::add_listener(Listener* listener) {
    const std::scoped_lock lock(m_listener_mutex);
    m_listeners.push_back(listener);
}

void TranscriptionService::remove_listener(Listener* listener) {
    const std::scoped_lock lock(m_listener_mutex);
    std::erase(m_listeners, listener);
}

void TranscriptionService::flush() {
    std::unique_lock lock(m_mutex);
    const auto ticket = ++m_flush_requested;
    m_pending.push_back({.m_command = Command::Flush, .m_language = {}});
    m_wake.notify_all();
    m_flushed.wait(lock, [&] { return m_flush_done >= ticket; });
}

template <typename F>
void TranscriptionService::notify(F&& call) {
    const std::scoped_lock lock(m_listener_mutex);
    for (auto* listener : m_listeners) { call(*listener); }
}

std::int64_t TranscriptionService::now() const {
    return m_config.m_now_utc_ms();
}

void TranscriptionService::run() {
    for (;;) {
        std::optional<Pending> pending;
        {
            std::unique_lock lock(m_mutex);
            m_wake.wait_for(lock, m_config.m_poll_interval, [this] {
                return m_quit || !m_pending.empty();
            });
            if (m_pending.empty() && m_quit) { return; }
            if (!m_pending.empty()) {
                pending = std::move(m_pending.front());
                m_pending.pop_front();
            }
        }
        try {
            if (pending) {
                handle(*pending);
            } else {
                drain_source();
            }
            if (m_session && std::chrono::steady_clock::now() - m_last_heartbeat >=
                                 m_config.m_heartbeat_interval) {
                m_store.heartbeat(*m_session, now());
                m_last_heartbeat = std::chrono::steady_clock::now();
            }
        } catch (const std::exception& e) {
            m_recording.store(false, std::memory_order_release);
            {
                const std::scoped_lock lock(m_mutex);
                m_session.reset();
            }
            notify([&](Listener& l) { l.service_error(e.what()); });
        }
        if (pending && pending->m_command == Command::Flush) {
            {
                const std::scoped_lock lock(m_mutex);
                ++m_flush_done;
            }
            m_flushed.notify_all();
        }
    }
}

void TranscriptionService::handle(const Pending& pending) {
    switch (pending.m_command) {
        case Command::Start:
            finish_recording();
            m_source.reset();
            begin_session(pending.m_language, now(), 0);
            m_recording.store(true, std::memory_order_release);
            break;
        case Command::Stop:
            m_recording.store(false, std::memory_order_release);
            finish_recording();
            break;
        case Command::Flush: drain_source(); break;
    }
}

void TranscriptionService::finish_recording() {
    if (!m_session) { return; }
    m_source.reset();  // ends the current segment; it is ready to pop afterwards
    drain_source();
    end_session();
}

void TranscriptionService::begin_session(const std::string& language,
                                         std::int64_t started_utc_ms,
                                         std::int64_t origin_sample) {
    const auto id =
        m_store.create_session(format_local_date_time(started_utc_ms), started_utc_ms, language);
    {
        const std::scoped_lock lock(m_mutex);
        m_session = id;
    }
    m_language = language;
    m_session_started_utc_ms = started_utc_ms;
    m_origin_sample = origin_sample;
    m_last_end_ms = 0;
    m_last_heartbeat = std::chrono::steady_clock::now();
    notify([](Listener& l) { l.sessions_changed(); });
}

void TranscriptionService::end_session() {
    const auto session = m_session;
    if (!session) { return; }
    m_store.finish_session(*session, m_session_started_utc_ms + m_last_end_ms);
    {
        const std::scoped_lock lock(m_mutex);
        m_session.reset();
    }
    notify([](Listener& l) { l.sessions_changed(); });
}

void TranscriptionService::drain_source() {
    stt::Segment segment;
    while (m_source.pop_segment(segment)) {
        if (m_session) { store(segment); }
    }
}

void TranscriptionService::store(const stt::Segment& segment) {
    // Roll over at a segment boundary once the session is too long; the new session
    // starts where this segment starts.
    const auto start_ms = samples_to_ms(segment.m_start_sample - m_origin_sample);
    if (segment.m_is_final && start_ms >= m_config.m_max_session_length.count() &&
        m_last_end_ms > 0) {
        const auto language = m_language;
        const auto next_start_utc = m_session_started_utc_ms + start_ms;
        const auto next_origin = segment.m_start_sample;
        end_session();
        begin_session(language, next_start_utc, next_origin);
    }

    const auto current = m_session;
    if (!current) { return; }
    const SessionId session = *current;

    StoredSegment stored{.m_session_id = session,
                         .m_seq = 0,
                         .m_source_id = segment.m_id,
                         .m_start_ms = samples_to_ms(segment.m_start_sample - m_origin_sample),
                         .m_end_ms = samples_to_ms(segment.m_end_sample - m_origin_sample),
                         .m_text = segment.m_text,
                         .m_words = {},
                         .m_is_final = segment.m_is_final};
    stored.m_words.reserve(segment.m_words.size());
    for (const auto& word : segment.m_words) {
        stored.m_words.push_back(
            {.m_text = word.m_text,
             .m_start_ms = samples_to_ms(word.m_start_sample - m_origin_sample),
             .m_end_ms = samples_to_ms(word.m_end_sample - m_origin_sample)});
    }
    const auto seq = m_store.upsert_segment(stored);
    m_last_end_ms = std::max(m_last_end_ms, stored.m_end_ms);
    notify([&](Listener& l) { l.segment_stored(session, seq); });
}

}  // namespace tpl::transcript
