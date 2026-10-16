#pragma once

namespace bms::api {
class Router;
}

namespace bms::audit {

// Registers this module's HTTP endpoints:
// GET /audit (S-27).
void register_routes(api::Router& router);

}  // namespace bms::audit
