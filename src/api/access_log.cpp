#include "api/access_log.hpp"

#include <httplib.h>

namespace bms::api {

std::string format_access_log(const httplib::Request& req, const httplib::Response& res) {
    return req.remote_addr + " \"" + req.method + ' ' + req.path + "\" " +
           std::to_string(res.status);
}

}  // namespace bms::api
