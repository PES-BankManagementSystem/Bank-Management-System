#include "api/router.hpp"

#include <exception>
#include <utility>

#include <httplib.h>

#include "api/errors.hpp"
#include "common/error.hpp"
#include "common/log.hpp"

namespace bms::api {

Request::Request(const httplib::Request& raw) : raw_(raw) {}

const std::string& Request::method() const { return raw_.method; }

const std::string& Request::path() const { return raw_.path; }

const std::string& Request::param(const std::string& name) const {
    return raw_.path_params.at(name);
}

std::optional<std::string> Request::query(const std::string& name) const {
    if (!raw_.has_param(name)) return std::nullopt;
    return raw_.get_param_value(name);
}

std::optional<std::string> Request::header(const std::string& name) const {
    if (!raw_.has_header(name)) return std::nullopt;
    return raw_.get_header_value(name);
}

const Json& Request::json() const {
    if (!json_) {
        Json parsed = Json::parse(raw_.body, nullptr, /*allow_exceptions=*/false);
        if (parsed.is_discarded() || !parsed.is_object()) {
            throw Error(ErrorKind::Validation, "INVALID_JSON",
                        "The request body must be a JSON object.");
        }
        json_ = std::move(parsed);
    }
    return *json_;
}

namespace {

httplib::Server::Handler wrap(Handler handler) {
    return [handler = std::move(handler)](const httplib::Request& raw, httplib::Response& res) {
        try {
            Response response = handler(Request(raw));
            write_json(res, response.status, response.body);
        } catch (const Error& e) {
            write_error(res, e);
        } catch (const std::exception& e) {
            log::error(raw.method + " " + raw.path + " failed: " + e.what());
            write_internal_error(res);
        } catch (...) {
            log::error(raw.method + " " + raw.path + " failed with an unknown exception");
            write_internal_error(res);
        }
    };
}

}  // namespace

Router::Router(httplib::Server& server) : server_(server) {}

void Router::get(const std::string& pattern, Handler handler) {
    server_.Get(pattern, wrap(std::move(handler)));
}

void Router::post(const std::string& pattern, Handler handler) {
    server_.Post(pattern, wrap(std::move(handler)));
}

void Router::put(const std::string& pattern, Handler handler) {
    server_.Put(pattern, wrap(std::move(handler)));
}

void Router::patch(const std::string& pattern, Handler handler) {
    server_.Patch(pattern, wrap(std::move(handler)));
}

void Router::del(const std::string& pattern, Handler handler) {
    server_.Delete(pattern, wrap(std::move(handler)));
}

}  // namespace bms::api
