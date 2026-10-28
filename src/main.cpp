#include <csignal>
#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "account/routes.hpp"
#include "api/router.hpp"
#include "api/server.hpp"
#include "audit/routes.hpp"
#include "auth/rbac.hpp"
#include "auth/routes.hpp"
#include "common/config.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "common/log.hpp"
#include "common/version.hpp"
#include "customer/routes.hpp"
#include "reporting/routes.hpp"
#include "staff/routes.hpp"
#include "transaction/routes.hpp"

namespace {

bms::api::Server* g_server = nullptr;

// Ctrl+C / SIGTERM: stop accepting requests and let main() return normally, so
// open database connections are closed cleanly.
extern "C" void handle_stop_signal(int) {
    if (g_server != nullptr) g_server->stop();
}

bool is_loopback_host(const std::string& host) {
    return host == "localhost" || host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

void register_all_routes(bms::api::Router& router, bms::Database& database,
                         bool allow_initial_credential_delivery) {
    bms::auth::register_routes(router, database, allow_initial_credential_delivery);
    bms::customer::register_routes(router, database, allow_initial_credential_delivery);
    bms::account::register_routes(router);
    bms::transaction::register_routes(router, database);
    bms::reporting::register_routes(router, database);
    bms::staff::register_routes(router, database, allow_initial_credential_delivery);
    bms::audit::register_routes(router, database, [&database](const bms::api::Request& request) {
        bms::auth::rbac::require(request, database, bms::auth::rbac::Operation::ViewAudit);
    });
}

}  // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    for (const std::string& arg : args) {
        if (arg == "--version") {
            std::cout << bms::kProjectName << ' ' << bms::kVersion << '\n';
            return 0;
        }
        if (arg == "--help") {
            std::cout << bms::usage();
            return 0;
        }
    }

    bms::Config config;
    try {
        config = bms::load_config(args, bms::system_env());
    } catch (const std::invalid_argument& e) {
        std::cerr << e.what() << "\nRun bms_server --help for usage.\n";
        return 2;
    }

    try {
        bms::Database database(config.db_path);
        bms::api::Server server(config);
        // One-time credential delivery and sign-in are allowed on loopback (local
        // development) or when the connection is encrypted.
        register_all_routes(server.router(), database,
                            is_loopback_host(config.host) || server.tls_enabled());
        const int port = server.bind();

        g_server = &server;
        std::signal(SIGINT, handle_stop_signal);
        std::signal(SIGTERM, handle_stop_signal);

        bms::log::info(std::string(bms::kProjectName) + ' ' + bms::kVersion +
                       " listening on " + (server.tls_enabled() ? "https://" : "http://") + config.host +
                       ':' + std::to_string(port));
        if (!server.tls_enabled() && !is_loopback_host(config.host)) {
            bms::log::warning(
                "Serving plain HTTP on a network address: sign-in and credential routes are "
                "disabled. Set --tls-cert and --tls-key to enable HTTPS.");
        }
        server.run();
        g_server = nullptr;
        bms::log::info("Server stopped");
    } catch (const std::exception& e) {
        bms::log::error(e.what());
        return 1;
    }
    return 0;
}
