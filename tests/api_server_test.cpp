// End-to-end tests of the API server skeleton (S-04): a real server on a free
// localhost port, exercised with an HTTP client.

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/access_log.hpp"
#include "api/errors.hpp"
#include "api/server.hpp"
#include "common/error.hpp"
#include "common/log.hpp"
#include "common/version.hpp"
#include "support/check.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

namespace {

const std::string kSecretPassword = "hunter2-do-not-log";
const std::string kInternalDetail = "no such table: secret_internal_table";

// Collects log lines so tests can inspect what the server logged.
struct CapturedLog {
    std::mutex mutex;
    std::vector<std::string> lines;

    bool any_contains(const std::string& text) {
        std::lock_guard<std::mutex> lock(mutex);
        for (const auto& line : lines) {
            if (line.find(text) != std::string::npos) return true;
        }
        return false;
    }
};

fs::path make_frontend_dir() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    fs::path dir = fs::temp_directory_path() / ("bms_frontend_" + std::to_string(stamp));
    fs::create_directories(dir);
    std::ofstream(dir / "index.html") << "<!doctype html><h1>BMS test page</h1>\n";
    return dir;
}

void register_test_routes(bms::api::Router& router) {
    using bms::api::Request;
    using bms::api::Response;

    router.get("/test/created", [](const Request&) { return Response::created({{"id", 7}}); });
    router.get("/test/validation", [](const Request&) -> Response {
        throw bms::validation_error("amount", "Amount must be positive.");
    });
    router.get("/test/rule", [](const Request&) -> Response {
        throw bms::business_rule("INSUFFICIENT_BALANCE", "Balance would fall below the minimum.");
    });
    router.get("/test/crash", [](const Request&) -> Response {
        throw std::runtime_error(kInternalDetail);
    });
    router.get("/test/items/:id", [](const Request& req) {
        return Response::ok({{"id", req.param("id")}, {"q", req.query("q").value_or("")}});
    });
    router.post("/test/echo", [](const Request& req) { return Response::ok(req.json()); });
    // Stand-in for the real login endpoint (S-07), to check what gets logged.
    router.post("/auth/login", [](const Request& req) -> Response {
        (void)req.json();
        throw bms::unauthenticated("Invalid user ID or password.");
    });
}

json body_of(const httplib::Result& res) { return json::parse(res->body); }

}  // namespace

int main() {
    using bms::test::run;

    run("error kinds map to HTTP status codes", [] {
        using bms::ErrorKind;
        CHECK_EQ(bms::api::http_status(ErrorKind::Validation), 400);
        CHECK_EQ(bms::api::http_status(ErrorKind::Unauthenticated), 401);
        CHECK_EQ(bms::api::http_status(ErrorKind::Forbidden), 403);
        CHECK_EQ(bms::api::http_status(ErrorKind::NotFound), 404);
        CHECK_EQ(bms::api::http_status(ErrorKind::Conflict), 409);
        CHECK_EQ(bms::api::http_status(ErrorKind::BusinessRule), 422);
    });

    run("error body has code and message, and field only when set", [] {
        const json with_field = bms::api::error_body(bms::validation_error("phone", "Bad phone."));
        CHECK_EQ(with_field, json({{"code", "VALIDATION_ERROR"}, {"message", "Bad phone."},
                                   {"field", "phone"}}));
        const json without = bms::api::error_body(bms::not_found("No such account."));
        CHECK_EQ(without, json({{"code", "NOT_FOUND"}, {"message", "No such account."}}));
    });

    run("access log line never contains the body or query string", [] {
        httplib::Request req;
        req.method = "POST";
        req.path = "/auth/login";
        req.remote_addr = "127.0.0.1";
        req.body = R"({"user_id":"u1","password":")" + kSecretPassword + "\"}";
        req.params.emplace("pin", "1234");
        httplib::Response res;
        res.status = 401;
        const std::string line = bms::api::format_access_log(req, res);
        CHECK_EQ(line, std::string("127.0.0.1 \"POST /auth/login\" 401"));
    });

    // ----- Live server --------------------------------------------------------
    CapturedLog captured;
    bms::log::set_sink([&captured](bms::log::Level, std::string_view line) {
        std::lock_guard<std::mutex> lock(captured.mutex);
        captured.lines.emplace_back(line);
    });

    const fs::path frontend = make_frontend_dir();
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    config.frontend_dir = frontend.string();

    bms::api::Server server(config);
    register_test_routes(server.router());
    const int port = server.bind();
    std::thread server_thread([&server] { server.run(); });
    server.wait_until_ready();

    httplib::Client client("127.0.0.1", port);
    client.set_connection_timeout(5);
    client.set_read_timeout(5);

    run("a second server cannot bind a port that is already in use", [&] {
        bms::Config clash = config;
        clash.port = port;
        bms::api::Server second(clash);
        CHECK_THROWS_AS(second.bind(), std::runtime_error);
    });

    run("GET /health reports ok and the version", [&] {
        auto res = client.Get("/health");
        CHECK(res);
        CHECK_EQ(res->status, 200);
        CHECK_EQ(body_of(res), json({{"status", "ok"}, {"version", bms::kVersion}}));
        CHECK_EQ(res->get_header_value("Content-Type"), std::string("application/json"));
        CHECK_EQ(res->get_header_value("Cache-Control"), std::string("no-store"));
        CHECK_EQ(res->get_header_value("X-Content-Type-Options"), std::string("nosniff"));
    });

    run("static front end is served at /", [&] {
        for (const char* path : {"/", "/index.html"}) {
            auto res = client.Get(path);
            CHECK(res);
            CHECK_EQ(res->status, 200);
            CHECK(res->body.find("BMS test page") != std::string::npos);
        }
    });

    run("unknown routes return a JSON 404", [&] {
        auto get = client.Get("/no/such/route");
        CHECK(get);
        CHECK_EQ(get->status, 404);
        CHECK_EQ(body_of(get)["code"], json("NOT_FOUND"));
        CHECK(body_of(get).contains("message"));
        CHECK(!body_of(get).contains("field"));

        auto post = client.Post("/no/such/route", "{}", "application/json");
        CHECK(post);
        CHECK_EQ(post->status, 404);
        CHECK_EQ(body_of(post)["code"], json("NOT_FOUND"));
    });

    run("validation errors return 400 with the field", [&] {
        auto res = client.Get("/test/validation");
        CHECK(res);
        CHECK_EQ(res->status, 400);
        CHECK_EQ(body_of(res), json({{"code", "VALIDATION_ERROR"},
                                     {"message", "Amount must be positive."},
                                     {"field", "amount"}}));
    });

    run("business-rule errors return 422 with their own code", [&] {
        auto res = client.Get("/test/rule");
        CHECK(res);
        CHECK_EQ(res->status, 422);
        CHECK_EQ(body_of(res)["code"], json("INSUFFICIENT_BALANCE"));
        CHECK(!body_of(res).contains("field"));
    });

    run("unexpected exceptions return a generic 500 without internal details", [&] {
        auto res = client.Get("/test/crash");
        CHECK(res);
        CHECK_EQ(res->status, 500);
        CHECK_EQ(body_of(res)["code"], json("INTERNAL_ERROR"));
        CHECK(res->body.find("secret_internal_table") == std::string::npos);
    });

    run("handlers can choose the status, and read path and query parameters", [&] {
        auto created = client.Get("/test/created");
        CHECK(created);
        CHECK_EQ(created->status, 201);
        CHECK_EQ(body_of(created), json({{"id", 7}}));

        auto item = client.Get("/test/items/42?q=abc");
        CHECK(item);
        CHECK_EQ(item->status, 200);
        CHECK_EQ(body_of(item), json({{"id", "42"}, {"q", "abc"}}));
    });

    run("request bodies must be JSON objects", [&] {
        auto echo = client.Post("/test/echo", R"({"amount": 100})", "application/json");
        CHECK(echo);
        CHECK_EQ(echo->status, 200);
        CHECK_EQ(body_of(echo), json({{"amount", 100}}));

        for (const char* bad : {"{not json", "[1, 2]", ""}) {
            auto res = client.Post("/test/echo", bad, "application/json");
            CHECK(res);
            CHECK_EQ(res->status, 400);
            CHECK_EQ(body_of(res)["code"], json("INVALID_JSON"));
        }
    });

    run("oversized request bodies are rejected with a JSON 413", [&] {
        const std::string big(2 * 1024 * 1024, 'x');
        auto res = client.Post("/test/echo", big, "application/json");
        CHECK(res);
        CHECK_EQ(res->status, 413);
        CHECK_EQ(body_of(res)["code"], json("PAYLOAD_TOO_LARGE"));
    });

    const std::string login_body =
        R"({"user_id": "officer1", "password": ")" + kSecretPassword + "\"}";
    auto login = client.Post("/auth/login?pin=9876", login_body, "application/json");

    // Stop the server so every access-log line has been written.
    server.stop();
    server_thread.join();
    bms::log::set_sink({});

    run("login requests are logged without the password", [&] {
        CHECK(login);
        CHECK_EQ(login->status, 401);
        CHECK(captured.any_contains("\"POST /auth/login\" 401"));
        CHECK(!captured.any_contains(kSecretPassword));
        CHECK(!captured.any_contains("9876"));
    });

    run("internal error details go to the server log", [&] {
        CHECK(captured.any_contains(kInternalDetail));
        CHECK(captured.any_contains("\"GET /test/items/42\" 200"));
        CHECK(!captured.any_contains("q=abc"));
    });

    std::error_code ec;
    fs::remove_all(frontend, ec);
    return bms::test::finish();
}
