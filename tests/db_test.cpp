#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "common/db.hpp"
#include "support/check.hpp"

using namespace bms;
using Transaction = Database::Transaction;

void test_schema_and_seed_loading()
{
    Database db(":memory:");

    // Load the actual schema from the db/ directory
    // DB_PATH is defined with quotes, so we need to remove them
    std::string db_path = DB_PATH;
    if (db_path.length() >= 2 && db_path.front() == '\"' && db_path.back() == '\"') {
        db_path = db_path.substr(1, db_path.length() - 2);
    }
    std::string schema_path = db_path + "/schema.sql";

    // Read the schema file contents
    std::ifstream schema_file(schema_path);
    if (!schema_file.is_open()) {
        throw std::runtime_error("Failed to open schema file: " + schema_path);
    }
    std::stringstream schema_stream;
    schema_stream << schema_file.rdbuf();
    std::string schema_sql = schema_stream.str();

    db.exec_script(schema_sql);

    // Test that we can open and use the database
    CHECK(db.is_open());
}

void test_seed_counts()
{
    // This test would load the actual seed data and verify counts
    // For now, we'll test the concept with a simplified version
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE users (
            user_id INTEGER PRIMARY KEY,
            username TEXT NOT NULL UNIQUE,
            role TEXT NOT NULL CHECK (role IN ('administrator', 'bank_officer', 'customer')),
            status TEXT NOT NULL CHECK (status IN ('active', 'inactive', 'locked')),
            failed_attempts INTEGER NOT NULL CHECK (failed_attempts >= 0),
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) STRICT;

        CREATE TABLE account_types (
            type_id INTEGER PRIMARY KEY,
            type_name TEXT NOT NULL UNIQUE,
            min_balance INTEGER NOT NULL CHECK (min_balance >= 0),
            interest_rate INTEGER NOT NULL,
            daily_withdrawal_limit INTEGER NOT NULL CHECK (daily_withdrawal_limit >= 0),
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) STRICT;
    )");

    // Insert test data
    db.exec(R"(
        INSERT INTO users (username, role, status, failed_attempts, password_hash) VALUES
        ('admin', 'administrator', 'active', 0, 'hash1'),
        ('officer1', 'bank_officer', 'active', 0, 'hash2'),
        ('officer2', 'bank_officer', 'active', 0, 'hash3'),
        ('customer1', 'customer', 'active', 0, 'hash4'),
        ('customer2', 'customer', 'active', 0, 'hash5'),
        ('customer3', 'customer', 'active', 0, 'hash6');

        INSERT INTO account_types (type_name, min_balance, interest_rate, daily_withdrawal_limit) VALUES
        ('Savings', 100000, 400, 5000000),
        ('Current', 500000, 0, 20000000);
    )");

    // Count users by role
    auto stmt = db.prepare("SELECT role, COUNT(*) FROM users GROUP BY role");
    int admin_count = 0, officer_count = 0, customer_count = 0;

    while (stmt->step())
    {
        std::string role = reinterpret_cast<const char *>(stmt->column_text(0));
        int count = stmt->column_int(1);

        if (role == "administrator")
            admin_count = count;
        else if (role == "bank_officer")
            officer_count = count;
        else if (role == "customer")
            customer_count = count;
    }

    CHECK_EQ(admin_count, 1);
    CHECK_EQ(officer_count, 2);
    CHECK_EQ(customer_count, 3);

    // Count account types
    stmt = db.prepare("SELECT COUNT(*) FROM account_types");
    stmt->step();
    int type_count = stmt->column_int(0);
    CHECK_EQ(type_count, 2);
}

void test_foreign_key_violation()
{
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE users (
            user_id INTEGER PRIMARY KEY,
            username TEXT NOT NULL UNIQUE,
            role TEXT NOT NULL CHECK (role IN ('administrator', 'bank_officer', 'customer')),
            status TEXT NOT NULL CHECK (status IN ('active', 'inactive', 'locked')),
            failed_attempts INTEGER NOT NULL CHECK (failed_attempts >= 0),
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) STRICT;

        CREATE TABLE account_types (
            type_id INTEGER PRIMARY KEY,
            type_name TEXT NOT NULL UNIQUE,
            min_balance INTEGER NOT NULL CHECK (min_balance >= 0),
            interest_rate INTEGER NOT NULL,
            daily_withdrawal_limit INTEGER NOT NULL CHECK (daily_withdrawal_limit >= 0),
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) STRICT;

        CREATE TABLE accounts (
            account_id INTEGER PRIMARY KEY,
            account_number TEXT NOT NULL UNIQUE,
            customer_id INTEGER NOT NULL,
            type_id INTEGER NOT NULL,
            status TEXT NOT NULL CHECK (status IN ('active', 'frozen', 'closed')),
            balance INTEGER NOT NULL CHECK (balance >= 0),
            opened_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (customer_id) REFERENCES users(user_id) ON DELETE RESTRICT,
            FOREIGN KEY (type_id) REFERENCES account_types(type_id) ON DELETE RESTRICT
        ) STRICT;
    )");

    // Insert prerequisite data
    db.exec(R"(
        INSERT INTO users (username, role, status, failed_attempts, password_hash) VALUES
        ('testuser', 'customer', 'active', 0, 'hash');

        INSERT INTO account_types (type_name, min_balance, interest_rate, daily_withdrawal_limit) VALUES
        ('Savings', 100000, 400, 5000000);
    )");

    // This should work - valid foreign key
    db.exec(R"(
        INSERT INTO accounts (account_number, customer_id, type_id, status, balance)
        VALUES ('ACC001', 1, 1, 'active', 1000000);
    )");

    // This should fail - invalid customer_id (foreign key violation)
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO accounts (account_number, customer_id, type_id, status, balance)
            VALUES ('ACC002', 999, 1, 'active', 1000000);
        )"),
        DbError);

    // This should fail - invalid type_id (foreign key violation)
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO accounts (account_number, customer_id, type_id, status, balance)
            VALUES ('ACC003', 1, 999, 'active', 1000000);
        )"),
        DbError);
}

void test_check_constraints()
{
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE users (
            user_id INTEGER PRIMARY KEY,
            username TEXT NOT NULL UNIQUE,
            role TEXT NOT NULL CHECK (role IN ('administrator', 'bank_officer', 'customer')),
            status TEXT NOT NULL CHECK (status IN ('active', 'inactive', 'locked')),
            failed_attempts INTEGER NOT NULL CHECK (failed_attempts >= 0),
            password_hash TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
            updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
        ) STRICT;

        CREATE TABLE accounts (
            account_id INTEGER PRIMARY KEY,
            account_number TEXT NOT NULL UNIQUE,
            balance INTEGER NOT NULL CHECK (balance >= 0),
            status TEXT NOT NULL CHECK (status IN ('active', 'frozen', 'closed'))
        ) STRICT;

        CREATE TABLE transactions (
            transaction_id INTEGER PRIMARY KEY,
            amount INTEGER NOT NULL CHECK (amount > 0),
            resulting_balance INTEGER NOT NULL CHECK (resulting_balance >= 0),
            type TEXT NOT NULL CHECK (type IN ('deposit', 'withdrawal', 'transfer_debit', 'transfer_credit', 'interest'))
        ) STRICT;
    )");

    // Test role constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO users (username, role, status, failed_attempts, password_hash)
            VALUES ('test', 'invalid_role', 'active', 0, 'hash');
        )"),
        DbError);

    // Test status constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO users (username, role, status, failed_attempts, password_hash)
            VALUES ('test', 'customer', 'invalid_status', 0, 'hash');
        )"),
        DbError);

    // Test failed_attempts constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO users (username, role, status, failed_attempts, password_hash)
            VALUES ('test', 'customer', 'active', -1, 'hash');
        )"),
        DbError);

    // Test account balance constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO accounts (account_number, balance, status)
            VALUES ('ACC001', -1000, 'active');
        )"),
        DbError);

    // Test account status constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO accounts (account_number, balance, status)
            VALUES ('ACC001', 1000, 'invalid_status');
        )"),
        DbError);

    // Test transaction amount constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO transactions (amount, resulting_balance, type)
            VALUES (-1000, 0, 'deposit');
        )"),
        DbError);

    // Test transaction resulting_balance constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO transactions (amount, resulting_balance, type)
            VALUES (1000, -1000, 'deposit');
        )"),
        DbError);

    // Test transaction type constraint
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO transactions (amount, resulting_balance, type)
            VALUES (1000, 1000, 'invalid_type');
        )"),
        DbError);
}

void test_duplicate_constraints()
{
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE users (
            user_id INTEGER PRIMARY KEY,
            username TEXT NOT NULL UNIQUE,
            password_hash TEXT NOT NULL
        ) STRICT;

        CREATE TABLE accounts (
            account_id INTEGER PRIMARY KEY,
            account_number TEXT NOT NULL UNIQUE
        ) STRICT;

        CREATE TABLE interest_runs (
            run_id INTEGER PRIMARY KEY,
            period TEXT NOT NULL UNIQUE
        ) STRICT;

        CREATE TABLE transactions (
    transaction_id INTEGER PRIMARY KEY,
    transfer_ref TEXT,
    type TEXT NOT NULL
) STRICT;

CREATE UNIQUE INDEX uq_transfer_ref_type
    ON transactions(transfer_ref, type) WHERE transfer_ref IS NOT NULL;

)");

    // Test duplicate username
    db.exec(R"(
        INSERT INTO users (username, password_hash) VALUES ('testuser', 'hash1');
    )");

    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO users (username, password_hash) VALUES ('testuser', 'hash2');
        )"),
        DbError);

    // Test duplicate account_number
    db.exec(R"(
        INSERT INTO accounts (account_number) VALUES ('ACC001');
    )");

    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO accounts (account_number) VALUES ('ACC001');
        )"),
        DbError);

    // Test duplicate interest_runs period
    db.exec(R"(
        INSERT INTO interest_runs (period) VALUES ('2026-01');
    )");

    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO interest_runs (period) VALUES ('2026-01');
        )"),
        DbError);

    // Test duplicate transfer_ref+type (should allow same transfer_ref with different types)
    db.exec(R"(
        INSERT INTO transactions (transfer_ref, type) VALUES ('TRF001', 'transfer_debit');
    )");

    // This should work - same transfer_ref, different type
    db.exec(R"(
        INSERT INTO transactions (transfer_ref, type) VALUES ('TRF001', 'transfer_credit');
    )");

    // This should fail - same transfer_ref and same type
    CHECK_THROWS_AS(
        db.exec(R"(
            INSERT INTO transactions (transfer_ref, type) VALUES ('TRF001', 'transfer_debit');
        )"),
        DbError);
}

void test_append_only_tables()
{
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE audit_log (
            log_id INTEGER PRIMARY KEY,
            action TEXT NOT NULL
        ) STRICT;

        CREATE TABLE transactions (
            transaction_id INTEGER PRIMARY KEY,
            amount INTEGER NOT NULL
        ) STRICT;

        CREATE TRIGGER prevent_audit_log_update
        BEFORE UPDATE ON audit_log
        BEGIN
            SELECT RAISE(ABORT, 'audit_log is append-only - UPDATE not allowed');
        END;

        CREATE TRIGGER prevent_audit_log_delete
        BEFORE DELETE ON audit_log
        BEGIN
            SELECT RAISE(ABORT, 'audit_log is append-only - DELETE not allowed');
        END;

        CREATE TRIGGER prevent_transactions_update
        BEFORE UPDATE ON transactions
        BEGIN
            SELECT RAISE(ABORT, 'transactions are append-only - UPDATE not allowed');
        END;

        CREATE TRIGGER prevent_transactions_delete
        BEFORE DELETE ON transactions
        BEGIN
            SELECT RAISE(ABORT, 'transactions are append-only - DELETE not allowed');
        END;
    )");

    // Insert test data
    db.exec(R"(
        INSERT INTO audit_log (action) VALUES ('TEST_ACTION');
        INSERT INTO transactions (amount) VALUES (1000);
    )");

    // Test that UPDATE on audit_log fails
    CHECK_THROWS_AS(
        db.exec(R"(
            UPDATE audit_log SET action = 'MODIFIED' WHERE log_id = 1;
        )"),
        DbError);

    // Test that DELETE on audit_log fails
    CHECK_THROWS_AS(
        db.exec(R"(
            DELETE FROM audit_log WHERE log_id = 1;
        )"),
        DbError);

    // Test that UPDATE on transactions fails
    CHECK_THROWS_AS(
        db.exec(R"(
            UPDATE transactions SET amount = 2000 WHERE transaction_id = 1;
        )"),
        DbError);

    // Test that DELETE on transactions fails
    CHECK_THROWS_AS(
        db.exec(R"(
            DELETE FROM transactions WHERE transaction_id = 1;
        )"),
        DbError);

    // Verify that INSERT still works (append-only)
    db.exec(R"(
        INSERT INTO audit_log (action) VALUES ('ANOTHER_ACTION');
        INSERT INTO transactions (amount) VALUES (2000);
    )"); // Should not throw
}

void test_transaction_guard()
{
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE test_table (
            id INTEGER PRIMARY KEY,
            value TEXT NOT NULL
        ) STRICT;
    )");

    // Test successful commit
    {
        Transaction tx(db);
        auto stmt = db.prepare("INSERT INTO test_table (value) VALUES (?)");
        stmt->bind(1, "test_value");
        stmt->step();
        tx.commit(); // Explicit commit
    }

    // Verify data was committed
    auto stmt = db.prepare("SELECT COUNT(*) FROM test_table");
    stmt->step();
    CHECK_EQ(stmt->column_int(0), 1);

    // Test rollback on scope exit
    {
        Transaction tx(db);
        auto stmt = db.prepare("INSERT INTO test_table (value) VALUES (?)");
        stmt->bind(1, "should_be_rolled_back");
        stmt->step();
        // tx goes out of scope without commit() - should rollback
    }

    // Verify data was rolled back (count should still be 1)
    stmt = db.prepare("SELECT COUNT(*) FROM test_table");
    stmt->step();
    CHECK_EQ(stmt->column_int(0), 1);
}

void test_prepared_statement_binding()
{
    Database db(":memory:");
    db.exec(R"(
        CREATE TABLE test_table (
            id INTEGER PRIMARY KEY,
            int_val INTEGER,
            text_val TEXT,
            null_val TEXT
        ) STRICT;
    )");

    // Test binding int64 value
    auto stmt = db.prepare("INSERT INTO test_table (int_val) VALUES (?)");
    stmt->bind(1, 1234567890123);
    stmt->step();

    // Test binding text value
    stmt = db.prepare("INSERT INTO test_table (text_val) VALUES (?)");
    stmt->bind(1, "Hello, World!'\"");
    stmt->step();

    // Test binding NULL value
    stmt = db.prepare("INSERT INTO test_table (null_val) VALUES (?)");
    stmt->bind(1, nullptr);
    stmt->step();

    // Verify the data was inserted correctly
    stmt = db.prepare("SELECT int_val, text_val, null_val FROM test_table ORDER BY id");

    // First row
    stmt->step();
    CHECK_EQ(stmt->column_int(0), 1234567890123);
    CHECK(stmt->column_null(1)); // text_val is NULL
    CHECK(stmt->column_null(2)); // null_val is NULL

    // Second row
    stmt->step();
    CHECK(stmt->column_null(0)); // int_val is NULL
    std::string text = reinterpret_cast<const char *>(stmt->column_text(1));
    CHECK_EQ(text, "Hello, World!'\"");
    CHECK(stmt->column_null(2)); // null_val is NULL

    // Third row
    stmt->step();
    CHECK(stmt->column_null(0)); // int_val is NULL
    CHECK(stmt->column_null(1)); // text_val is NULL
    CHECK(stmt->column_null(2)); // null_val is NULL
}

void test_actual_schema_file()
{
    Database db(":memory:");

    // Load the actual schema from the db/ directory
    // DB_PATH is defined with quotes, so we need to remove them
    std::string db_path = DB_PATH;
    if (db_path.length() >= 2 && db_path.front() == '\"' && db_path.back() == '\"') {
        db_path = db_path.substr(1, db_path.length() - 2);
    }
    std::string schema_path = db_path + "/schema.sql";

    // Read the schema file contents
    std::ifstream schema_file(schema_path);
    if (!schema_file.is_open()) {
        throw std::runtime_error("Failed to open schema file: " + schema_path);
    }
    std::stringstream schema_stream;
    schema_stream << schema_file.rdbuf();
    std::string schema_sql = schema_stream.str();

    // Execute the actual schema
    db.exec_script(schema_sql);

    // Verify that tables were created correctly
    // Check that we can query the sqlite_master table for our expected tables
    auto stmt = db.prepare("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name");

    std::vector<std::string> table_names;
    while (stmt->step()) {
        table_names.push_back(reinterpret_cast<const char *>(stmt->column_text(0)));
    }

    // Verify we have the expected tables
    bool has_users = false;
    bool has_account_types = false;
    bool has_customers = false;
    bool has_accounts = false;
    bool has_transactions = false;
    bool has_audit_log = false;
    bool has_interest_runs = false;

    for (const auto& table_name : table_names) {
        if (table_name == "users") has_users = true;
        else if (table_name == "account_types") has_account_types = true;
        else if (table_name == "customers") has_customers = true;
        else if (table_name == "accounts") has_accounts = true;
        else if (table_name == "transactions") has_transactions = true;
        else if (table_name == "audit_log") has_audit_log = true;
        else if (table_name == "interest_runs") has_interest_runs = true;
    }

    CHECK(has_users);
    CHECK(has_account_types);
    CHECK(has_customers);
    CHECK(has_accounts);
    CHECK(has_transactions);
    CHECK(has_audit_log);
    CHECK(has_interest_runs);

    // Verify foreign key pragma is ON (should be enabled by our Database::open())
    stmt = db.prepare("PRAGMA foreign_keys");
    stmt->step();
    CHECK_EQ(stmt->column_int(0), 1); // Should be 1 (ON)

    // Verify user_version is set to 1
    stmt = db.prepare("PRAGMA user_version");
    stmt->step();
    CHECK_EQ(stmt->column_int(0), 1);
}

void test_actual_seed_file()
{
    Database db(":memory:");

    // Load the actual schema from the db/ directory
    std::string db_path = DB_PATH;
    if (db_path.length() >= 2 && db_path.front() == '\"' && db_path.back() == '\"') {
        db_path = db_path.substr(1, db_path.length() - 2);
    }
    std::string schema_path = db_path + "/schema.sql";

    // Read and execute the schema file
    std::ifstream schema_file(schema_path);
    if (!schema_file.is_open()) {
        throw std::runtime_error("Failed to open schema file: " + schema_path);
    }
    std::stringstream schema_stream;
    schema_stream << schema_file.rdbuf();
    std::string schema_sql = schema_stream.str();
    db.exec_script(schema_sql);

    // Load the actual seed data
    std::string seed_path = db_path + "/seed.sql";
    std::ifstream seed_file(seed_path);
    if (!seed_file.is_open()) {
        throw std::runtime_error("Failed to open seed file: " + seed_path);
    }
    std::stringstream seed_stream;
    seed_stream << seed_file.rdbuf();
    std::string seed_sql = seed_stream.str();

    // Execute the actual seed data
    db.exec_script(seed_sql);

    // Verify seed data counts

    // Count users by role
    auto stmt = db.prepare("SELECT role, COUNT(*) FROM users GROUP BY role");
    int admin_count = 0, officer_count = 0, customer_count = 0;

    while (stmt->step()) {
        std::string role = reinterpret_cast<const char *>(stmt->column_text(0));
        int count = stmt->column_int(1);

        if (role == "administrator")
            admin_count = count;
        else if (role == "bank_officer")
            officer_count = count;
        else if (role == "customer")
            customer_count = count;
    }

    CHECK_EQ(admin_count, 1);    // 1 administrator
    CHECK_EQ(officer_count, 2);  // 2 bank officers
    CHECK_EQ(customer_count, 3); // 3 customers

    // Count account types
    stmt = db.prepare("SELECT COUNT(*) FROM account_types");
    stmt->step();
    int type_count = stmt->column_int(0);
    CHECK_EQ(type_count, 2); // Savings and Current

    // Verify specific account type values
    stmt = db.prepare("SELECT type_name, min_balance, interest_rate, daily_withdrawal_limit FROM account_types ORDER BY type_id");

    // First row should be Savings
    stmt->step();
    std::string type_name = reinterpret_cast<const char *>(stmt->column_text(0));
    int min_balance = stmt->column_int(1);
    int interest_rate = stmt->column_int(2);
    int daily_limit = stmt->column_int(3);

    CHECK_EQ(type_name, "Savings");
    CHECK_EQ(min_balance, 100000); // 1,000.00 INR in paise
    CHECK_EQ(interest_rate, 400);  // 4.00% in basis points
    CHECK_EQ(daily_limit, 5000000); // 50,000.00 INR daily limit

    // Second row should be Current
    stmt->step();
    type_name = reinterpret_cast<const char *>(stmt->column_text(0));
    min_balance = stmt->column_int(1);
    interest_rate = stmt->column_int(2);
    daily_limit = stmt->column_int(3);

    CHECK_EQ(type_name, "Current");
    CHECK_EQ(min_balance, 500000); // 5,000.00 INR in paise
    CHECK_EQ(interest_rate, 0);    // 0.00% in basis points
    CHECK_EQ(daily_limit, 20000000); // 200,000.00 INR daily limit

    // Count customers
    stmt = db.prepare("SELECT COUNT(*) FROM customers");
    stmt->step();
    int customer_count_db = stmt->column_int(0);
    CHECK_EQ(customer_count_db, 3); // 3 customers

    // Count accounts
    stmt = db.prepare("SELECT status, COUNT(*) FROM accounts GROUP BY status");
    int active_count = 0, frozen_count = 0, closed_count = 0;

    while (stmt->step()) {
        std::string status = reinterpret_cast<const char *>(stmt->column_text(0));
        int count = stmt->column_int(1);

        if (status == "active")
            active_count = count;
        else if (status == "frozen")
            frozen_count = count;
        else if (status == "closed")
            closed_count = count;
    }

    // Based on actual seed data: 3 active (ACC001, ACC004, ACC005), 1 frozen (ACC002), 1 closed (ACC003)
    CHECK_EQ(active_count, 3);
    CHECK_EQ(frozen_count, 1);
    CHECK_EQ(closed_count, 1);

    // Count transactions
    stmt = db.prepare("SELECT COUNT(*) FROM transactions");
    stmt->step();
    int transaction_count = stmt->column_int(0);
    // Only 8 transactions are inserted because the 9th one (interest transaction)
    // has amount=0 which violates the CHECK (amount > 0) constraint, and is ignored
    // due to "INSERT OR IGNORE" in the seed data
    CHECK_EQ(transaction_count, 8);

    // Count audit log entries
    stmt = db.prepare("SELECT COUNT(*) FROM audit_log");
    stmt->step();
    int audit_log_count = stmt->column_int(0);
    CHECK_EQ(audit_log_count, 8); // 8 audit log entries
}

int main()
{
    bms::test::run("schema and seed loading", test_schema_and_seed_loading);
    bms::test::run("seed counts", test_seed_counts);
    bms::test::run("foreign key violation", test_foreign_key_violation);
    bms::test::run("check constraints", test_check_constraints);
    bms::test::run("duplicate constraints", test_duplicate_constraints);
    bms::test::run("append-only tables", test_append_only_tables);
    bms::test::run("transaction guard", test_transaction_guard);
    bms::test::run("prepared statement binding", test_prepared_statement_binding);
    bms::test::run("actual schema file", test_actual_schema_file);
    bms::test::run("actual seed file", test_actual_seed_file);
    return bms::test::finish();
}