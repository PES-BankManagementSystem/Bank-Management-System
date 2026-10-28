#pragma once

#include <functional>

namespace bms::api {
class Request;
class Router;
}

namespace bms {
class Database;
}

namespace bms::audit {

// Throws (401/403) unless the request comes from a signed-in Administrator.
// The caller supplies it so this module does not depend on the auth module,
// which itself writes audit entries.
using RequireAdministrator = std::function<void(const api::Request&)>;

// Registers this module's HTTP endpoints (S-27, NFR-5, TC-16), Administrator only:
//   GET /audit?from=YYYY-MM-DD&to=YYYY-MM-DD&account=<number>&limit=&offset=
// The trail is read-only: POST, PUT, PATCH and DELETE on /audit and /audit/:id
// are refused with 405 and no route can change or remove an entry.
void register_routes(api::Router& router, Database& database,
                     RequireAdministrator require_administrator);

}  // namespace bms::audit
