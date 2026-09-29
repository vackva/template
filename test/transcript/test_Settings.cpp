#include <gtest/gtest.h>
#include <stdlib.h>  // NOLINT(modernize-deprecated-headers): POSIX setenv / unsetenv, MSVC _putenv_s

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "TestDir.h"
#include "tpl/transcript/Settings.h"

namespace {

using tpl::transcript::Settings;

TEST(Settings, DefaultsLiveInTheUserDataDir) {
    const auto defaults = tpl::transcript::default_settings();
    EXPECT_EQ(defaults.m_storage_dir, tpl::transcript::default_data_dir());
    EXPECT_EQ(defaults.m_retention_days, 0);
    EXPECT_EQ(defaults.m_max_session_hours, 24);
    EXPECT_EQ(defaults.m_language, "en");
    EXPECT_TRUE(defaults.m_model_dir.empty());
    EXPECT_EQ(tpl::transcript::default_data_dir().filename(), "tpl");
    EXPECT_TRUE(tpl::transcript::default_data_dir().is_absolute());
}

/// Sets (or, with an empty value, removes) an environment variable. Single-threaded tests only.
void set_env(const char* name, const std::string& value) {
#if defined(THL_PLATFORM_WINDOWS)
    _putenv_s(name, value.c_str());
#else
    if (value.empty()) {
        unsetenv(name);  // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    } else {
        setenv(name, value.c_str(), 1);  // NOLINT(concurrency-mt-unsafe,misc-include-cleaner)
    }
#endif
}

TEST(Settings, DataDirCanBeOverridden) {
    const auto dir = tpl::transcript::test::fresh_dir();
    set_env("TPL_DATA_DIR", dir.string());
    EXPECT_EQ(tpl::transcript::default_data_dir(), dir);
    set_env("TPL_DATA_DIR", "");
    EXPECT_NE(tpl::transcript::default_data_dir(), dir);
}

TEST(Settings, MissingFileGivesDefaults) {
    const auto file = tpl::transcript::test::fresh_dir() / "settings.json";
    EXPECT_EQ(tpl::transcript::load_settings(file), tpl::transcript::default_settings());
}

TEST(Settings, RoundTrip) {
    const auto file = tpl::transcript::test::fresh_dir() / "sub" / "settings.json";
    const Settings settings{.m_storage_dir = "/data/transcripts",
                      .m_retention_days = 30,
                      .m_max_session_hours = 8,
                      .m_language = "de",
                      .m_model_dir = "/models/parakeet"};
    tpl::transcript::save_settings(file, settings);
    EXPECT_EQ(tpl::transcript::load_settings(file), settings);
    EXPECT_FALSE(std::filesystem::exists(file.string() + ".tmp"));
}

TEST(Settings, InvalidValuesFallBackPerField) {
    const auto file = tpl::transcript::test::fresh_dir() / "settings.json";
    std::ofstream(file) << R"({"retention_days": -3, "max_session_hours": "8", "language": "fr",
                               "storage_dir": 42, "unknown": true})";
    EXPECT_EQ(tpl::transcript::load_settings(file), tpl::transcript::default_settings());
}

TEST(Settings, NonObjectJsonGivesDefaults) {
    const auto file = tpl::transcript::test::fresh_dir() / "settings.json";
    std::ofstream(file) << "[1, 2, 3]";
    EXPECT_EQ(tpl::transcript::load_settings(file), tpl::transcript::default_settings());
}

TEST(Settings, BrokenJsonThrows) {
    const auto file = tpl::transcript::test::fresh_dir() / "settings.json";
    std::ofstream(file) << "{ not json";
    EXPECT_THROW((void)tpl::transcript::load_settings(file), std::runtime_error);
}

}  // namespace
