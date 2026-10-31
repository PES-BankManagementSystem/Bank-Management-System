#pragma once

#include <cstdint>
#include <string>

namespace bms {
class Database;
}

namespace bms::account {

struct AccountStatus {
    std::string account_number;
    std::string type;
    std::string status;
    std::int64_t balance_paise;
};

std::string transition_status(const std::string& current_status,
                              const std::string& action,
                              const std::string& account_number);

void require_active_status(const std::string& status,
                           const std::string& account_number,
                           const std::string& account_role = {});

AccountStatus change_status(Database& database, const std::string& account_number,
                            const std::string& action, std::int64_t operator_id);

}  // namespace bms::account
