#pragma once

#include <cstdint>

namespace bms::api {
class Router;
}

namespace bms {
class Database;
}

namespace bms::transaction {

// Registers this module's HTTP endpoints:
// POST /transactions and POST /transfers (S-17, S-18, S-19).
void register_routes(api::Router& router, Database& database, std::int64_t operator_id);

}  // namespace bms::transaction
