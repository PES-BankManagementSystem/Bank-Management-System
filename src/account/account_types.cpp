#include "account/account_types.hpp"

#include <cctype>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "audit/record.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::account {
namespace {

using Json = nlohmann::json;

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::string trim(std::string value) {
    const auto is_space = [](unsigned char ch) { return std::isspace(ch) != 0; };
    auto first = value.begin();
    while (first != value.end() && is_space(static_cast<unsigned char>(*first))) ++first;
    auto last = value.end();
    while (last != first && is_space(static_cast<unsigned char>(*(last - 1)))) --last;
    return std::string(first, last);
}

std::size_t character_count(const std::string& value) {
    std::size_t count = 0;
    for (const unsigned char ch : value) {
        if ((ch & 0xc0) != 0x80) ++count;
    }
    return count;
}

AccountTypeInput validated(AccountTypeInput input) {
    Error::FieldErrors errors;
    input.name = trim(std::move(input.name));
    if (input.name.empty()) {
        errors.emplace_back("name", "Name is required.");
    } else if (character_count(input.name) > 50) {
        errors.emplace_back("name", "Name must be 50 characters or fewer.");
    }
    if (input.minimum_balance_paise < 0 ||
        input.minimum_balance_paise > kMaximumAccountTypeMoneyPaise) {
        errors.emplace_back("minimumBalance", "Minimum balance must be between INR 0.00 and INR 10,000,000.00.");
    }
    if (input.interest_rate_basis_points < 0 ||
        input.interest_rate_basis_points > kMaximumInterestRateBasisPoints) {
        errors.emplace_back("interestRate", "Interest rate must be between 0.00 and 20.00 percent per annum.");
    }
    if (input.daily_withdrawal_limit_paise < 0 ||
        input.daily_withdrawal_limit_paise > kMaximumAccountTypeMoneyPaise) {
        errors.emplace_back("dailyWithdrawalLimit",
                            "Daily withdrawal limit must be between INR 0.00 and INR 10,000,000.00.");
    }
    if (!errors.empty()) throw validation_errors(std::move(errors));
    return input;
}

void reject_duplicate_name(Database& database, const std::string& name,
                           std::int64_t except_type_id = 0) {
    auto query = database.prepare(
        "SELECT 1 FROM account_types WHERE type_name = ? COLLATE NOCASE AND type_id != ?");
    query->bind(1, name);
    query->bind(2, except_type_id);
    if (query->step()) {
        throw conflict("DUPLICATE_ACCOUNT_TYPE_NAME",
                       "An account type with this name already exists.", "name");
    }
}

AccountType load_type(Database& database, std::int64_t type_id) {
    auto query = database.prepare(
        "SELECT type_id, type_name, min_balance, interest_rate, daily_withdrawal_limit, created_at "
        "FROM account_types WHERE type_id = ?");
    query->bind(1, type_id);
    if (!query->step()) throw not_found("Account type " + std::to_string(type_id) + " was not found.");
    return {query->column_int(0), column_string(*query, 1), query->column_int(2),
            query->column_int(3), query->column_int(4), column_string(*query, 5)};
}

std::string audit_details(const AccountTypeInput& input, const AccountType* previous) {
    Json details = Json::object();
    const auto add_change = [&details](const char* field, const Json& old_value,
                                       const Json& new_value) {
        if (old_value != new_value) {
            details[field] = {{"old", old_value}, {"new", new_value}};
        }
    };
    add_change("name", previous ? Json(previous->name) : Json(nullptr), input.name);
    add_change("minimumBalancePaise",
               previous ? Json(previous->minimum_balance_paise) : Json(nullptr),
               input.minimum_balance_paise);
    add_change("interestRateBasisPoints",
               previous ? Json(previous->interest_rate_basis_points) : Json(nullptr),
               input.interest_rate_basis_points);
    add_change("dailyWithdrawalLimitPaise",
               previous ? Json(previous->daily_withdrawal_limit_paise) : Json(nullptr),
               input.daily_withdrawal_limit_paise);
    return details.dump();
}

}  // namespace

std::vector<AccountType> list_account_types(Database& database, std::int64_t operator_id) {
    auth::rbac::require_role(database, operator_id, auth::rbac::Operation::ViewAccountTypes);
    auto query = database.prepare(
        "SELECT type_id, type_name, min_balance, interest_rate, daily_withdrawal_limit, created_at "
        "FROM account_types ORDER BY type_id");
    std::vector<AccountType> types;
    while (query->step()) {
        types.push_back({query->column_int(0), column_string(*query, 1), query->column_int(2),
                         query->column_int(3), query->column_int(4), column_string(*query, 5)});
    }
    return types;
}

AccountType create_account_type(Database& database, const AccountTypeInput& raw_input,
                                std::int64_t operator_id) {
    auth::rbac::require_role(database, operator_id, auth::rbac::Operation::ManageAccountTypes);
    const AccountTypeInput input = validated(raw_input);
    Database::Transaction transaction(database, true);
    reject_duplicate_name(database, input.name);
    auto insert = database.prepare(
        "INSERT INTO account_types (type_name, min_balance, interest_rate, daily_withdrawal_limit) "
        "VALUES (?, ?, ?, ?)");
    insert->bind(1, input.name);
    insert->bind(2, input.minimum_balance_paise);
    insert->bind(3, input.interest_rate_basis_points);
    insert->bind(4, input.daily_withdrawal_limit_paise);
    insert->step();
    auto id = database.prepare("SELECT last_insert_rowid()");
    id->step();
    const std::int64_t type_id = id->column_int(0);
    audit::record(database, operator_id, "ACCOUNT_TYPE_CREATED", "account_types", type_id,
                  audit_details(input, nullptr));
    const AccountType created = load_type(database, type_id);
    transaction.commit();
    return created;
}

AccountType update_account_type(Database& database, std::int64_t type_id,
                                const AccountTypeInput& raw_input, std::int64_t operator_id) {
    auth::rbac::require_role(database, operator_id, auth::rbac::Operation::ManageAccountTypes);
    const AccountTypeInput input = validated(raw_input);
    Database::Transaction transaction(database, true);
    const AccountType previous = load_type(database, type_id);
    reject_duplicate_name(database, input.name, type_id);
    auto update = database.prepare(
        "UPDATE account_types SET type_name = ?, min_balance = ?, interest_rate = ?, "
        "daily_withdrawal_limit = ? WHERE type_id = ?");
    update->bind(1, input.name);
    update->bind(2, input.minimum_balance_paise);
    update->bind(3, input.interest_rate_basis_points);
    update->bind(4, input.daily_withdrawal_limit_paise);
    update->bind(5, type_id);
    update->step();
    audit::record(database, operator_id, "ACCOUNT_TYPE_UPDATED", "account_types", type_id,
                  audit_details(input, &previous));
    const AccountType updated = load_type(database, type_id);
    transaction.commit();
    return updated;
}

}  // namespace bms::account
