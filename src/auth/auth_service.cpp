#include "auth/auth_service.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <sodium.h>

#include "common/database.hpp"
#include "common/log.hpp"

namespace bms::auth {

namespace {

// Session lifetime: 30 minutes.
constexpr int kSessionLifetimeMinutes = 30;

std::string utc_now() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

std::string utc_plus_minutes(int minutes) {
    auto now = std::chrono::system_clock::now();
    auto future = now + std::chrono::minutes(minutes);
    std::time_t t = std::chrono::system_clock::to_time_t(future);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

}  // namespace

std::string hash_password(const std::string& password) {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialisation failed");
    }
    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("Password hashing failed (out of memory?)");
    }
    return std::string(hash);
}

bool verify_password(const std::string& password, const std::string& hash) {
    if (sodium_init() < 0) return false;
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

std::string generate_token() {
    if (sodium_init() < 0) {
        throw std::runtime_error("libsodium initialisation failed");
    }
    // 32 random bytes → 64 hex characters.
    unsigned char buf[32];
    randombytes_buf(buf, sizeof buf);
    char hex[65];
    sodium_bin2hex(hex, sizeof hex, buf, sizeof buf);
    return std::string(hex);
}

std::string create_session(const std::string& user_id, const std::string& role) {
    std::string token = generate_token();
    std::string expires = utc_plus_minutes(kSessionLifetimeMinutes);

    auto& db = Database::instance();
    auto stmt = db.prepare(
        "INSERT INTO sessions (token, user_id, role, expires_at) VALUES (?1, ?2, ?3, ?4)");
    stmt.bind(1, token).bind(2, user_id).bind(3, role).bind(4, expires);
    stmt.execute();
    return token;
}

std::optional<AuthUser> validate_session(const std::string& token) {
    if (token.empty()) return std::nullopt;

    auto& db = Database::instance();
    auto stmt = db.prepare(
        "SELECT s.user_id, s.role, u.name FROM sessions s "
        "JOIN users u ON u.id = s.user_id "
        "WHERE s.token = ?1 AND s.expires_at > ?2 AND u.active = 1");
    stmt.bind(1, token).bind(2, utc_now());
    if (!stmt.step()) return std::nullopt;

    AuthUser user;
    user.id = stmt.column_text(0);
    user.role = stmt.column_text(1);
    user.name = stmt.column_text(2);
    return user;
}

void invalidate_session(const std::string& token) {
    auto& db = Database::instance();
    auto stmt = db.prepare("DELETE FROM sessions WHERE token = ?1");
    stmt.bind(1, token);
    stmt.execute();
}

void cleanup_expired_sessions() {
    auto& db = Database::instance();
    auto stmt = db.prepare("DELETE FROM sessions WHERE expires_at <= ?1");
    stmt.bind(1, utc_now());
    stmt.execute();
}

}  // namespace bms::auth
