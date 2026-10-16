#include <csignal>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "account/routes.hpp"
#include "api/server.hpp"
#include "audit/routes.hpp"
#include "auth/routes.hpp"
#include "common/config.hpp"
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

void register_all_routes(bms::api::Router& router) {
    bms::auth::register_routes(router);
    bms::customer::register_routes(router);
    bms::account::register_routes(router);
    bms::transaction::register_routes(router);
    bms::reporting::register_routes(router);
    bms::staff::register_routes(router);
    bms::audit::register_routes(router);
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
        bms::api::Server server(config);
        register_all_routes(server.router());
        const int port = server.bind();

        g_server = &server;
        std::signal(SIGINT, handle_stop_signal);
        std::signal(SIGTERM, handle_stop_signal);

        bms::log::info(std::string(bms::kProjectName) + ' ' + bms::kVersion +
                       " listening on http://" + config.host + ':' + std::to_string(port));
        server.run();
        g_server = nullptr;
        bms::log::info("Server stopped");
    } catch (const std::exception& e) {
        bms::log::error(e.what());
        return 1;
    }
    return 0;
}
