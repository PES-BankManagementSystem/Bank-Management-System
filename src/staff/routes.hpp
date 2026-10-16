#pragma once

namespace bms::api {
class Router;
}

namespace bms::staff {

// Registers this module's HTTP endpoints:
// /staff and GET /dashboard (S-24, S-25).
void register_routes(api::Router& router);

}  // namespace bms::staff
