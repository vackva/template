#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "tpl/Exports.h"
#include "tpl/stt/Segment.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/Types.h"

namespace tpl::transcript {

struct ServiceConfig {
    /// A recording longer than this continues in a new session (at a segment boundary).
    std::chrono::milliseconds m_max_session_length = std::chrono::hours(24);
    /// How often the worker asks the source for segments.
    std::chrono::milliseconds m_poll_interval{20};
    /// How often a recording session proves it is alive (see recover_abandoned_sessions).
    std::chrono::milliseconds m_heartbeat_interval = std::chrono::minutes(1);
    /// Wall clock in Unix milliseconds; replaceable for tests.
    std::function<std::int64_t()> m_now_utc_ms;
};

/// Records what a SegmentSource hears into a TranscriptStore, one session per start/stop.
///
/// Threads: the audio thread only reads is_recording() and pushes to the source; a worker
/// thread owned by the service pulls segments, writes them and notifies listeners — on
/// that worker thread, so a UI marshals the calls to its own thread. start/stop return at
/// once; the worker carries them out in order.
class TPL_API TranscriptionService {
public:
    class TPL_API Listener {
    public:
        Listener() = default;
        Listener(const Listener&) = delete;
        Listener& operator=(const Listener&) = delete;
        Listener(Listener&&) = delete;
        Listener& operator=(Listener&&) = delete;
        virtual ~Listener();
        /// Segment `seq` of `session` was appended or revised.
        virtual void segment_stored(SessionId session, std::int64_t seq) = 0;
        /// A session was created, finished or rolled over.
        virtual void sessions_changed() = 0;
        /// Something failed on the worker (e.g. the disk is full); recording stopped.
        virtual void service_error(const std::string& message) = 0;
    };

    TranscriptionService(stt::SegmentSource& source,
                         TranscriptStore& store,
                         ServiceConfig config = {});
    /// Stops a running recording (finishing its session) and joins the worker.
    ~TranscriptionService();
    TranscriptionService(const TranscriptionService&) = delete;
    TranscriptionService& operator=(const TranscriptionService&) = delete;
    TranscriptionService(TranscriptionService&&) = delete;
    TranscriptionService& operator=(TranscriptionService&&) = delete;

    /// Starts a new session titled with the local date and time.
    void start(const std::string& language);
    /// Ends the current segment and finishes the session.
    void stop();

    /// Audio thread: whether to push audio to the source.
    [[nodiscard]] bool is_recording() const noexcept {
        return m_recording.load(std::memory_order_acquire);
    }
    /// The session being recorded, if any.
    [[nodiscard]] std::optional<SessionId> active_session() const;

    void add_listener(Listener* listener);
    void remove_listener(Listener* listener);

    /// Blocks until every command issued so far is done and the source is drained (tests).
    void flush();

private:
    enum class Command { Start, Stop, Flush };
    struct Pending {
        Command m_command;
        std::string m_language;
    };

    void run();
    void handle(const Pending& pending);
    void begin_session(const std::string& language,
                       std::int64_t started_utc_ms,
                       std::int64_t origin_sample);
    void end_session();
    void finish_recording();
    void drain_source();
    void store(const stt::Segment& segment);
    [[nodiscard]] std::int64_t now() const;
    template <typename F>
    void notify(F&& call);

    stt::SegmentSource& m_source;
    TranscriptStore& m_store;
    ServiceConfig m_config;

    std::atomic<bool> m_recording{false};

    mutable std::mutex m_mutex;  // commands, active session, flush bookkeeping
    std::condition_variable m_wake;
    std::condition_variable m_flushed;
    std::deque<Pending> m_pending;
    std::uint64_t m_flush_requested = 0;
    std::uint64_t m_flush_done = 0;
    bool m_quit = false;

    std::mutex m_listener_mutex;
    std::vector<Listener*> m_listeners;

    // Worker-thread state.
    std::optional<SessionId> m_session;
    std::string m_language;
    std::int64_t m_session_started_utc_ms = 0;
    std::int64_t m_origin_sample = 0;  // source position where the session starts
    std::int64_t m_last_end_ms = 0;
    std::chrono::steady_clock::time_point m_last_heartbeat;

    std::thread m_worker;  // last: starts after everything above is constructed
};

}  // namespace tpl::transcript
