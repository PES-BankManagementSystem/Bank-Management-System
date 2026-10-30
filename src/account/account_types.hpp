#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bms {
class Database;
}

namespace bms::account {

struct AccountType {
    std::int64_t type_id;
    std::string name;
    std::int64_t minimum_balance_paise;
    std::int64_t interest_rate_basis_points;
    std::int64_t daily_withdrawal_limit_paise;
    std::string created_at;
};

struct AccountTypeInput {
    std::string name;
    std::int64_t minimum_balance_paise;
    std::int64_t interest_rate_basis_points;
    std::int64_t daily_withdrawal_limit_paise;
};

constexpr std::int64_t kMaximumAccountTypeMoneyPaise = 1000000000;
constexpr std::int64_t kMaximumInterestRateBasisPoints = 2000;

std::vector<AccountType> list_account_types(Database& database, std::int64_t operator_id);
AccountType create_account_type(Database& database, const AccountTypeInput& input,
                                std::int64_t operator_id);
AccountType update_account_type(Database& database, std::int64_t type_id,
                                const AccountTypeInput& input, std::int64_t operator_id);

}  // namespace bms::account
