#include "api/errors.hpp"

#include <httplib.h>

namespace bms::api {

int http_status(ErrorKind kind) {
    switch (kind) {
        case ErrorKind::Validation: return 400;
        case ErrorKind::Unauthenticated: return 401;
        case ErrorKind::Forbidden: return 403;
        case ErrorKind::NotFound: return 404;
        case ErrorKind::Conflict: return 409;
        case ErrorKind::BusinessRule: return 422;
    }
    return 500;
}

nlohmann::json error_body(const Error& error) {
    nlohmann::json body = {{"code", error.code()}, {"message", error.what()}};
    if (error.field()) body["field"] = *error.field();
    return body;
}

void write_json(httplib::Response& res, int status, const nlohmann::json& body) {
    res.status = status;
    // API responses can contain balances and personal details: never cache them.
    res.set_header("Cache-Control", "no-store");
    if (body.is_null()) {
        res.body.clear();
        return;
    }
    res.set_content(body.dump(), "application/json");
}

void write_error(httplib::Response& res, const Error& error) {
    write_json(res, http_status(error.kind()), error_body(error));
}

void write_internal_error(httplib::Response& res) {
    write_json(res, 500,
               {{"code", "INTERNAL_ERROR"},
                {"message", "Something went wrong on our side. Please try again, or contact "
                            "support if the problem continues."}});
}

}  // namespace bms::api
