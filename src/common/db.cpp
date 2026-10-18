#include "db.hpp"
#include <cstring>
#include <stdexcept>

namespace bms {

std::string DbError::format_sqlite_error(int rc, const std::string& context) {
    std::string msg = sqlite3_errstr(rc);
    if (!context.empty()) {
        msg = context + ": " + msg;
    }
    return msg;
}

Database::Database(const std::string& path) {
    open(path);
}

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

void Database::open(const std::string& path) {
    if (is_open()) {
        throw DbError("Database already open");
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw DbError(rc, "Failed to open database");
    }

    // Enable foreign keys
    exec("PRAGMA foreign_keys = ON");

    // Set busy timeout to 5 seconds
    sqlite3_busy_timeout(db_, 5000);
}

void Database::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void Database::exec(const std::string& sql) {
    if (!db_) {
        throw DbError("Database not open");
    }

    char* errmsg = nullptr;
    int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "Unknown error";
        sqlite3_free(errmsg);
        throw DbError(rc, "SQL execution failed: " + msg);
    }
}

void Database::exec_script(const std::string& sql) {
    exec(sql); // For SQLite, exec can handle multiple statements
}

std::unique_ptr<Statement> Database::prepare(const std::string& sql) {
    if (!db_) {
        throw DbError("Database not open");
    }

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw DbError(rc, "Failed to prepare statement");
    }

    return std::make_unique<Statement>(stmt, *this);
}

void Database::begin(bool immediate) {
    if (!db_) {
        throw DbError("Database not open");
    }

    std::string sql = immediate ? "BEGIN IMMEDIATE" : "BEGIN";
    exec(sql);
}

void Database::commit() {
    if (!db_) {
        throw DbError("Database not open");
    }

    exec("COMMIT");
}

void Database::rollback() {
    if (!db_) {
        throw DbError("Database not open");
    }

    exec("ROLLBACK");
}

Statement::Statement(sqlite3_stmt* stmt, Database& db)
    : sqlite3_stmt_(stmt), db_(&db) {}

Statement::~Statement() {
    if (sqlite3_stmt_) {
        sqlite3_finalize(sqlite3_stmt_);
    }
}

Statement::Statement(Statement&& other) noexcept
    : sqlite3_stmt_(other.sqlite3_stmt_), db_(other.db_) {
    other.sqlite3_stmt_ = nullptr;
    other.db_ = nullptr;
}

Statement& Statement::operator=(Statement&& other) noexcept {
    if (this != &other) {
        if (sqlite3_stmt_) {
            sqlite3_finalize(sqlite3_stmt_);
        }
        sqlite3_stmt_ = other.sqlite3_stmt_;
        db_ = other.db_;
        other.sqlite3_stmt_ = nullptr;
        other.db_ = nullptr;
    }
    return *this;
}

void Statement::bind(int index, std::int64_t value) {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    int rc = sqlite3_bind_int64(sqlite3_stmt_, index, value);
    if (rc != SQLITE_OK) {
        throw DbError(rc, "Failed to bind int64 value");
    }
}

void Statement::bind(int index, const std::string& value) {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    int rc = sqlite3_bind_text(sqlite3_stmt_, index, value.c_str(), -1, SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
        throw DbError(rc, "Failed to bind text value");
    }
}

void Statement::bind(int index, std::nullptr_t) {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    int rc = sqlite3_bind_null(sqlite3_stmt_, index);
    if (rc != SQLITE_OK) {
        throw DbError(rc, "Failed to bind NULL value");
    }
}

bool Statement::step() {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    int rc = sqlite3_step(sqlite3_stmt_);
    if (rc == SQLITE_ROW) {
        return true;
    } else if (rc == SQLITE_DONE) {
        return false;
    } else {
        throw DbError(rc, "Failed to step statement");
    }
}

void Statement::reset() {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    int rc = sqlite3_reset(sqlite3_stmt_);
    if (rc != SQLITE_OK) {
        throw DbError(rc, "Failed to reset statement");
    }
}

std::int64_t Statement::column_int(int index) const {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    return sqlite3_column_int64(sqlite3_stmt_, index);
}

const unsigned char* Statement::column_text(int index) const {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    return sqlite3_column_text(sqlite3_stmt_, index);
}

bool Statement::column_null(int index) const {
    if (!sqlite3_stmt_) {
        throw DbError("Statement not prepared");
    }

    return sqlite3_column_type(sqlite3_stmt_, index) == SQLITE_NULL;
}

} // namespace bms