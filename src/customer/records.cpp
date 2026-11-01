#include "customer/records.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "audit/record.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "common/input_validation.hpp"

namespace bms::customer {
namespace {

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::string normalize_phone_query(const std::string& value) {
    std::string normalized;
    normalized.reserve(value.size());
    for (const unsigned char ch : value) {
        if (ch != '-' && std::isspace(ch) == 0) {
            normalized.push_back(static_cast<char>(ch));
        }
    }
    return normalized;
}

std::string escape_like(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        if (ch == '%' || ch == '_' || ch == '\\') escaped.push_back('\\');
        escaped.push_back(ch);
    }
    return escaped;
}

std::string like_pattern(const std::string& value, bool leading_wildcard,
                         bool trailing_wildcard) {
    return (leading_wildcard ? "%" : "") + escape_like(value) +
           (trailing_wildcard ? "%" : "");
}

CustomerDetails read_customer(Database& database, const std::string& customer_id) {
    auto customer = database.prepare(
        "SELECT c.customer_code, c.full_name, c.date_of_birth, c.address, c.phone, "
        "c.email, c.identification_reference, c.status, "
        "c.created_at, c.updated_at FROM customers c "
        "WHERE c.customer_code = ? COLLATE NOCASE");
    customer->bind(1, customer_id);
    if (!customer->step()) {
        throw not_found("Customer " + customer_id + " was not found.");
    }

    CustomerDetails details{
        column_string(*customer, 0), column_string(*customer, 1),
        column_string(*customer, 2), column_string(*customer, 3),
        column_string(*customer, 4), column_string(*customer, 5),
        column_string(*customer, 6), column_string(*customer, 7),
        column_string(*customer, 8), column_string(*customer, 9), {}};

    auto accounts = database.prepare(
        "SELECT a.account_number, t.type_name, a.status, a.balance "
        "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
        "JOIN customers c ON c.customer_id = a.customer_id "
        "WHERE c.customer_code = ? COLLATE NOCASE ORDER BY a.account_id");
    accounts->bind(1, customer_id);
    while (accounts->step()) {
        details.accounts.push_back({
            column_string(*accounts, 0), column_string(*accounts, 1),
            column_string(*accounts, 2), accounts->column_int(3)});
    }
    return details;
}

std::int64_t find_customer_key(Database& database, const std::string& customer_id) {
    auto customer = database.prepare(
        "SELECT customer_id FROM customers WHERE customer_code = ? COLLATE NOCASE");
    customer->bind(1, customer_id);
    if (!customer->step()) {
        throw not_found("Customer " + customer_id + " was not found.");
    }
    return customer->column_int(0);
}

void reject_duplicate(Database& database, const char* sql, const std::string& value,
                      std::int64_t customer_key, const std::string& field,
                      const std::string& message) {
    auto query = database.prepare(sql);
    query->bind(1, value);
    query->bind(2, customer_key);
    if (query->step()) throw conflict("DUPLICATE_CUSTOMER_FIELD", message + ".", field);
}

void reject_duplicate_fields(Database& database, const CustomerInput& input,
                             std::int64_t customer_key) {
    reject_duplicate(database,
                     "SELECT 1 FROM customers WHERE email = ? AND customer_id <> ?",
                     input.email, customer_key, "email",
                     "Email address is already registered");
    reject_duplicate(database,
                     "SELECT 1 FROM customers WHERE phone = ? AND customer_id <> ?",
                     input.phone, customer_key, "phone",
                     "Phone number is already registered");
    reject_duplicate(database,
                     "SELECT 1 FROM customers WHERE identification_reference = ? "
                     "AND customer_id <> ?",
                     input.identification_reference, customer_key,
                     "identificationReference",
                     "Identification reference is already registered");
}

std::vector<std::string> changed_fields(const CustomerDetails& before,
                                        const CustomerInput& after) {
    std::vector<std::string> changed;
    if (before.name != after.name) changed.emplace_back("name");
    if (before.date_of_birth != after.date_of_birth) changed.emplace_back("dateOfBirth");
    if (before.address != after.address) changed.emplace_back("address");
    if (before.phone != after.phone) changed.emplace_back("phone");
    if (before.email != after.email) changed.emplace_back("email");
    if (before.identification_reference != after.identification_reference) {
        changed.emplace_back("identificationReference");
    }
    return changed;
}

std::string audit_details(const std::string& customer_id,
                          const std::vector<std::string>& changed) {
    std::string details = "customerId=" + customer_id + ";fields=";
    for (std::size_t index = 0; index < changed.size(); ++index) {
        if (index != 0) details.push_back(',');
        details += changed[index];
    }
    return details;
}

}  // namespace

CustomerPage search_customers(Database& database, const std::string& query,
                              std::int64_t limit, std::int64_t offset) {
    validate_maximum_length(query, "query", 100);
    const std::string trimmed = [&query] {
        const auto first = std::find_if_not(query.begin(), query.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        });
        const auto last = std::find_if_not(query.rbegin(), query.rend(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }).base();
        return first < last ? std::string(first, last) : std::string{};
    }();
    const std::string code_pattern = like_pattern(trimmed, false, true);
    const std::string name_pattern = like_pattern(trimmed, true, true);
    const std::string normalized_phone = normalize_phone_query(trimmed);
    const std::string phone_pattern = like_pattern(normalized_phone, true, true);
    const std::string where =
        " WHERE (? = '' OR lower(c.customer_code) LIKE lower(?) ESCAPE '\\' "
        "OR c.full_name LIKE ? ESCAPE '\\' COLLATE NOCASE "
        "OR (? <> '' AND replace(replace(replace(replace(replace(c.phone, '-', ''), "
        "' ', ''), char(9), ''), char(10), ''), char(13), '') LIKE ? ESCAPE '\\'))";

    CustomerPage page;
    auto count = database.prepare(
        "SELECT COUNT(*) FROM customers c" + where);
    count->bind(1, trimmed);
    count->bind(2, code_pattern);
    count->bind(3, name_pattern);
    count->bind(4, normalized_phone);
    count->bind(5, phone_pattern);
    if (count->step()) page.total = count->column_int(0);

    auto results = database.prepare(
        "SELECT c.customer_code, c.full_name, c.phone, c.email, "
        "c.status FROM customers c" + where +
        " ORDER BY c.full_name COLLATE NOCASE, c.customer_code COLLATE NOCASE "
        "LIMIT ? OFFSET ?");
    results->bind(1, trimmed);
    results->bind(2, code_pattern);
    results->bind(3, name_pattern);
    results->bind(4, normalized_phone);
    results->bind(5, phone_pattern);
    results->bind(6, limit);
    results->bind(7, offset);
    while (results->step()) {
        page.items.push_back({column_string(*results, 0), column_string(*results, 1),
                              column_string(*results, 2), column_string(*results, 3),
                              column_string(*results, 4)});
    }
    return page;
}

CustomerDetails get_customer(Database& database, const std::string& customer_id) {
    return read_customer(database, customer_id);
}

CustomerDetails get_customer_for_user(Database& database, std::int64_t user_id) {
    auto customer = database.prepare(
        "SELECT customer_code FROM customers WHERE user_id = ?");
    customer->bind(1, user_id);
    if (!customer->step()) throw not_found("Your customer profile was not found.");
    const std::string customer_id = column_string(*customer, 0);
    return read_customer(database, customer_id);
}

CustomerDetails update_customer(Database& database, const std::string& customer_id,
                                const CustomerInput& raw_input,
                                std::int64_t operator_user_id) {
    auth::rbac::require_role(database, operator_user_id,
                             auth::rbac::Operation::UpdateCustomer);
    const CustomerInput input = validate_customer_input(raw_input);
    Database::Transaction transaction(database, true);
    const std::int64_t customer_key = find_customer_key(database, customer_id);
    const CustomerDetails before = read_customer(database, customer_id);
    reject_duplicate_fields(database, input, customer_key);

    const std::vector<std::string> changed = changed_fields(before, input);
    auto update = database.prepare(
        "UPDATE customers SET full_name = ?, date_of_birth = ?, address = ?, phone = ?, "
        "email = ?, identification_reference = ?, updated_at = CURRENT_TIMESTAMP "
        "WHERE customer_id = ?");
    update->bind(1, input.name);
    update->bind(2, input.date_of_birth);
    update->bind(3, input.address);
    update->bind(4, input.phone);
    update->bind(5, input.email);
    update->bind(6, input.identification_reference);
    update->bind(7, customer_key);
    update->step();

    audit::record(database, operator_user_id, "CUSTOMER_UPDATED", "customers",
                  customer_key, audit_details(before.customer_id, changed));

    CustomerDetails updated = read_customer(database, customer_id);
    transaction.commit();
    return updated;
}

CustomerDetails deactivate_customer(Database& database, const std::string& customer_id,
                                    std::int64_t operator_user_id) {
    auth::rbac::require_role(database, operator_user_id,
                             auth::rbac::Operation::DeactivateCustomer);
    Database::Transaction transaction(database, true);
    const std::int64_t customer_key = find_customer_key(database, customer_id);

    auto current = database.prepare(
        "SELECT c.customer_code, c.status, c.user_id, u.status FROM customers c "
        "LEFT JOIN users u ON u.user_id = c.user_id WHERE c.customer_id = ?");
    current->bind(1, customer_key);
    if (!current->step()) {
        throw not_found("Customer " + customer_id + " was not found.");
    }
    const std::string canonical_customer_id = column_string(*current, 0);
    const std::string old_status = column_string(*current, 1);
    const bool has_user = !current->column_null(2);
    const std::int64_t user_id = has_user ? current->column_int(2) : 0;
    const std::string user_status = column_string(*current, 3);
    const bool changed = old_status != "inactive" ||
                         (has_user && user_status != "inactive");
    if (!changed) {
        transaction.commit();
        return read_customer(database, customer_id);
    }

    auto deactivate_record = database.prepare(
        "UPDATE customers SET status = 'inactive', updated_at = CURRENT_TIMESTAMP "
        "WHERE customer_id = ?");
    deactivate_record->bind(1, customer_key);
    deactivate_record->step();
    if (has_user && user_status != "inactive") {
        auto deactivate_login = database.prepare(
            "UPDATE users SET status = 'inactive', updated_at = CURRENT_TIMESTAMP "
            "WHERE user_id = ?");
        deactivate_login->bind(1, user_id);
        deactivate_login->step();
    }
    audit::record(database, operator_user_id, "CUSTOMER_DEACTIVATED", "customers",
                  customer_key, "customerId=" + canonical_customer_id +
                                    ";oldStatus=" + old_status + ";newStatus=inactive");

    CustomerDetails updated = read_customer(database, customer_id);
    transaction.commit();
    return updated;
}

void require_active_customer(Database& database, const std::string& customer_id) {
    auto query = database.prepare(
        "SELECT status FROM customers WHERE customer_code = ? COLLATE NOCASE");
    query->bind(1, customer_id);
    if (!query->step()) {
        throw not_found("Customer " + customer_id + " was not found.");
    }
    if (column_string(*query, 0) != "active") {
        throw conflict("CUSTOMER_INACTIVE",
                       "Customer " + customer_id +
                           " has status inactive and cannot open a new account.");
    }
}

}  // namespace bms::customer
