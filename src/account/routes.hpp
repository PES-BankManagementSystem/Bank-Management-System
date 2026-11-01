#pragma once

namespace bms::api {
class Router;
}

namespace bms {
class Database;
}

namespace bms::account {

// Registers this module's HTTP endpoints:
// GET /customers/me/accounts, GET /accounts/:acc/balance,
// POST /accounts, PATCH /accounts/:acc/status and /account-types
// (S-14, S-15, S-16, S-20).
void register_routes(api::Router& router, Database& database);

}  // namespace bms::account
