#pragma once

#include <filesystem>
#include <string>

#include "tpl/Exports.h"

namespace tpl::transcript {

/// Per-user settings of the transcription app, edited in the gear panel and stored as
/// settings.json in default_data_dir() (never in the moved storage directory, so they
/// are found again after a move).
struct Settings {
    std::filesystem::path m_storage_dir;  ///< where transcripts.db lives
    int m_retention_days = 0;             ///< 0 keeps sessions until deleted
    int m_max_session_hours = 24;         ///< a longer recording continues in a new session
    std::string m_language = "en";        ///< "en" or "de"
    std::filesystem::path m_model_dir;    ///< empty: tpl::stt::default_model_dir()

    friend bool operator==(const Settings&, const Settings&) = default;
};

/// Per-user data directory: $TPL_DATA_DIR if set (portable installs, tests), else
/// ~/Library/Application Support/tpl (macOS), %APPDATA%\tpl (Windows),
/// $XDG_DATA_HOME/tpl or ~/.local/share/tpl (Linux).
[[nodiscard]] TPL_API std::filesystem::path default_data_dir();

/// Defaults: storage in default_data_dir(), keep forever, 24 h sessions, English.
[[nodiscard]] TPL_API Settings default_settings();

/// Reads `file`; missing file or fields fall back to default_settings(). Throws
/// std::runtime_error only if the file exists but is not valid JSON.
[[nodiscard]] TPL_API Settings load_settings(const std::filesystem::path& file);

/// Writes `file` (creating its directory) atomically: temp file, then rename.
TPL_API void save_settings(const std::filesystem::path& file, const Settings& settings);

}  // namespace tpl::transcript
