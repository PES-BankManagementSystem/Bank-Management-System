#pragma once

#include <functional>
#include <optional>
#include <string>

#include "api/router.hpp"
#include "auth/auth_service.hpp"

namespace bms::auth {

// Extracts the session token from the Authorization header (Bearer scheme) or
// the X-Session-Token header. Validates it and returns the authenticated user.
// Throws bms::Error(Unauthenticated) if the token is missing, expired or
// invalid, so protected handlers never have to check themselves.
AuthUser require_auth(const api::Request& req);

// Like require_auth, but also checks that the user's role is one of the allowed
// roles. Throws bms::Error(Forbidden) if the role is not allowed.
AuthUser require_role(const api::Request& req, const std::vector<std::string>& allowed_roles);

// Convenience: require the caller to be an Administrator.
AuthUser require_admin(const api::Request& req);

}  // namespace bms::auth
