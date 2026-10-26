#pragma once

#include <cstdint>
#include <string>

namespace bms::api {
class Router;
class Request;
}

namespace bms {
class Database;
}

namespace bms::auth {

struct Principal {
    std::int64_t user_id;
    std::string role;
    bool must_change_password;
};

void register_routes(api::Router& router, Database& database, bool allow_plain_http);
// Validates the bearer token on a protected request and returns the current
// user (id and role) for the module's handler (S-08). Throws 401 for a
// missing, unknown, expired or logged-out token, or an inactive/locked user.
Principal authenticate(const api::Request& request, Database& database,
                       bool allow_password_change = false);

// Test hook: makes every live session expire now, so tests can check expiry
// without waiting for the 30-minute lifetime.
void expire_all_sessions_for_testing();

}  // namespace bms::auth
