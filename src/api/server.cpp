#include "api/server.hpp"

#include <exception>
#include <filesystem>
#include <stdexcept>
#include <utility>

#include <httplib.h>

#include "api/access_log.hpp"
#include "api/errors.hpp"
#include "common/log.hpp"
#include "common/version.hpp"

namespace bms::api {
namespace {

constexpr std::size_t kMaxRequestBodyBytes = 1024 * 1024;

// Fills in a JSON body for errors produced outside our handlers: unknown
// routes, oversized requests, malformed HTTP. Errors written by handlers
// already have a body and are left alone.
httplib::Server::HandlerResponse fill_empty_error(const httplib::Request&, httplib::Response& res) {
    if (!res.body.empty()) return httplib::Server::HandlerResponse::Unhandled;

    switch (res.status) {
        case 400:
            write_json(res, 400, {{"code", "BAD_REQUEST"}, {"message", "The request is malformed."}});
            break;
        case 404:
            write_json(res, 404, {{"code", "NOT_FOUND"}, {"message", "No such page or endpoint."}});
            break;
        case 405:
            write_json(res, 405, {{"code", "METHOD_NOT_ALLOWED"},
                                  {"message", "This endpoint does not support that method."}});
            break;
        case 413:
            write_json(res, 413, {{"code", "PAYLOAD_TOO_LARGE"},
                                  {"message", "The request body is too large."}});
            break;
        case 500:
            write_internal_error(res);
            break;
        default:
            write_json(res, res.status, {{"code", "HTTP_" + std::to_string(res.status)},
                                         {"message", httplib::status_message(res.status)}});
    }
    return httplib::Server::HandlerResponse::Handled;
}

}  // namespace

Server::Server(Config config)
    : config_(std::move(config)), http_(std::make_unique<httplib::Server>()), router_(*http_) {
    // httplib's default sets SO_REUSEPORT on Linux, which lets a second server
    // bind the same port and silently take half the requests. Allow a quick
    // restart (SO_REUSEADDR) but never a shared port.
    http_->set_socket_options([](socket_t sock) {
#ifdef _WIN32
        httplib::set_socket_opt(sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, 1);
#else
        httplib::set_socket_opt(sock, SOL_SOCKET, SO_REUSEADDR, 1);
#endif
    });
    http_->set_payload_max_length(kMaxRequestBodyBytes);
    http_->set_default_headers({
        {"X-Content-Type-Options", "nosniff"},
        {"X-Frame-Options", "DENY"},
        {"Referrer-Policy", "no-referrer"},
    });

    http_->set_logger([](const httplib::Request& req, const httplib::Response& res) {
        log::info(format_access_log(req, res));
    });

    // Last line of defence for exceptions thrown outside Router handlers.
    http_->set_exception_handler(
        [](const httplib::Request& req, httplib::Response& res, std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                log::error(req.method + " " + req.path + " failed: " + e.what());
            } catch (...) {
                log::error(req.method + " " + req.path + " failed with an unknown exception");
            }
            write_internal_error(res);
        });

    http_->set_error_handler(fill_empty_error);

    router_.get("/health", [](const Request&) {
        return Response::ok({{"status", "ok"}, {"version", kVersion}});
    });

    namespace fs = std::filesystem;
    std::error_code ec;
    if (!config_.frontend_dir.empty() && fs::is_directory(config_.frontend_dir, ec)) {
        http_->set_mount_point("/", config_.frontend_dir);
    } else {
        log::warning("Front-end folder '" + config_.frontend_dir +
                     "' not found; only the API is served. Use --frontend to set it.");
    }
}

Server::~Server() = default;

int Server::bind() {
    int port = config_.port;
    bool ok = false;
    if (port == 0) {
        port = http_->bind_to_any_port(config_.host);
        ok = port > 0;
    } else {
        ok = http_->bind_to_port(config_.host, port);
    }
    if (!ok) {
        throw std::runtime_error("Could not listen on " + config_.host + ":" +
                                 std::to_string(config_.port) +
                                 ". Is another program using that port?");
    }
    return port;
}

void Server::run() { http_->listen_after_bind(); }

void Server::stop() { http_->stop(); }

void Server::wait_until_ready() const { http_->wait_until_ready(); }

}  // namespace bms::api
