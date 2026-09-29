#include "Sqlite.h"

#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace tpl::transcript::sqlite {

namespace {

constexpr int k_busy_timeout_ms = 5000;

}  // namespace

Database::Database(const std::filesystem::path& file) {
    const auto name = file.u8string();
    const int result =
        sqlite3_open_v2(reinterpret_cast<const char*>(name.c_str()),
                        &m_db,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr);
    if (result != SQLITE_OK) {
        const std::string message = m_db != nullptr ? sqlite3_errmsg(m_db) : sqlite3_errstr(result);
        sqlite3_close(m_db);
        throw std::runtime_error("tpl::transcript: cannot open " + file.string() + ": " + message);
    }
    sqlite3_busy_timeout(m_db, k_busy_timeout_ms);
}

Database::~Database() {
    sqlite3_close_v2(m_db);
}

void Database::exec(const char* sql) {
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, nullptr) != SQLITE_OK) { fail(sql); }
}

std::int64_t Database::last_insert_rowid() const noexcept {
    return sqlite3_last_insert_rowid(m_db);
}

int Database::changes() const noexcept {
    return sqlite3_changes(m_db);
}

void Database::fail(std::string_view what) const {
    throw std::runtime_error("tpl::transcript: SQLite error in \"" + std::string(what) +
                             "\": " + sqlite3_errmsg(m_db));
}

Statement::Statement(Database& db, const char* sql) : m_db(db) {
    if (sqlite3_prepare_v2(db.handle(), sql, -1, &m_statement, nullptr) != SQLITE_OK) {
        db.fail(sql);
    }
}

Statement::~Statement() {
    sqlite3_finalize(m_statement);
}

Statement& Statement::bind(int index, std::int64_t value) {
    if (sqlite3_bind_int64(m_statement, index, value) != SQLITE_OK) { m_db.fail("bind"); }
    return *this;
}

Statement& Statement::bind(int index, std::string_view value) {
    if (sqlite3_bind_text64(m_statement,
                            index,
                            value.data(),
                            static_cast<sqlite3_uint64>(value.size()),
                            SQLITE_TRANSIENT,
                            SQLITE_UTF8) != SQLITE_OK) {
        m_db.fail("bind");
    }
    return *this;
}

Statement& Statement::bind_blob(int index, std::span<const std::uint8_t> value) {
    if (sqlite3_bind_blob64(m_statement,
                            index,
                            value.data(),
                            static_cast<sqlite3_uint64>(value.size()),
                            SQLITE_TRANSIENT) != SQLITE_OK) {
        m_db.fail("bind");
    }
    return *this;
}

Statement& Statement::bind_null(int index) {
    if (sqlite3_bind_null(m_statement, index) != SQLITE_OK) { m_db.fail("bind"); }
    return *this;
}

bool Statement::step() {
    const int result = sqlite3_step(m_statement);
    if (result == SQLITE_ROW) { return true; }
    if (result == SQLITE_DONE) { return false; }
    m_db.fail(sqlite3_sql(m_statement));
}

void Statement::run() {
    while (step()) {}
}

std::int64_t Statement::column_int(int index) const {
    return sqlite3_column_int64(m_statement, index);
}

std::optional<std::int64_t> Statement::column_optional_int(int index) const {
    if (sqlite3_column_type(m_statement, index) == SQLITE_NULL) { return std::nullopt; }
    return sqlite3_column_int64(m_statement, index);
}

std::string Statement::column_text(int index) const {
    const auto* text = sqlite3_column_text(m_statement, index);
    const auto size = static_cast<std::size_t>(sqlite3_column_bytes(m_statement, index));
    return text == nullptr ? std::string{} : std::string(reinterpret_cast<const char*>(text), size);
}

std::vector<std::uint8_t> Statement::column_blob(int index) const {
    const auto* data = static_cast<const std::uint8_t*>(sqlite3_column_blob(m_statement, index));
    const auto size = static_cast<std::size_t>(sqlite3_column_bytes(m_statement, index));
    return data == nullptr ? std::vector<std::uint8_t>{}
                           : std::vector<std::uint8_t>(data, data + size);
}

Transaction::Transaction(Database& db) : m_db(db) {
    m_db.exec("BEGIN IMMEDIATE");
}

Transaction::~Transaction() {
    if (!m_done) { sqlite3_exec(m_db.handle(), "ROLLBACK", nullptr, nullptr, nullptr); }
}

void Transaction::commit() {
    m_db.exec("COMMIT");
    m_done = true;
}

}  // namespace tpl::transcript::sqlite
