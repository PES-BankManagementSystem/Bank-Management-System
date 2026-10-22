#include "auth/routes.hpp"

#include <sodium.h>

#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

#include "api/router.hpp"
#include "auth/session.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::auth {
namespace {

constexpr int kMaxFailedAttempts = 3;
constexpr int kSessionTtlSeconds = 60 * 60;
constexpr char kGenericCredentialError[] = "Invalid username or password.";

std::int64_t parse_user_id_param(const api::Request& request) {
    const std::string raw = request.param("id");
    if (raw.empty()) {
        throw validation_error("id", "User id is required.");
    }
    std::size_t read = 0;
    const long long value = std::stoll(raw, &read, 10);
    if (read != raw.size() || value <= 0) {
        throw validation_error("id", "User id must be a positive integer.");
    }
    if (value > std::numeric_limits<std::int64_t>::max()) {
        throw validation_error("id", "User id is out of range.");
    }
    return static_cast<std::int64_t>(value);
}

void write_audit(Database& database, std::int64_t user_id, const std::string& action,
                 const std::string& entity, std::int64_t entity_id, const std::string& details) {
    auto insert = database.prepare(
        "INSERT INTO audit_log (user_id, action, entity, entity_id, details) "
        "VALUES (?, ?, ?, ?, ?)");
    insert->bind(1, user_id);
    insert->bind(2, action);
    insert->bind(3, entity);
    insert->bind(4, entity_id);
    insert->bind(5, details);
    insert->step();
}

}  // namespace

void register_routes(api::Router& router, Database& database) {
    ensure_session_schema(database);

    router.post("/auth/login", [&database](const api::Request& request) {
        const auto& body = request.json();
        if (!body.contains("username") || !body["username"].is_string()) {
            throw validation_error("username", "Username is required.");
        }
        if (!body.contains("password") || !body["password"].is_string()) {
            throw validation_error("password", "Password is required.");
        }
        const std::string username = body["username"].get<std::string>();
        const std::string password = body["password"].get<std::string>();
        if (username.empty()) throw validation_error("username", "Username is required.");
        if (password.empty()) throw validation_error("password", "Password is required.");

        Database::Transaction transaction(database, true);
        auto user = database.prepare(
            "SELECT user_id, role, status, failed_attempts, password_hash "
            "FROM users WHERE username = ?");
        user->bind(1, username);
        if (!user->step()) {
            throw unauthenticated(kGenericCredentialError);
        }
        const std::int64_t user_id = user->column_int(0);
        const std::string role =
            reinterpret_cast<const char*>(user->column_text(1));
        const std::string status =
            reinterpret_cast<const char*>(user->column_text(2));
        const std::int64_t failed_attempts = user->column_int(3);
        const std::string password_hash =
            reinterpret_cast<const char*>(user->column_text(4));

        const bool password_ok =
            (status == "active") &&
            crypto_pwhash_str_verify(password_hash.c_str(), password.c_str(),
                                     password.size()) == 0;
        if (!password_ok) {
            if (status == "active") {
                const std::int64_t next_attempts = failed_attempts + 1;
                const bool lock = next_attempts >= kMaxFailedAttempts;
                auto update = database.prepare(
                    "UPDATE users SET failed_attempts = ?, status = ?, updated_at = CURRENT_TIMESTAMP "
                    "WHERE user_id = ?");
                update->bind(1, next_attempts);
                update->bind(2, lock ? std::string("locked") : std::string("active"));
                update->bind(3, user_id);
                update->step();
                write_audit(database, user_id, "LOGIN_FAILED", "users", user_id,
                            lock ? "User locked after too many failed login attempts."
                                 : "Invalid credentials.");
            }
            throw unauthenticated(kGenericCredentialError);
        }

        auto reset = database.prepare(
            "UPDATE users SET failed_attempts = 0, updated_at = CURRENT_TIMESTAMP WHERE user_id = ?");
        reset->bind(1, user_id);
        reset->step();

        auto cleanup = database.prepare(
            "UPDATE auth_sessions SET revoked_at = CURRENT_TIMESTAMP "
            "WHERE user_id = ? AND revoked_at IS NULL");
        cleanup->bind(1, user_id);
        cleanup->step();

        const std::string token = issue_session_token();
        auto insert = database.prepare(
            "INSERT INTO auth_sessions (user_id, token, expires_at) "
            "VALUES (?, ?, datetime('now', '+' || ? || ' seconds'))");
        insert->bind(1, user_id);
        insert->bind(2, token);
        insert->bind(3, std::to_string(kSessionTtlSeconds));
        insert->step();

        auto expiry = database.prepare(
            "SELECT expires_at FROM auth_sessions WHERE token = ?");
        expiry->bind(1, token);
        expiry->step();
        const auto* expires_at_text = expiry->column_text(0);
        const std::string expires_at =
            expires_at_text ? reinterpret_cast<const char*>(expires_at_text) : "";

        write_audit(database, user_id, "LOGIN_SUCCESS", "users", user_id,
                    "User authenticated.");
        transaction.commit();

        return api::Response::ok({
            {"session_token", token},
            {"expires_at", expires_at},
            {"user", {{"user_id", user_id}, {"username", username}, {"role", role}}},
        });
    });

    router.post("/auth/logout", [&database](const api::Request& request) {
        const AuthenticatedUser auth = require_authenticated(database, request);
        Database::Transaction transaction(database, true);
        auto revoke = database.prepare(
            "UPDATE auth_sessions SET revoked_at = CURRENT_TIMESTAMP "
            "WHERE token = ? AND revoked_at IS NULL");
        revoke->bind(1, auth.session_token);
        revoke->step();
        write_audit(database, auth.user_id, "LOGOUT", "users", auth.user_id,
                    "Session ended.");
        transaction.commit();
        return api::Response::no_content();
    });

    router.patch("/users/:id/unlock", [&database](const api::Request& request) {
        const AuthenticatedUser auth = require_authenticated(database, request);
        require_role(auth, {"administrator"});
        const std::int64_t target_user_id = parse_user_id_param(request);

        Database::Transaction transaction(database, true);
        auto user = database.prepare("SELECT user_id FROM users WHERE user_id = ?");
        user->bind(1, target_user_id);
        if (!user->step()) throw not_found("No such user.");

        auto unlock = database.prepare(
            "UPDATE users SET status = 'active', failed_attempts = 0, "
            "updated_at = CURRENT_TIMESTAMP WHERE user_id = ?");
        unlock->bind(1, target_user_id);
        unlock->step();
        write_audit(database, auth.user_id, "USER_UNLOCK", "users", target_user_id,
                    "Administrator unlocked user.");
        transaction.commit();

        return api::Response::ok({
            {"user_id", target_user_id},
            {"status", "active"},
        });
    });
}

}  // namespace bms::auth
