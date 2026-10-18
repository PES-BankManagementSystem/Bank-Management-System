#pragma once

#include "transaction/deposit.hpp"

namespace bms::transaction {

// Withdraws an amount in rupees from an active account, atomically enforcing
// the account type's minimum balance and daily withdrawal limit.
DepositResult withdraw(Database& database, const std::string& account_number,
                       const std::string& amount, std::int64_t operator_id);

}  // namespace bms::transaction
