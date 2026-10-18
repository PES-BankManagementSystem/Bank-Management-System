#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace bms::auth {

// Authenticated user info, available after middleware validates the session.
struct AuthUser {
    std::string id;
    std::string role;
    std::string name;
};

// Hash a plaintext password with Argon2id (libsodium).
std::string hash_password(const std::string& password);

// Verify a plaintext password against an Argon2id hash.
bool verify_password(const std::string& password, const std::string& hash);

// Create a cryptographically random session token.
std::string generate_token();

// Store a session and return the token. Sessions expire after 30 minutes.
std::string create_session(const std::string& user_id, const std::string& role);

// Validate a session token. Returns the user info if valid and not expired.
std::optional<AuthUser> validate_session(const std::string& token);

// Invalidate (delete) a session token.
void invalidate_session(const std::string& token);

// Clean up expired sessions (called periodically or on login).
void cleanup_expired_sessions();

}  // namespace bms::auth
