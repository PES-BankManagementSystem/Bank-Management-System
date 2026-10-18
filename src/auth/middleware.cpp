#include "auth/middleware.hpp"

#include <algorithm>

#include "common/error.hpp"

namespace bms::auth {

namespace {

std::string extract_token(const api::Request& req) {
    // Try Authorization: Bearer <token> first.
    auto auth_header = req.header("Authorization");
    if (auth_header) {
        const std::string& value = *auth_header;
        if (value.size() > 7 && value.substr(0, 7) == "Bearer ") {
            return value.substr(7);
        }
    }
    // Fall back to X-Session-Token header.
    auto token_header = req.header("X-Session-Token");
    if (token_header) return *token_header;
    return "";
}

}  // namespace

AuthUser require_auth(const api::Request& req) {
    std::string token = extract_token(req);
    if (token.empty()) {
        throw unauthenticated("Please log in to continue.");
    }
    auto user = validate_session(token);
    if (!user) {
        throw unauthenticated("Session expired or invalid. Please log in again.");
    }
    return *user;
}

AuthUser require_role(const api::Request& req, const std::vector<std::string>& allowed_roles) {
    AuthUser user = require_auth(req);
    if (std::find(allowed_roles.begin(), allowed_roles.end(), user.role) == allowed_roles.end()) {
        throw forbidden("You are not allowed to perform this action.");
    }
    return user;
}

AuthUser require_admin(const api::Request& req) {
    return require_role(req, {"administrator"});
}

}  // namespace bms::auth
