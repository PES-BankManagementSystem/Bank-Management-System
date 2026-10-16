#pragma once

#include <string>

namespace httplib {
struct Request;
struct Response;
}  // namespace httplib

namespace bms::api {

// One access-log line per request, e.g.
//   127.0.0.1 "POST /auth/login" 401
//
// Request bodies, query strings and headers are never logged on any route:
// bodies on /auth carry passwords and PINs (NFR-4, SEC-2), headers carry
// session tokens, and query strings carry search terms such as customer
// names and phone numbers.
std::string format_access_log(const httplib::Request& req, const httplib::Response& res);

}  // namespace bms::api
