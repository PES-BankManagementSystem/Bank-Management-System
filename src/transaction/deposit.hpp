#pragma once

#include <cstdint>
#include <string>

namespace bms {
class Database;
}

namespace bms::transaction {

struct DepositResult {
    std::int64_t transaction_id;
    std::int64_t account_id;
    std::int64_t operator_id;
    std::int64_t amount_paise;
    std::int64_t resulting_balance_paise;
    std::string created_at;
};

// Parses a decimal rupee amount without floating point and records a deposit
// atomically. Invalid amounts raise a VALIDATION_ERROR for the amount field.
DepositResult deposit(Database& database, const std::string& account_number,
                      const std::string& amount, std::int64_t operator_id);

std::string format_money(std::int64_t amount_paise);

}  // namespace bms::transaction
