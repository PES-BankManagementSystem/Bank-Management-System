#pragma once

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>

namespace bms {
class Database;
}

namespace bms::api {
class Request;
}

namespace bms::auth {

struct AuthenticatedUser {
    std::int64_t user_id = 0;
    std::string role;
    std::optional<std::int64_t> customer_id;
    std::string session_token;
};

void ensure_session_schema(Database& database);
AuthenticatedUser require_authenticated(Database& database, const api::Request& request);
void require_role(const AuthenticatedUser& user, std::initializer_list<const char*> roles);
std::string issue_session_token();

}  // namespace bms::auth
