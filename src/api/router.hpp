#pragma once

#include <functional>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace httplib {
class Server;
struct Request;
}  // namespace httplib

namespace bms::api {

using Json = nlohmann::json;

// Read-only view of an incoming request, handed to every route handler.
class Request {
public:
    explicit Request(const httplib::Request& raw);

    const std::string& method() const;
    const std::string& path() const;

    // Value of a ":name" segment in the route pattern, e.g. param("id") for
    // "/customers/:id". Asking for a name the pattern does not have is a
    // programming error and throws std::out_of_range (reported as a 500).
    const std::string& param(const std::string& name) const;

    // Query-string value, e.g. query("from") for "?from=2026-10-01".
    std::optional<std::string> query(const std::string& name) const;

    std::optional<std::string> header(const std::string& name) const;

    // The body parsed as a JSON object. Throws a validation bms::Error with
    // code INVALID_JSON (HTTP 400) if the body is missing, malformed or not an
    // object. Parsed once and cached.
    const Json& json() const;

    const httplib::Request& raw() const { return raw_; }

private:
    const httplib::Request& raw_;
    mutable std::optional<Json> json_;
};

struct Response {
    int status = 200;
    Json body = Json::object();

    static Response ok(Json body) { return {200, std::move(body)}; }
    static Response created(Json body) { return {201, std::move(body)}; }
    static Response no_content() { return {204, nullptr}; }
};

// A route handler returns a Response, or throws bms::Error for an expected
// failure. Any other exception becomes a generic 500 with no internal details.
using Handler = std::function<Response(const Request&)>;

// Lets each module register its own endpoints, e.g. in account/routes.cpp:
//
//   void register_routes(api::Router& router) {
//       router.post("/accounts", open_account);
//       router.get("/accounts/:acc/balance", get_balance);
//   }
//
// Every handler is wrapped so errors always use the shared JSON format.
class Router {
public:
    explicit Router(httplib::Server& server);

    void get(const std::string& pattern, Handler handler);
    void post(const std::string& pattern, Handler handler);
    void put(const std::string& pattern, Handler handler);
    void patch(const std::string& pattern, Handler handler);
    void del(const std::string& pattern, Handler handler);

private:
    httplib::Server& server_;
};

}  // namespace bms::api
