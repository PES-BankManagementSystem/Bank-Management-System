#pragma once

namespace bms::api {
class Router;
}

namespace bms {
class Database;
}

namespace bms::transaction {

// Registers this module's HTTP endpoints:
// POST /transactions and POST /transfers (S-17, S-18, S-19).
//
// Each handler requires a signed-in Bank Officer or Administrator; the
// operator recorded on the transaction is that user.
void register_routes(api::Router& router, Database& database);

}  // namespace bms::transaction
