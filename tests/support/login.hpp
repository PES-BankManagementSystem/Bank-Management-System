#pragma once

#include <stdexcept>
#include <string>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace bms::test {

// Signs in a seeded user (all share this password) through POST /auth/login and
// returns the Authorization header for later requests. The server under test
// must have auth::register_routes() registered.
inline httplib::Headers sign_in(httplib::Client& client, const std::string& username) {
    const auto response = client.Post(
        "/auth/login",
        nlohmann::json({{"username", username}, {"password", "Password123!"}}).dump(),
        "application/json");
    if (!response || response->status != 200) {
        throw std::runtime_error("Test sign-in failed for " + username);
    }
    return {{"Authorization",
             "Bearer " + nlohmann::json::parse(response->body)["accessToken"].get<std::string>()}};
}

}  // namespace bms::test
