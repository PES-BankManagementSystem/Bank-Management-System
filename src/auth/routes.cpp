#include "auth/routes.hpp"

#include "api/router.hpp"
#include "auth/auth_service.hpp"
#include "auth/middleware.hpp"
#include "common/database.hpp"
#include "common/error.hpp"
#include "common/log.hpp"

namespace bms::auth {

namespace {

using api::Handler;
using api::Request;
using api::Response;
using Json = nlohmann::json;

// POST /auth/login  (S-07)
// Verifies user_id + password, handles failed-attempt counting and lockout (S-09).
Response handle_login(const Request& req) {
    const Json& body = req.json();
    if (!body.contains("user_id") || !body["user_id"].is_string()) {
        throw validation_error("user_id", "user_id is required.");
    }
    if (!body.contains("password") || !body["password"].is_string()) {
        throw validation_error("password", "password is required.");
    }

    const std::string user_id = body["user_id"].get<std::string>();
    const std::string password = body["password"].get<std::string>();

    auto& db = Database::instance();

    // Look up the user.
    auto stmt = db.prepare(
        "SELECT id, name, role, password_hash, failed_attempts, locked, active "
        "FROM users WHERE id = ?1");
    stmt.bind(1, user_id);
    if (!stmt.step()) {
        // Generic error: do not reveal whether the user exists (NFR-4).
        throw Error(ErrorKind::Unauthenticated, "INVALID_CREDENTIALS",
                    "Invalid user ID or password.");
    }

    const std::string id = stmt.column_text(0);
    const std::string name = stmt.column_text(1);
    const std::string role = stmt.column_text(2);
    const std::string hash = stmt.column_text(3);
    const int failed = stmt.column_int(4);
    const int locked = stmt.column_int(5);
    const int active = stmt.column_int(6);

    // Inactive user (deactivated officer/customer).
    if (!active) {
        throw Error(ErrorKind::Unauthenticated, "INVALID_CREDENTIALS",
                    "Invalid user ID or password.");
    }

    // Locked account (S-09).
    if (locked) {
        throw Error(ErrorKind::Unauthenticated, "ACCOUNT_LOCKED",
                    "Your account is locked after too many failed login attempts. "
                    "Please contact an Administrator to unlock it.");
    }

    // Wrong password.
    if (!verify_password(password, hash)) {
        int new_failed = failed + 1;
        if (new_failed >= 3) {
            // Lock the account (S-09).
            auto lock_stmt = db.prepare(
                "UPDATE users SET failed_attempts = ?1, locked = 1, "
                "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = ?2");
            lock_stmt.bind(1, new_failed).bind(2, id);
            lock_stmt.execute();
            log::warning("Account locked after " + std::to_string(new_failed) +
                         " failed attempts: " + id);
            throw Error(ErrorKind::Unauthenticated, "ACCOUNT_LOCKED",
                        "Your account has been locked after 3 failed login attempts. "
                        "Please contact an Administrator to unlock it.");
        } else {
            auto fail_stmt = db.prepare(
                "UPDATE users SET failed_attempts = ?1, "
                "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = ?2");
            fail_stmt.bind(1, new_failed).bind(2, id);
            fail_stmt.execute();
        }
        throw Error(ErrorKind::Unauthenticated, "INVALID_CREDENTIALS",
                    "Invalid user ID or password.");
    }

    // Successful login: reset failed_attempts counter.
    {
        auto reset = db.prepare(
            "UPDATE users SET failed_attempts = 0, "
            "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = ?1");
        reset.bind(1, id);
        reset.execute();
    }

    // Clean up expired sessions while we're here.
    cleanup_expired_sessions();

    // Create a new session.
    std::string token = create_session(id, role);

    return Response::ok({
        {"token", token},
        {"user", {{"id", id}, {"name", name}, {"role", role}}}
    });
}

// POST /auth/logout  (S-08)
Response handle_logout(const Request& req) {
    // Extract token from header.
    auto auth_header = req.header("Authorization");
    std::string token;
    if (auth_header) {
        const std::string& value = *auth_header;
        if (value.size() > 7 && value.substr(0, 7) == "Bearer ") {
            token = value.substr(7);
        }
    }
    if (token.empty()) {
        auto token_header = req.header("X-Session-Token");
        if (token_header) token = *token_header;
    }
    if (!token.empty()) {
        invalidate_session(token);
    }
    return Response::ok({{"message", "Logged out successfully."}});
}

// PATCH /users/:id/unlock  (S-09, Administrator only)
Response handle_unlock(const Request& req) {
    AuthUser admin = require_admin(req);
    const std::string target_id = req.param("id");

    auto& db = Database::instance();

    // Check the target user exists.
    auto check = db.prepare("SELECT id, locked FROM users WHERE id = ?1");
    check.bind(1, target_id);
    if (!check.step()) {
        throw not_found("User '" + target_id + "' not found.");
    }

    // Unlock the account and reset failed_attempts.
    auto unlock = db.prepare(
        "UPDATE users SET locked = 0, failed_attempts = 0, "
        "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = ?1");
    unlock.bind(1, target_id);
    unlock.execute();

    log::info("Account unlocked by " + admin.id + ": " + target_id);

    return Response::ok({{"message", "Account unlocked successfully."}, {"user_id", target_id}});
}

}  // namespace

void register_routes(api::Router& router) {
    router.post("/auth/login", handle_login);
    router.post("/auth/logout", handle_logout);
    router.patch("/users/:id/unlock", handle_unlock);
}

}  // namespace bms::auth
