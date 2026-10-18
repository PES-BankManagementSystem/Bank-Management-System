#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

struct sqlite3;
struct sqlite3_stmt;

namespace bms {

// RAII wrapper around an SQLite prepared statement.
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql);
    ~Statement();

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    Statement& bind(int index, const std::string& value);
    Statement& bind(int index, int value);
    Statement& bind(int index, double value);
    Statement& bind_null(int index);

    // Returns true while there are rows to read.
    bool step();

    // Execute without reading rows (INSERT, UPDATE, DELETE).
    void execute();

    // Reset statement for re-execution.
    void reset();

    std::string column_text(int index) const;
    int column_int(int index) const;
    double column_double(int index) const;
    bool column_is_null(int index) const;

private:
    sqlite3_stmt* stmt_ = nullptr;
    sqlite3* db_ = nullptr;
};

// Thin RAII wrapper around an SQLite connection. The connection is opened in
// WAL mode and enforces foreign keys. Thread-safety: one Database per thread;
// the underlying SQLite library serialises writes via its own mutex.
class Database {
public:
    explicit Database(const std::string& path);
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    Statement prepare(const std::string& sql);

    // Run an SQL string (may contain multiple statements separated by ';').
    void exec(const std::string& sql);

    // Run a function inside BEGIN IMMEDIATE ... COMMIT, rolling back on
    // exception.
    void transaction(const std::function<void()>& fn);

    sqlite3* raw() { return db_; }

    // The single shared connection for the server process.
    static Database& instance();
    static void init(const std::string& path);

private:
    sqlite3* db_ = nullptr;
};

}  // namespace bms
