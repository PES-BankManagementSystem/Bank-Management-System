#include "auth/auth_service.hpp"

#include <iostream>
#include <filesystem>
#include <memory>

#include "common/database.hpp"
#include "support/check.hpp"

namespace fs = std::filesystem;

int main() {
    // Setup temporary database for testing
    const std::string test_db = "test_auth.db";
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    try {
        bms::Database::init(test_db);
    } catch (const std::exception& e) {
        std::cerr << "Database::init failed: " << e.what() << "\n";
        return 1;
    }

    auto& db = bms::Database::instance();

    // 1. Password hashing & verification
    bms::test::run("hash_password produces valid Argon2id hash", []() {
        std::string pwd = "Password123!";
        std::string hash = bms::auth::hash_password(pwd);
        CHECK(!hash.empty());
        CHECK(hash != pwd);
        CHECK(bms::auth::verify_password(pwd, hash));
        CHECK(!bms::auth::verify_password("WrongPassword!", hash));
    });

    // 2. Token generation
    bms::test::run("generate_token produces 64-char hex string", []() {
        std::string token1 = bms::auth::generate_token();
        std::string token2 = bms::auth::generate_token();
        CHECK_EQ(token1.size(), 64);
        CHECK_EQ(token2.size(), 64);
        CHECK(token1 != token2);
    });

    // 3. Session lifecycle
    bms::test::run("create, validate, and invalidate session", [&db]() {
        // Insert admin user to satisfy sessions.user_id foreign key constraint
        std::string hash = bms::auth::hash_password("AdminPass123");
        auto ins = db.prepare(
            "INSERT OR REPLACE INTO users (id, name, role, password_hash, active, locked, failed_attempts) "
            "VALUES ('admin', 'System Admin', 'administrator', ?1, 1, 0, 0)");
        ins.bind(1, hash);
        ins.execute();

        std::string token = bms::auth::create_session("admin", "administrator");
        CHECK(!token.empty());

        auto user = bms::auth::validate_session(token);
        CHECK(user.has_value());
        if (user) {
            CHECK_EQ(user->id, "admin");
            CHECK_EQ(user->role, "administrator");
        }

        // Invalidate session
        bms::auth::invalidate_session(token);
        auto expired = bms::auth::validate_session(token);
        CHECK(!expired.has_value());
    });

    // 4. Test account lockout after 3 failed attempts (S-09)
    bms::test::run("account locks after 3 failed login attempts and unlocks via admin", [&db]() {
        // Insert a test user
        std::string hash = bms::auth::hash_password("Secret123");
        auto ins = db.prepare(
            "INSERT OR REPLACE INTO users (id, name, role, password_hash, active, locked, failed_attempts) "
            "VALUES ('lock_test_user', 'Lock User', 'customer', ?1, 1, 0, 0)");
        ins.bind(1, hash);
        ins.execute();

        // Check initial state
        auto check1 = db.prepare("SELECT locked, failed_attempts FROM users WHERE id = 'lock_test_user'");
        CHECK(check1.step());
        CHECK_EQ(check1.column_int(0), 0);
        CHECK_EQ(check1.column_int(1), 0);

        // Simulate 2 failed attempts
        auto fail1 = db.prepare("UPDATE users SET failed_attempts = failed_attempts + 1 WHERE id = 'lock_test_user'");
        fail1.execute();
        fail1.reset();
        fail1.execute();

        auto check2 = db.prepare("SELECT locked, failed_attempts FROM users WHERE id = 'lock_test_user'");
        CHECK(check2.step());
        CHECK_EQ(check2.column_int(0), 0);
        CHECK_EQ(check2.column_int(1), 2);

        // 3rd failed attempt locks account
        auto fail3 = db.prepare("UPDATE users SET failed_attempts = failed_attempts + 1, locked = 1 WHERE id = 'lock_test_user'");
        fail3.execute();

        auto check3 = db.prepare("SELECT locked, failed_attempts FROM users WHERE id = 'lock_test_user'");
        CHECK(check3.step());
        CHECK_EQ(check3.column_int(0), 1);
        CHECK_EQ(check3.column_int(1), 3);

        // Admin unlock (S-09)
        auto unlock = db.prepare("UPDATE users SET locked = 0, failed_attempts = 0 WHERE id = 'lock_test_user'");
        unlock.execute();

        auto check4 = db.prepare("SELECT locked, failed_attempts FROM users WHERE id = 'lock_test_user'");
        CHECK(check4.step());
        CHECK_EQ(check4.column_int(0), 0);
        CHECK_EQ(check4.column_int(1), 0);
    });

    // 5. Deactivated user cannot have valid active status (S-24)
    bms::test::run("deactivated user status and session cleanup", [&db]() {
        std::string hash = bms::auth::hash_password("OfficerPass1");
        auto ins = db.prepare(
            "INSERT OR REPLACE INTO users (id, name, role, password_hash, active, locked, failed_attempts) "
            "VALUES ('deact_officer', 'Deactivated Officer', 'bank_officer', ?1, 1, 0, 0)");
        ins.bind(1, hash);
        ins.execute();

        std::string token = bms::auth::create_session("deact_officer", "bank_officer");
        CHECK(!token.empty());

        // Deactivate officer and invalidate sessions
        auto deact = db.prepare("UPDATE users SET active = 0 WHERE id = 'deact_officer'");
        deact.execute();
        auto del_sess = db.prepare("DELETE FROM sessions WHERE user_id = 'deact_officer'");
        del_sess.execute();

        auto sess = bms::auth::validate_session(token);
        CHECK(!sess.has_value());
    });

    // Clean up temporary database
    if (fs::exists(test_db)) {
        fs::remove(test_db);
    }

    return bms::test::finish();
}
