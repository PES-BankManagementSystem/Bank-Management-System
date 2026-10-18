#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>

namespace bms
{

    // Failure reported by SQLite. Named DbError so it does not clash with
    // bms::Error in common/error.hpp.
    class DbError : public std::runtime_error
    {
    public:
        explicit DbError(const std::string &message) : std::runtime_error(message) {}
        explicit DbError(int sqlite_rc, const std::string &context = "")
            : std::runtime_error(format_sqlite_error(sqlite_rc, context)),
              sqlite_rc_(sqlite_rc) {}

        int sqlite_rc() const noexcept { return sqlite_rc_; }

    private:
        static std::string format_sqlite_error(int rc, const std::string &context);
        int sqlite_rc_ = 0;
    };

    class Statement;

    class Database
    {
    public:
        Database() = default;
        explicit Database(const std::string &path);
        ~Database();

        Database(const Database &) = delete;
        Database &operator=(const Database &) = delete;
        Database(Database &&) noexcept;
        Database &operator=(Database &&) noexcept;

        void open(const std::string &path);
        void close();

        bool is_open() const noexcept { return db_ != nullptr; }

        void exec(const std::string &sql);
        void exec_script(const std::string &sql);

        std::unique_ptr<Statement> prepare(const std::string &sql);

        void begin(bool immediate = false);
        void commit();
        void rollback();

        // Rolls back on scope exit unless commit() was called.
        class Transaction
        {
        public:
            explicit Transaction(Database &db, bool immediate = false)
                : db_(db), done_(false)
            {
                db_.begin(immediate);
            }

            ~Transaction()
            {
                if (!done_)
                {
                    try
                    {
                        db_.rollback();
                    }
                    catch (...)
                    {
                    }
                }
            }

            Transaction(const Transaction &) = delete;
            Transaction &operator=(const Transaction &) = delete;

            void commit()
            {
                db_.commit();
                done_ = true;
            }
            void rollback()
            {
                db_.rollback();
                done_ = true;
            }

        private:
            Database &db_;
            bool done_;
        };

    private:
        sqlite3 *db_ = nullptr;
    };

    class Statement
    {
    public:
        Statement() = default;
        Statement(sqlite3_stmt *stmt, Database &db);
        ~Statement();

        Statement(const Statement &) = delete;
        Statement &operator=(const Statement &) = delete;
        Statement(Statement &&) noexcept;
        Statement &operator=(Statement &&) noexcept;

        void bind(int index, std::int64_t value);
        void bind(int index, const std::string &value);
        void bind(int index, std::nullptr_t);

        bool step();
        void reset();

        std::int64_t column_int(int index) const;
        const unsigned char *column_text(int index) const;
        bool column_null(int index) const;

        int column_count() const noexcept
        {
            return sqlite3_stmt_ ? sqlite3_column_count(sqlite3_stmt_) : 0;
        }

    private:
        sqlite3_stmt *sqlite3_stmt_ = nullptr;
        Database *db_ = nullptr;
    };

} // namespace bms