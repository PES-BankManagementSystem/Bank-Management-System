#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace bms {
class Database;
}

namespace bms::account {

struct OpenAccountInput {
    std::string customer_id;
    std::int64_t account_type_id;
    std::int64_t initial_deposit_paise;
};

struct OpenAccountResult {
    std::int64_t account_id;
    std::string account_number;
    std::string account_type;
    std::string status;
    std::int64_t balance_paise;
    std::int64_t transaction_id;
    std::string transaction_reference;
    std::string transaction_created_at;
};

using AccountNumberGenerator = std::function<std::string()>;

std::string generate_account_number();
OpenAccountResult open_account(Database& database, const OpenAccountInput& input,
                               std::int64_t operator_id);
OpenAccountResult open_account(Database& database, const OpenAccountInput& input,
                               std::int64_t operator_id,
                               const AccountNumberGenerator& account_number_generator);

}  // namespace bms::account
