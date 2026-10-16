#pragma once

namespace bms::api {
class Router;
}

namespace bms::reporting {

// Registers this module's HTTP endpoints:
// balance, history, statement and interest endpoints
// (S-20, S-21, S-22, S-23).
void register_routes(api::Router& router);

}  // namespace bms::reporting
