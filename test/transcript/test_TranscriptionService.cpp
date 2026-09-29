#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "TestDir.h"
#include "tpl/stt/Segment.h"
#include "tpl/transcript/ReplaySegmentSource.h"
#include "tpl/transcript/TranscriptStore.h"
#include "tpl/transcript/TranscriptionService.h"
#include "tpl/transcript/Types.h"

namespace {

using tpl::transcript::test::must;

using tpl::transcript::ReplaySegmentSource;
using tpl::transcript::ServiceConfig;
using tpl::transcript::SessionId;
using tpl::transcript::TranscriptionService;
using tpl::transcript::TranscriptStore;

constexpr std::int64_t k_start_utc = 1'790'000'000'000;

struct RecordingListener final : TranscriptionService::Listener {
    void segment_stored(SessionId session, std::int64_t seq) override {
        const std::scoped_lock lock(m_mutex);
        m_stored.emplace_back(session, seq);
    }
    void sessions_changed() override { ++m_sessions_changed; }
    void service_error(const std::string& message) override {
        const std::scoped_lock lock(m_mutex);
        m_errors.push_back(message);
    }

    std::mutex m_mutex;
    std::vector<std::pair<SessionId, std::int64_t>> m_stored;
    std::vector<std::string> m_errors;
    std::atomic<int> m_sessions_changed{0};
};

ServiceConfig test_config(std::chrono::milliseconds max_session = std::chrono::hours(24)) {
    return {.m_max_session_length = max_session,
            .m_poll_interval = std::chrono::milliseconds(1),
            .m_heartbeat_interval = std::chrono::milliseconds(0),
            .m_now_utc_ms = [] { return k_start_utc; }};
}

void push_seconds(tpl::stt::SegmentSource& source, double seconds) {
    const std::vector<float> block(static_cast<std::size_t>(seconds * 16000.0), 0.0f);
    source.push_audio(block);
}

class TranscriptionServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_store = std::make_unique<TranscriptStore>(tpl::transcript::test::fresh_dir());
    }

    std::unique_ptr<TranscriptStore> m_store;
};

TEST_F(TranscriptionServiceTest, RecordsSegmentsIntoANewSession) {
    ReplaySegmentSource source(ReplaySegmentSource::synthetic_script(3, 5, 4.0));
    source.prepare(16000.0, 512);
    RecordingListener listener;
    TranscriptionService service(source, *m_store, test_config());
    service.add_listener(&listener);

    EXPECT_FALSE(service.is_recording());
    service.start("de");
    service.flush();
    EXPECT_TRUE(service.is_recording());
    const auto session = service.active_session();
    ASSERT_TRUE(session.has_value());

    push_seconds(source, 11.0);  // three segments; the script loops at 11.8 s
    service.flush();
    EXPECT_EQ(m_store->segment_count(must(session)), 3);
    const auto segments = m_store->load_segments(must(session), 0, 3);
    EXPECT_EQ(segments[1].m_start_ms, 4'000);
    EXPECT_EQ(segments[1].m_words.size(), 5u);

    service.stop();
    service.flush();
    EXPECT_FALSE(service.is_recording());
    EXPECT_FALSE(service.active_session().has_value());
    const auto info = m_store->session(must(session));
    EXPECT_EQ(must(info).m_language, "de");
    EXPECT_EQ(must(info).m_started_utc_ms, k_start_utc);
    EXPECT_EQ(must(info).m_ended_utc_ms, k_start_utc + segments[2].m_end_ms);
    EXPECT_EQ(must(info).m_title.size(), 16u);  // "YYYY-MM-DD HH:MM"

    service.remove_listener(&listener);
    EXPECT_EQ(listener.m_stored.size(), 3u);
    EXPECT_EQ(listener.m_sessions_changed.load(), 2);
    EXPECT_TRUE(listener.m_errors.empty());
}

TEST_F(TranscriptionServiceTest, SegmentsWithoutASessionAreDropped) {
    ReplaySegmentSource source(ReplaySegmentSource::synthetic_script(2, 3, 2.0));
    source.prepare(16000.0, 512);
    TranscriptionService service(source, *m_store, test_config());
    push_seconds(source, 10.0);
    service.flush();
    EXPECT_TRUE(m_store->list_sessions().empty());
}

TEST_F(TranscriptionServiceTest, EachStartIsANewSessionFromPositionZero) {
    ReplaySegmentSource source(ReplaySegmentSource::synthetic_script(1, 3, 2.0));
    source.prepare(16000.0, 512);
    TranscriptionService service(source, *m_store, test_config());
    service.start("en");
    service.flush();
    const auto first = service.active_session();
    push_seconds(source, 2.0);
    service.start("en");  // restart without stop: finishes the first
    service.flush();
    const auto second = service.active_session();
    ASSERT_TRUE(first && second);
    EXPECT_NE(must(first), must(second));
    EXPECT_EQ(m_store->segment_count(must(first)), 1);
    EXPECT_TRUE(must(m_store->session(must(first))).m_ended_utc_ms.has_value());
    push_seconds(source, 2.0);
    service.flush();
    EXPECT_EQ(m_store->load_segments(must(second), 0, 1).at(0).m_start_ms, 0);
}

TEST_F(TranscriptionServiceTest, RollsOverLongRecordingsAtSegmentBoundaries) {
    // 10 s segments, 25 s max: segments at 0, 10, 20 | 30 (new session, starts at 0 ms), 40 ...
    ReplaySegmentSource source(ReplaySegmentSource::synthetic_script(6, 4, 10.0));
    source.prepare(16000.0, 512);
    TranscriptionService service(source, *m_store, test_config(std::chrono::seconds(25)));
    service.start("en");
    service.flush();
    push_seconds(source, 57.5);  // all six segments; the script loops at 58 s
    service.flush();
    service.stop();
    service.flush();

    const auto sessions = m_store->list_sessions();
    ASSERT_EQ(sessions.size(), 2u);
    const auto& first = sessions[1];
    const auto& second = sessions[0];
    EXPECT_EQ(first.m_segment_count, 3);
    EXPECT_EQ(second.m_segment_count, 3);
    EXPECT_EQ(second.m_started_utc_ms, k_start_utc + 30'000);
    EXPECT_EQ(m_store->load_segments(second.m_id, 0, 1).at(0).m_start_ms, 0);
    EXPECT_EQ(first.m_ended_utc_ms, k_start_utc + 27'000);  // 20 s + 7 s of speech
}

// A simulated day of speech through the whole path: bounded time, every segment stored.
TEST_F(TranscriptionServiceTest, SoakTwentyFourHours) {
    constexpr std::size_t k_segments = 24 * 3600 / 8;  // one every 8 s
    ReplaySegmentSource source(ReplaySegmentSource::synthetic_script(k_segments, 20, 8.0));
    source.prepare(48000.0, 4096);
    TranscriptionService service(source, *m_store, test_config());
    service.start("en");
    service.flush();
    const std::vector<float> minute(std::size_t{48000} * 60, 0.0f);
    for (int h = 0; h < 24; ++h) {
        for (int m = 0; m < 59; ++m) { source.push_audio(minute); }
        // The script loops at 86398.6 s: the day's last minute stops 2 s short of it.
        source.push_audio(std::span(minute).first(h < 23 ? minute.size()
                                                         : minute.size() - std::size_t{2} * 48000));
        service.flush();
    }
    service.stop();
    service.flush();
    const auto sessions = m_store->list_sessions();
    ASSERT_EQ(sessions.size(), 1u);
    EXPECT_EQ(sessions[0].m_segment_count, static_cast<std::int64_t>(k_segments));
    EXPECT_FALSE(m_store->search("quietly", 5).empty());
}

class ThrowingSource final : public tpl::stt::SegmentSource {
public:
    void prepare(double /*sample_rate*/, int /*max_block_size*/) override {}
    void push_audio(std::span<const float> /*mono*/) noexcept override {}
    bool pop_segment(tpl::stt::Segment& /*out*/) override { return false; }
    void reset() override { throw std::runtime_error("backend failed"); }
};

TEST_F(TranscriptionServiceTest, WorkerErrorsStopRecordingAndReachListeners) {
    ThrowingSource source;
    RecordingListener listener;
    TranscriptionService service(source, *m_store, test_config());
    service.add_listener(&listener);
    service.start("en");
    service.flush();
    EXPECT_FALSE(service.is_recording());
    EXPECT_FALSE(service.active_session().has_value());
    service.remove_listener(&listener);
    ASSERT_EQ(listener.m_errors.size(), 1u);
    EXPECT_EQ(listener.m_errors[0], "backend failed");
}

TEST_F(TranscriptionServiceTest, DestructorFinishesTheSession) {
    ReplaySegmentSource source(ReplaySegmentSource::synthetic_script(1, 3, 2.0));
    source.prepare(16000.0, 512);
    std::optional<SessionId> session;
    {
        TranscriptionService service(source, *m_store, test_config());
        service.start("en");
        service.flush();
        session = service.active_session();
    }
    ASSERT_TRUE(session.has_value());
    EXPECT_TRUE(must(m_store->session(must(session))).m_ended_utc_ms.has_value());
}

TEST_F(TranscriptionServiceTest, HeartbeatKeepsTheSessionFromRecovery) {
    ReplaySegmentSource source({});
    TranscriptionService service(source, *m_store, test_config());
    service.start("en");
    service.flush();
    service.flush();  // one more loop: heartbeat written with the fixed clock
    EXPECT_EQ(m_store->recover_abandoned_sessions(k_start_utc), 0);
    EXPECT_EQ(m_store->recover_abandoned_sessions(k_start_utc + 1), 1);
}

}  // namespace
