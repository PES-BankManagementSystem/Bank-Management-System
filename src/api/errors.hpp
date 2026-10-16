#pragma once

#include <nlohmann/json.hpp>

#include "common/error.hpp"

namespace httplib {
struct Response;
}

// The one JSON error format used by every endpoint (SADS 3.3):
//
//   HTTP/1.1 400 Bad Request
//   { "code": "VALIDATION_ERROR", "message": "Amount must be positive.", "field": "amount" }
//
// "field" is present only when the error is about a specific input field.
namespace bms::api {

int http_status(ErrorKind kind);

nlohmann::json error_body(const Error& error);

void write_json(httplib::Response& res, int status, const nlohmann::json& body);
void write_error(httplib::Response& res, const Error& error);

// Generic 500 for unexpected failures. Never includes exception text, file
// names or SQL, which go to the server log instead.
void write_internal_error(httplib::Response& res);

}  // namespace bms::api
