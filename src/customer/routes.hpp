#pragma once

namespace bms::api {
class Router;
}

namespace bms {
class Database;
}

namespace bms::customer {

void register_routes(api::Router& router, Database& database,
                     bool allow_initial_credential_delivery);

}  // namespace bms::customer
