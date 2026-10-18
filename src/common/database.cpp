#include "common/database.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <sqlite3.h>

#include "common/log.hpp"

namespace bms {

// ---------------------------------------------------------------------------
// Statement
// ---------------------------------------------------------------------------

Statement::Statement(sqlite3* db, const std::string& sql) : db_(db) {
    if (sqlite3_prepare_v2(db, sql.c_str(), static_cast<int>(sql.size()), &stmt_, nullptr) !=
        SQLITE_OK) {
        throw std::runtime_error(std::string("SQL prepare error: ") + sqlite3_errmsg(db));
    }
}

Statement::~Statement() {
    if (stmt_) sqlite3_finalize(stmt_);
}

Statement& Statement::bind(int index, const std::string& value) {
    sqlite3_bind_text(stmt_, index, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    return *this;
}

Statement& Statement::bind(int index, int value) {
    sqlite3_bind_int(stmt_, index, value);
    return *this;
}

Statement& Statement::bind(int index, double value) {
    sqlite3_bind_double(stmt_, index, value);
    return *this;
}

Statement& Statement::bind_null(int index) {
    sqlite3_bind_null(stmt_, index);
    return *this;
}

bool Statement::step() {
    int rc = sqlite3_step(stmt_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(std::string("SQL step error: ") + sqlite3_errmsg(db_));
}

void Statement::execute() {
    int rc = sqlite3_step(stmt_);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(std::string("SQL execute error: ") + sqlite3_errmsg(db_));
    }
}

std::string Statement::column_text(int index) const {
    const unsigned char* text = sqlite3_column_text(stmt_, index);
    return text ? reinterpret_cast<const char*>(text) : "";
}

int Statement::column_int(int index) const {
    return sqlite3_column_int(stmt_, index);
}

double Statement::column_double(int index) const {
    return sqlite3_column_double(stmt_, index);
}

bool Statement::column_is_null(int index) const {
    return sqlite3_column_type(stmt_, index) == SQLITE_NULL;
}

// ---------------------------------------------------------------------------
// Database
// ---------------------------------------------------------------------------

Database::Database(const std::string& path) {
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        std::string err = sqlite3_errmsg(db_);
        sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("Cannot open database '" + path + "': " + err);
    }
    // WAL mode for better concurrency and foreign key enforcement.
    exec("PRAGMA journal_mode=WAL");
    exec("PRAGMA foreign_keys=ON");
}

Database::~Database() {
    if (db_) sqlite3_close(db_);
}

Statement Database::prepare(const std::string& sql) {
    return Statement(db_, sql);
}

void Database::exec(const std::string& sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown error";
        sqlite3_free(err);
        throw std::runtime_error("SQL exec error: " + msg);
    }
}

void Database::transaction(const std::function<void()>& fn) {
    exec("BEGIN IMMEDIATE");
    try {
        fn();
        exec("COMMIT");
    } catch (...) {
        exec("ROLLBACK");
        throw;
    }
}

namespace {
std::unique_ptr<Database> g_instance;
}

Database& Database::instance() {
    if (!g_instance) {
        throw std::runtime_error("Database not initialised. Call Database::init() first.");
    }
    return *g_instance;
}

void Database::init(const std::string& path) {
    g_instance = std::make_unique<Database>(path);
    log::info("Database opened: " + path);

    // Apply schema from db/schema.sql if it exists as a resource.
    // For now, apply the schema inline.
    g_instance->exec(R"SQL(
        CREATE TABLE IF NOT EXISTS users (
            id              TEXT PRIMARY KEY,
            name            TEXT NOT NULL,
            role            TEXT NOT NULL CHECK (role IN ('administrator', 'bank_officer', 'customer')),
            password_hash   TEXT NOT NULL,
            failed_attempts INTEGER NOT NULL DEFAULT 0,
            locked          INTEGER NOT NULL DEFAULT 0,
            active          INTEGER NOT NULL DEFAULT 1,
            created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
            updated_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
        );
        CREATE TABLE IF NOT EXISTS sessions (
            token       TEXT PRIMARY KEY,
            user_id     TEXT NOT NULL REFERENCES users(id),
            role        TEXT NOT NULL,
            created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
            expires_at  TEXT NOT NULL
        );
        CREATE TABLE IF NOT EXISTS accounts (
            id              TEXT PRIMARY KEY,
            customer_id     TEXT NOT NULL REFERENCES users(id),
            account_type    TEXT NOT NULL CHECK (account_type IN ('savings', 'current')),
            balance         REAL NOT NULL DEFAULT 0.0,
            min_balance     REAL NOT NULL DEFAULT 1000.0,
            active          INTEGER NOT NULL DEFAULT 1,
            created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
            updated_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
        );
        CREATE TABLE IF NOT EXISTS transactions (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            account_id      TEXT NOT NULL REFERENCES accounts(id),
            type            TEXT NOT NULL CHECK (type IN ('deposit', 'withdrawal', 'transfer_in', 'transfer_out')),
            amount          REAL NOT NULL,
            balance_after   REAL NOT NULL,
            description     TEXT,
            performed_by    TEXT NOT NULL REFERENCES users(id),
            created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
        );
        CREATE TABLE IF NOT EXISTS audit_log (
            id          INTEGER PRIMARY KEY AUTOINCREMENT,
            user_id     TEXT REFERENCES users(id),
            action      TEXT NOT NULL,
            target_type TEXT,
            target_id   TEXT,
            details     TEXT,
            created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
        );
        CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
        CREATE INDEX IF NOT EXISTS idx_sessions_user  ON sessions(user_id);
        CREATE INDEX IF NOT EXISTS idx_accounts_customer ON accounts(customer_id);
        CREATE INDEX IF NOT EXISTS idx_transactions_account ON transactions(account_id);
        CREATE INDEX IF NOT EXISTS idx_audit_user ON audit_log(user_id);
    )SQL");
    log::info("Schema applied");
}

}  // namespace bms
