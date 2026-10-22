#include "auth/session.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <stdexcept>
#include <string>

#include "api/router.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::auth {
namespace {

constexpr std::size_t kSessionTokenBytes = 32;

std::string parse_bearer_token(const api::Request& request) {
    const auto header = request.header("Authorization");
    if (!header.has_value()) {
        throw unauthenticated();
    }
    const std::string& value = *header;
    constexpr char prefix[] = "Bearer ";
    if (value.size() <= sizeof(prefix) - 1 ||
        value.compare(0, sizeof(prefix) - 1, prefix) != 0) {
        throw unauthenticated();
    }
    const std::string token = value.substr(sizeof(prefix) - 1);
    if (token.empty()) throw unauthenticated();
    return token;
}

bool has_role(const std::string& role, std::initializer_list<const char*> roles) {
    return std::any_of(roles.begin(), roles.end(),
                       [&](const char* candidate) { return role == candidate; });
}

void ensure_sodium_initialized() {
    static const bool initialized = [] {
        if (sodium_init() < 0) {
            throw std::runtime_error("Unable to initialize sodium.");
        }
        return true;
    }();
    (void)initialized;
}

}  // namespace

void ensure_session_schema(Database& database) {
    database.exec(
        "CREATE TABLE IF NOT EXISTS auth_sessions ("
        "session_id INTEGER PRIMARY KEY,"
        "user_id INTEGER NOT NULL,"
        "token TEXT NOT NULL UNIQUE,"
        "created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,"
        "expires_at TEXT NOT NULL,"
        "revoked_at TEXT,"
        "FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE RESTRICT"
        ") STRICT");
    database.exec(
        "CREATE INDEX IF NOT EXISTS idx_auth_sessions_user_active "
        "ON auth_sessions(user_id, expires_at) WHERE revoked_at IS NULL");

    auto version = database.prepare("PRAGMA user_version");
    version->step();
    if (version->column_int(0) < 2) {
        database.exec("PRAGMA user_version = 2");
    }
}

AuthenticatedUser require_authenticated(Database& database, const api::Request& request) {
    const std::string token = parse_bearer_token(request);

    auto query = database.prepare(
        "SELECT s.user_id, u.role, c.customer_id "
        "FROM auth_sessions s "
        "JOIN users u ON u.user_id = s.user_id "
        "LEFT JOIN customers c ON c.user_id = u.user_id "
        "WHERE s.token = ? "
        "AND s.revoked_at IS NULL "
        "AND s.expires_at > CURRENT_TIMESTAMP "
        "AND u.status = 'active'");
    query->bind(1, token);
    if (!query->step()) {
        throw unauthenticated();
    }

    AuthenticatedUser user;
    user.user_id = query->column_int(0);
    const auto* role_text = query->column_text(1);
    user.role = role_text ? reinterpret_cast<const char*>(role_text) : "";
    if (!query->column_null(2)) {
        user.customer_id = query->column_int(2);
    }
    user.session_token = token;
    return user;
}

void require_role(const AuthenticatedUser& user, std::initializer_list<const char*> roles) {
    if (!has_role(user.role, roles)) {
        throw forbidden();
    }
}

std::string issue_session_token() {
    ensure_sodium_initialized();
    std::array<unsigned char, kSessionTokenBytes> bytes{};
    randombytes_buf(bytes.data(), bytes.size());

    std::string token;
    token.reserve(bytes.size() * 2);
    constexpr char hex[] = "0123456789abcdef";
    for (unsigned char byte : bytes) {
        token.push_back(hex[(byte >> 4) & 0x0F]);
        token.push_back(hex[byte & 0x0F]);
    }
    return token;
}

}  // namespace bms::auth
