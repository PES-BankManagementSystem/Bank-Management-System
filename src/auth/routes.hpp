#pragma once

namespace bms::api {
class Router;
}

namespace bms::auth {

// Registers this module's HTTP endpoints:
// POST /auth/login, POST /auth/logout and PATCH /users/:id/unlock
// (S-07, S-08, S-09).
void register_routes(api::Router& router);

}  // namespace bms::auth
