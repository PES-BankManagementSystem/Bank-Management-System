#pragma once

namespace bms::api {
class Router;
}

namespace bms::account {

// Registers this module's HTTP endpoints:
// POST /accounts, PATCH /accounts/:acc/status and
// /account-types (S-14, S-15, S-16).
void register_routes(api::Router& router);

}  // namespace bms::account
