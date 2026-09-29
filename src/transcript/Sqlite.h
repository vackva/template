#pragma once

#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tpl::transcript::sqlite {

/// Owns one sqlite3 connection. Throws std::runtime_error with SQLite's message on failure.
class Database {
public:
    explicit Database(const std::filesystem::path& file);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;

    void exec(const char* sql);
    [[nodiscard]] sqlite3* handle() const noexcept { return m_db; }
    [[nodiscard]] std::int64_t last_insert_rowid() const noexcept;
    [[nodiscard]] int changes() const noexcept;

    [[noreturn]] void fail(std::string_view what) const;

private:
    sqlite3* m_db = nullptr;
};

/// A prepared statement; bind indices are 1-based like SQLite's.
class Statement {
public:
    Statement(Database& db, const char* sql);
    ~Statement();
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement(Statement&&) = delete;
    Statement& operator=(Statement&&) = delete;

    Statement& bind(int index, std::int64_t value);
    Statement& bind(int index, std::string_view value);
    Statement& bind_blob(int index, std::span<const std::uint8_t> value);
    Statement& bind_null(int index);

    /// Advances to the next row; false when done.
    bool step();
    /// Runs a statement that returns no rows.
    void run();

    [[nodiscard]] std::int64_t column_int(int index) const;
    [[nodiscard]] std::optional<std::int64_t> column_optional_int(int index) const;
    [[nodiscard]] std::string column_text(int index) const;
    [[nodiscard]] std::vector<std::uint8_t> column_blob(int index) const;

private:
    Database& m_db;
    sqlite3_stmt* m_statement = nullptr;
};

/// BEGIN IMMEDIATE ... COMMIT, rolled back if destroyed without commit().
class Transaction {
public:
    explicit Transaction(Database& db);
    ~Transaction();
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    Transaction(Transaction&&) = delete;
    Transaction& operator=(Transaction&&) = delete;

    void commit();

private:
    Database& m_db;
    bool m_done = false;
};

}  // namespace tpl::transcript::sqlite
