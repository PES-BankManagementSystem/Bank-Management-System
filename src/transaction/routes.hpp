#pragma once

namespace bms::api {
class Router;
}

namespace bms::transaction {

// Registers this module's HTTP endpoints:
// POST /transactions and POST /transfers (S-17, S-18, S-19).
void register_routes(api::Router& router);

}  // namespace bms::transaction
