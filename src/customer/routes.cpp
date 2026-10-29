#include "customer/routes.hpp"

#include <charconv>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>

#include "api/router.hpp"
#include "auth/rbac.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "customer/registration.hpp"
#include "customer/records.hpp"
#include "customer/validation.hpp"

namespace bms::customer {
namespace {

constexpr char kCustomerDataHttpMessage[] =
    "Customer data services are disabled on network-accessible HTTP listeners. Enable HTTPS.";

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

CustomerInput read_customer_input(const api::Json& body) {
    const std::set<std::string> editable_fields = {
        "name", "dateOfBirth", "address", "phone", "identificationReference", "email"};
    const std::set<std::string> immutable_fields = {
        "customerId", "customer_code", "user_id", "createdAt"};
    Error::FieldErrors errors;
    for (auto it = body.begin(); it != body.end(); ++it) {
        if (immutable_fields.find(it.key()) != immutable_fields.end()) {
            errors.emplace_back(it.key(), "This field cannot be changed.");
        } else if (editable_fields.find(it.key()) == editable_fields.end()) {
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
    return validate_customer_input(std::move(input), std::move(errors));
}

std::int64_t pagination_value(const api::Request& request, const char* name,
                              std::int64_t default_value, std::int64_t maximum) {
    const auto value = request.query(name);
    if (!value) return default_value;
    std::int64_t parsed = 0;
    const char* first = value->data();
    const char* last = first + value->size();
    const auto result = std::from_chars(first, last, parsed);
    if (value->empty() || result.ec != std::errc{} || result.ptr != last ||
        parsed < 0 || parsed > maximum || (std::string(name) == "limit" && parsed == 0)) {
        const std::string expectation = std::string(name) == "limit"
            ? "Limit must be an integer from 1 to 100."
            : "Offset must be a non-negative integer.";
        throw validation_error(name, expectation);
    }
    return parsed;
}

std::string money_string(std::int64_t paise) {
    const std::int64_t rupees = paise / 100;
    const std::int64_t remainder = paise % 100;
    return std::to_string(rupees) + "." +
           (remainder < 10 ? "0" : "") + std::to_string(remainder);
}

api::Json customer_details_json(const CustomerDetails& customer) {
    api::Json accounts = api::Json::array();
    for (const CustomerAccount& account : customer.accounts) {
        accounts.push_back({
            {"accountNumber", account.account_number},
            {"type", account.type},
            {"status", account.status},
            {"balance", money_string(account.balance_paise)},
        });
    }
    return {
        {"customerId", customer.customer_id},
        {"name", customer.name},
        {"dateOfBirth", customer.date_of_birth},
        {"address", customer.address},
        {"phone", customer.phone},
        {"email", customer.email},
        {"identificationReference", customer.identification_reference},
        {"status", customer.status},
        {"createdAt", customer.created_at},
        {"updatedAt", customer.updated_at},
        {"accounts", std::move(accounts)},
    };
}

std::string lowercase(std::string value) {
    for (char& character : value) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    return value;
}

void require_loopback_data_access(bool allow_initial_credential_delivery) {
    if (!allow_initial_credential_delivery) throw forbidden(kCustomerDataHttpMessage);
}

void require_staff_role(const auth::Principal& principal) {
    if (principal.role != "administrator" && principal.role != "bank_officer") {
        throw forbidden();
    }
}

}  // namespace

void register_routes(api::Router& router, Database& database,
                     bool allow_initial_credential_delivery) {
    router.get("/customers", [&database, allow_initial_credential_delivery](
                                 const api::Request& request) {
        require_loopback_data_access(allow_initial_credential_delivery);
        const auth::Principal principal = auth::rbac::require(
            request, database, auth::rbac::Operation::ViewCustomer);
        require_staff_role(principal);

        const std::string query = request.query("query").value_or("");
        const std::int64_t limit = pagination_value(request, "limit", 20, 100);
        const std::int64_t offset =
            pagination_value(request, "offset", 0, std::numeric_limits<std::int64_t>::max());
        const CustomerPage page = search_customers(database, query, limit, offset);
        api::Json items = api::Json::array();
        for (const CustomerSummary& customer : page.items) {
            items.push_back({
                {"customerId", customer.customer_id},
                {"name", customer.name},
                {"phone", customer.phone},
                {"email", customer.email},
                {"status", customer.status},
            });
        }
        return api::Response::ok({
            {"items", std::move(items)},
            {"total", page.total},
            {"limit", limit},
            {"offset", offset},
        });
    });

    router.get("/customers/me", [&database, allow_initial_credential_delivery](
                                   const api::Request& request) {
        require_loopback_data_access(allow_initial_credential_delivery);
        const auth::Principal principal = auth::rbac::require(
            request, database, auth::rbac::Operation::ViewCustomer);
        if (principal.role != "customer") throw forbidden();
        return api::Response::ok(customer_details_json(
            get_customer_for_user(database, principal.user_id)));
    });

    router.get("/customers/:id", [&database, allow_initial_credential_delivery](
                                     const api::Request& request) {
        require_loopback_data_access(allow_initial_credential_delivery);
        const auth::Principal principal = auth::rbac::require(
            request, database, auth::rbac::Operation::ViewCustomer);
        const std::string customer_id = request.param("id");
        if (principal.role == "customer") {
            const CustomerDetails own =
                get_customer_for_user(database, principal.user_id);
            if (lowercase(own.customer_id) != lowercase(customer_id)) {
                throw not_found("Customer " + customer_id + " was not found.");
            }
            return api::Response::ok(customer_details_json(own));
        }
        require_staff_role(principal);
        return api::Response::ok(customer_details_json(
            get_customer(database, customer_id)));
    });

    router.put("/customers/:id", [&database, allow_initial_credential_delivery](
                                     const api::Request& request) {
        require_loopback_data_access(allow_initial_credential_delivery);
        const auth::Principal principal = auth::authenticate(request, database);
        const api::Json& body = request.json();
        if (body.is_object() && body.contains("status")) {
            auth::rbac::require_permission(
                request, database, principal, auth::rbac::Operation::DeactivateCustomer);
            if (body.size() != 1) {
                throw validation_error(
                    "status", "Submit only the status field to deactivate a customer.");
            }
            if (!body["status"].is_string()) {
                throw validation_error("status", "Status must be a string.");
            }
            if (body["status"].get<std::string>() != "inactive") {
                throw validation_error("status", "Status must be 'inactive'.");
            }
            return api::Response::ok(customer_details_json(
                deactivate_customer(database, request.param("id"), principal.user_id)));
        }

        auth::rbac::require_permission(
            request, database, principal, auth::rbac::Operation::UpdateCustomer);
        require_staff_role(principal);
        const CustomerInput input = read_customer_input(request.json());
        return api::Response::ok(customer_details_json(
            update_customer(database, request.param("id"), input, principal.user_id)));
    });

    router.post("/customers", [&database, allow_initial_credential_delivery](
                                  const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::RegisterCustomer);
        if (!allow_initial_credential_delivery) {
            throw forbidden(
                "Customer registration with one-time credential delivery is disabled on "
                "network-accessible HTTP listeners. Enable HTTPS before using this route.");
        }

        const CustomerInput normalized = read_customer_input(request.json());
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
