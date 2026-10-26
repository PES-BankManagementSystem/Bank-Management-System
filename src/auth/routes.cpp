#include "auth/routes.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <sodium.h>

#include "api/router.hpp"
#include "auth/password.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::auth {
namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kSessionLifetime = std::chrono::minutes(30);

struct Session {
    std::int64_t user_id;
    Clock::time_point expires_at;
};

std::mutex sessions_mutex;
std::unordered_map<std::string, Session> sessions;

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::string new_token() {
    std::array<unsigned char, 32> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    std::array<char, 65> encoded{};
    sodium_bin2hex(encoded.data(), encoded.size(), bytes.data(), bytes.size());
    return encoded.data();
}

std::optional<std::string> bearer_token(const api::Request& request) {
    const auto authorization = request.header("Authorization");
    if (!authorization || authorization->compare(0, 7, "Bearer ") != 0 ||
        authorization->size() <= 7) {
        return std::nullopt;
    }
    return authorization->substr(7);
}

std::optional<Session> find_session(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    const auto now = Clock::now();
    for (auto it = sessions.begin(); it != sessions.end();) {
        if (it->second.expires_at <= now) it = sessions.erase(it);
        else ++it;
    }
    const auto it = sessions.find(token);
    if (it == sessions.end()) return std::nullopt;
    return it->second;
}

void remove_session(const std::string& token) {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    sessions.erase(token);
}

void invalid_login() {
    throw unauthenticated("Invalid username or password.");
}

}  // namespace

Principal authenticate(const api::Request& request, Database& database,
                       bool allow_password_change) {
    const auto token = bearer_token(request);
    if (!token) throw unauthenticated();

    const auto session = find_session(*token);
    if (!session) throw unauthenticated("The session is invalid or has expired.");

    auto query = database.prepare(
        "SELECT role, status, must_change_password FROM users WHERE user_id = ?");
    query->bind(1, session->user_id);
    if (!query->step()) {
        remove_session(*token);
        throw unauthenticated("The session is invalid or has expired.");
    }

    const std::string role = column_string(*query, 0);
    const std::string status = column_string(*query, 1);
    if (status != "active") {
        remove_session(*token);
        throw unauthenticated("The session is invalid or has expired.");
    }
    const bool must_change_password = query->column_int(2) != 0;
    if (must_change_password && !allow_password_change) {
        throw forbidden("A password change is required before using this service.");
    }
    return {session->user_id, role, must_change_password};
}

void expire_all_sessions_for_testing() {
    std::lock_guard<std::mutex> lock(sessions_mutex);
    for (auto& entry : sessions) entry.second.expires_at = Clock::now() - std::chrono::seconds(1);
}

void register_routes(api::Router& router, Database& database, bool allow_plain_http) {
    router.post("/auth/login", [&database, allow_plain_http](const api::Request& request) {
        if (!allow_plain_http) {
            throw forbidden("Sign-in is disabled on network-accessible HTTP listeners. Enable HTTPS.");
        }
        const auto& body = request.json();
        if (!body.contains("username") || !body["username"].is_string() ||
            !body.contains("password") || !body["password"].is_string()) {
            invalid_login();
        }

        const std::string username = body["username"].get<std::string>();
        const std::string password = body["password"].get<std::string>();
        if (username.empty() || password.empty() || username.size() > 128 ||
            password.size() > 1024) {
            invalid_login();
        }

        Database::Transaction transaction(database, true);
        auto query = database.prepare(
            "SELECT user_id, role, status, failed_attempts, password_hash, must_change_password "
            "FROM users WHERE username = ?");
        query->bind(1, username);
        if (!query->step()) {
            transaction.rollback();
            invalid_login();
        }

        const std::int64_t user_id = query->column_int(0);
        const std::string role = column_string(*query, 1);
        const std::string status = column_string(*query, 2);
        const std::string hash = column_string(*query, 4);
        const bool must_change_password = query->column_int(5) != 0;
        if (status != "active" || !verify_password(password, hash)) {
            auto failed = database.prepare(
                "UPDATE users SET failed_attempts = failed_attempts + 1, "
                "status = CASE WHEN failed_attempts + 1 >= 3 THEN 'locked' ELSE status END, "
                "updated_at = CURRENT_TIMESTAMP WHERE user_id = ? AND status = 'active'");
            failed->bind(1, user_id);
            failed->step();
            transaction.commit();
            invalid_login();
        }

        auto reset = database.prepare(
            "UPDATE users SET failed_attempts = 0, updated_at = CURRENT_TIMESTAMP "
            "WHERE user_id = ?");
        reset->bind(1, user_id);
        reset->step();
        transaction.commit();

        if (sodium_init() < 0) {
            throw std::runtime_error("Unable to initialize secure session services.");
        }
        const std::string token = new_token();
        {
            std::lock_guard<std::mutex> lock(sessions_mutex);
            sessions[token] = {user_id, Clock::now() + kSessionLifetime};
        }
        return api::Response::ok({
            {"accessToken", token},
            {"tokenType", "Bearer"},
            {"expiresIn", std::chrono::duration_cast<std::chrono::seconds>(kSessionLifetime).count()},
            {"role", role},
            {"mustChangePassword", must_change_password},
        });
    });

    router.post("/auth/logout", [allow_plain_http](const api::Request& request) {
        if (!allow_plain_http) {
            throw forbidden("Sign-out is disabled on network-accessible HTTP listeners. Enable HTTPS.");
        }
        const auto token = bearer_token(request);
        if (!token) throw unauthenticated();
        remove_session(*token);
        return api::Response::no_content();
    });

    router.post("/auth/change-password", [&database, allow_plain_http](const api::Request& request) {
        if (!allow_plain_http) {
            throw forbidden("Password changes are disabled on network-accessible HTTP listeners. Enable HTTPS.");
        }
        const Principal principal = authenticate(request, database, true);
        const auto& body = request.json();
        if (!body.contains("newPassword") || !body["newPassword"].is_string()) {
            throw validation_error("newPassword", "A new password is required.");
        }
        const std::string password = body["newPassword"].get<std::string>();
        if (password.size() < 12 || password.size() > 1024) {
            throw validation_error("newPassword", "Password must be at least 12 characters.");
        }
        if (!principal.must_change_password) {
            throw conflict("PASSWORD_CHANGE_NOT_REQUIRED", "A password change is not required.");
        }

        const std::string hash = hash_password(password);
        auto update = database.prepare(
            "UPDATE users SET password_hash = ?, must_change_password = 0, "
            "failed_attempts = 0, updated_at = CURRENT_TIMESTAMP WHERE user_id = ?");
        update->bind(1, hash);
        update->bind(2, principal.user_id);
        update->step();
        return api::Response::ok({{"message", "Password changed."}});
    });
}

}  // namespace bms::auth
