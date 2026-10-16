#pragma once

#include <memory>

#include "api/router.hpp"
#include "common/config.hpp"

namespace httplib {
class Server;
}

namespace bms::api {

// The HTTP server (SADS API layer). It provides:
//   - GET /health
//   - the static front end from config.frontend_dir, served at /
//   - the JSON error format for errors, unknown routes and unexpected exceptions
//   - an access log that never contains request bodies
//
// Modules add their endpoints through router() before bind() is called.
class Server {
public:
    explicit Server(Config config);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    Router& router() { return router_; }

    // Binds to config.host:config.port and returns the port actually used
    // (useful with port 0). Throws std::runtime_error if the port is taken.
    int bind();

    // Serves requests until stop() is called. Call bind() first.
    void run();

    // Thread-safe; makes run() return.
    void stop();

    // Blocks until run() is accepting connections.
    void wait_until_ready() const;

private:
    Config config_;
    std::unique_ptr<httplib::Server> http_;
    Router router_;
};

}  // namespace bms::api
