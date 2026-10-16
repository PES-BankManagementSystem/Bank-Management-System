#pragma once

namespace bms::api {
class Router;
}

namespace bms::customer {

// Registers this module's HTTP endpoints:
// POST /customers, GET /customers, GET /customers/:id and
// PUT /customers/:id (S-11, S-12, S-13).
void register_routes(api::Router& router);

}  // namespace bms::customer
