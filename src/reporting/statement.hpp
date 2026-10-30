#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bms {
class Database;
}

namespace bms::reporting {

struct StatementEntry {
    std::int64_t transaction_id;
    std::string type;
    // Signed for statement arithmetic: debits are negative, credits positive.
    std::int64_t amount_paise;
    std::int64_t resulting_balance_paise;
    std::string created_at;
};

struct AccountStatement {
    std::string account_number;
    std::string from;
    std::string to;
    std::int64_t opening_balance_paise;
    std::int64_t closing_balance_paise;
    std::vector<StatementEntry> transactions;
};

// Returns an account statement for inclusive YYYY-MM-DD calendar dates.
// Invalid dates, or a from date later than to, raise a validation error.
AccountStatement get_account_statement(Database& database, const std::string& account_number,
                                      const std::string& from, const std::string& to);

}  // namespace bms::reporting
