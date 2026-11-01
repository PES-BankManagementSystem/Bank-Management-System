#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bms {
class Database;
}

namespace bms::account {

struct AccountSummary {
    std::string account_number;
    std::string account_type;
    std::string status;
    std::int64_t balance_paise;
};

struct AccountBalance {
    std::string account_number;
    std::string account_type;
    std::string status;
    std::int64_t balance_paise;
    std::string as_of;
};

std::vector<AccountSummary> list_customer_accounts(Database& database,
                                                   std::int64_t user_id);
AccountBalance get_account_balance(Database& database,
                                   const std::string& account_number);

}  // namespace bms::account
