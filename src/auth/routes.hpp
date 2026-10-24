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
Principal authenticate(const api::Request& request, Database& database,
                       bool allow_password_change = false);

}  // namespace bms::auth
