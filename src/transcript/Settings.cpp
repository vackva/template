#include "tpl/transcript/Settings.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ios>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#if defined(THL_PLATFORM_WINDOWS)
#include <cstddef>
#include <memory>
#endif
#include <stdexcept>
#include <string>

namespace tpl::transcript {

namespace {

std::filesystem::path env_path(const char* name) {
#if defined(THL_PLATFORM_WINDOWS)
    // MSVC deprecates getenv (C4996, an error under /WX). The wide variant also keeps
    // non-ASCII paths intact; _wdupenv_s allocates the copy.
    const std::wstring wide_name(name, name + std::char_traits<char>::length(name));
    wchar_t* value = nullptr;
    std::size_t size = 0;
    if (_wdupenv_s(&value, &size, wide_name.c_str()) != 0 || value == nullptr) { return {}; }
    const std::unique_ptr<wchar_t, decltype(&std::free)> owned(value, &std::free);
    return *value != L'\0' ? std::filesystem::path(value) : std::filesystem::path{};
#else
    // NOLINTNEXTLINE(concurrency-mt-unsafe): read once at startup, nothing sets env vars
    const char* value = std::getenv(name);
    return value != nullptr && *value != '\0' ? std::filesystem::path(value)
                                              : std::filesystem::path{};
#endif
}

}  // namespace

std::filesystem::path default_data_dir() {
    if (auto overridden = env_path("TPL_DATA_DIR"); !overridden.empty()) { return overridden; }
#if defined(THL_PLATFORM_MACOS)
    return env_path("HOME") / "Library" / "Application Support" / "tpl";
#elif defined(THL_PLATFORM_WINDOWS)
    return env_path("APPDATA") / "tpl";
#else
    const auto xdg = env_path("XDG_DATA_HOME");
    return (xdg.empty() ? env_path("HOME") / ".local" / "share" : xdg) / "tpl";
#endif
}

Settings default_settings() {
    Settings settings;
    settings.m_storage_dir = default_data_dir();
    return settings;
}

Settings load_settings(const std::filesystem::path& file) {
    auto settings = default_settings();
    std::ifstream in(file);
    if (!in) { return settings; }

    nlohmann::json json;
    try {
        in >> json;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("tpl::transcript: " + file.string() +
                                 " is not valid JSON: " + e.what());
    }
    if (!json.is_object()) { return settings; }

    const auto path_or = [&](const char* key, const std::filesystem::path& fallback) {
        return json.contains(key) && json[key].is_string()
                   ? std::filesystem::path(json[key].get<std::string>())
                   : fallback;
    };
    const auto int_or = [&](const char* key, int fallback, int min, int max) {
        if (!json.contains(key) || !json[key].is_number_integer()) { return fallback; }
        const auto value = json[key].get<long long>();
        return value < min || value > max ? fallback : static_cast<int>(value);
    };

    settings.m_storage_dir = path_or("storage_dir", settings.m_storage_dir);
    settings.m_model_dir = path_or("model_dir", settings.m_model_dir);
    settings.m_retention_days = int_or("retention_days", settings.m_retention_days, 0, 36500);
    settings.m_max_session_hours =
        int_or("max_session_hours", settings.m_max_session_hours, 1, 24 * 7);
    if (json.contains("language") && json["language"].is_string()) {
        const auto language = json["language"].get<std::string>();
        if (language == "en" || language == "de") { settings.m_language = language; }
    }
    return settings;
}

void save_settings(const std::filesystem::path& file, const Settings& settings) {
    const nlohmann::json json{{"storage_dir", settings.m_storage_dir.string()},
                              {"model_dir", settings.m_model_dir.string()},
                              {"retention_days", settings.m_retention_days},
                              {"max_session_hours", settings.m_max_session_hours},
                              {"language", settings.m_language}};
    if (file.has_parent_path()) { std::filesystem::create_directories(file.parent_path()); }
    auto temp = file;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << json.dump(2) << '\n';
        if (!out) { throw std::runtime_error("tpl::transcript: cannot write " + temp.string()); }
    }
    std::filesystem::rename(temp, file);
}

}  // namespace tpl::transcript
