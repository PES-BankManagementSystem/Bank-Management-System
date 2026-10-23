#include "customer/routes.hpp"

#include <set>
#include <string>
#include <utility>

#include "api/router.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "customer/registration.hpp"
#include "customer/validation.hpp"

namespace bms::customer {
namespace {

std::string json_string(const api::Json& body, const char* key,
                        Error::FieldErrors& errors) {
    if (!body.contains(key)) {
        errors.emplace_back(key, "This field is required.");
        return {};
    }
    if (!body[key].is_string()) {
        errors.emplace_back(key, "This field must be a string.");
        return {};
    }
    return body[key].get<std::string>();
}

}  // namespace

void register_routes(api::Router& router, Database& database,
                     bool allow_initial_credential_delivery) {
    router.post("/customers", [&database, allow_initial_credential_delivery](
                                  const api::Request& request) {
        const auth::Principal principal = auth::authenticate(request, database);
        if (principal.role != "administrator" && principal.role != "bank_officer") {
            throw forbidden();
        }
        if (!allow_initial_credential_delivery) {
            throw forbidden(
                "Customer registration with one-time credential delivery is disabled on "
                "network-accessible HTTP listeners. Enable HTTPS before using this route.");
        }

        const auto& body = request.json();
        const std::set<std::string> allowed_fields = {
            "name", "dateOfBirth", "address", "phone", "identificationReference", "email"};
        Error::FieldErrors errors;
        for (auto it = body.begin(); it != body.end(); ++it) {
            if (allowed_fields.find(it.key()) == allowed_fields.end()) {
                errors.emplace_back(it.key(), "Unknown field.");
            }
        }

        CustomerInput input{
            json_string(body, "name", errors),
            json_string(body, "dateOfBirth", errors),
            json_string(body, "address", errors),
            json_string(body, "phone", errors),
            json_string(body, "identificationReference", errors),
            json_string(body, "email", errors),
        };
        const CustomerInput normalized = validate_customer_input(std::move(input), std::move(errors));
        const RegistrationResult result =
            register_customer(database, normalized, principal.user_id);

        api::Response response = api::Response::created({
            {"customerId", result.customer_code},
            {"name", result.name},
            {"dateOfBirth", result.date_of_birth},
            {"address", result.address},
            {"phone", result.phone},
            {"identificationReference", result.identification_reference},
            {"email", result.email},
            {"createdAt", result.created_at},
            {"login", {{"username", result.username},
                       {"temporaryPassword", result.temporary_password}}},
        });
        response.headers.emplace("Location", "/customers/" + result.customer_code);
        return response;
    });
}

}  // namespace bms::customer
