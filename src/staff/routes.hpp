#pragma once

namespace bms::api {
class Router;
}

namespace bms {
class Database;
}

namespace bms::staff {

// Registers this module's HTTP endpoints (S-24, FR-13), Administrator only:
//   POST  /staff       create a Bank Officer with a one-time initial password
//   GET   /staff       list Bank Officers
//   PATCH /staff/:id   rename, deactivate or reactivate a Bank Officer
// GET /dashboard (S-25) will be added here later.
//
// allow_initial_credential_delivery is true only on loopback listeners or when
// the server serves HTTPS: the initial password is returned once in the
// response, so POST /staff is refused on network-accessible plain-HTTP
// listeners (S-28).
void register_routes(api::Router& router, Database& database,
                     bool allow_initial_credential_delivery);

}  // namespace bms::staff
