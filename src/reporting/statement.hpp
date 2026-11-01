#pragma once

#include <cstdint>
#include <optional>
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

struct HistoryEntry {
    std::int64_t transaction_id;
    std::string type;
    std::int64_t amount_paise;
    std::int64_t resulting_balance_paise;
    std::string date_time;
    std::optional<std::string> counterpart_account;
};

struct AccountTransactionHistory {
    std::string account_number;
    std::int64_t account_id;
    std::int64_t current_balance_paise;
    std::int64_t total;
    std::int64_t limit;
    std::int64_t offset;
    std::int64_t snapshot;
    bool access_denied;
    std::vector<HistoryEntry> transactions;
};

// Returns an account statement for inclusive YYYY-MM-DD calendar dates.
// Invalid dates, or a from date later than to, raise a validation error.
AccountStatement get_account_statement(Database& database, const std::string& account_number,
                                      const std::string& from, const std::string& to);

// Returns a newest-first page of stored transaction rows. A customer_user_id
// limits the read to accounts owned by that customer; a denied or nonexistent
// customer account is returned as access_denied so the API can audit it without
// revealing whether the account exists.
AccountTransactionHistory get_account_transaction_history(
    Database& database, const std::string& account_number,
    std::optional<std::int64_t> customer_user_id,
    const std::optional<std::string>& from, const std::optional<std::string>& to,
    const std::optional<std::string>& limit, const std::optional<std::string>& offset,
    const std::optional<std::string>& snapshot = std::nullopt);

}  // namespace bms::reporting
