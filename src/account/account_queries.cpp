#include "account/account_queries.hpp"

#include <string>
#include <vector>

#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::account {
namespace {

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

}  // namespace

std::vector<AccountSummary> list_customer_accounts(Database& database,
                                                   std::int64_t user_id) {
    std::int64_t customer_id = 0;
    {
        auto customer = database.prepare(
            "SELECT customer_id FROM customers WHERE user_id = ?");
        customer->bind(1, user_id);
        if (!customer->step()) {
            throw not_found("Your customer profile was not found.");
        }
        customer_id = customer->column_int(0);
    }

    auto query = database.prepare(
        "SELECT a.account_number, t.type_name, a.status, a.balance "
        "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
        "WHERE a.customer_id = ? ORDER BY a.account_number COLLATE NOCASE");
    query->bind(1, customer_id);
    std::vector<AccountSummary> accounts;
    while (query->step()) {
        accounts.push_back({column_string(*query, 0), column_string(*query, 1),
                            column_string(*query, 2), query->column_int(3)});
    }
    return accounts;
}

AccountBalance get_account_balance(Database& database,
                                   const std::string& account_number) {
    auto query = database.prepare(
        "SELECT a.account_number, t.type_name, a.status, a.balance, CURRENT_TIMESTAMP "
        "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
        "WHERE a.account_number = ?");
    query->bind(1, account_number);
    if (!query->step()) {
        throw not_found("Account " + account_number + " was not found.");
    }
    return {column_string(*query, 0), column_string(*query, 1),
            column_string(*query, 2), query->column_int(3),
            column_string(*query, 4)};
}

}  // namespace bms::account
