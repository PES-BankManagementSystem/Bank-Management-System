#include "transaction/withdraw.hpp"

#include <string>

#include "audit/record.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "common/input_validation.hpp"

namespace bms::transaction {

DepositResult withdraw(Database& database, const std::string& account_number,
                       const std::string& amount, std::int64_t operator_id) {
    auth::rbac::require_role(database, operator_id, auth::rbac::Operation::Withdraw);
    const std::int64_t amount_paise = parse_amount_paise(amount);
    validate_account_number(account_number, "account_number");

    // Acquire the write lock before reading the daily total so concurrent
    // withdrawals cannot both pass the same limit check.
    Database::Transaction transaction(database, true);
    std::int64_t account_id = 0;
    std::int64_t balance = 0;
    std::int64_t minimum_balance = 0;
    std::int64_t daily_limit = 0;
    std::string status;
    {
        auto account = database.prepare(
            "SELECT a.account_id, a.status, a.balance, t.min_balance, t.daily_withdrawal_limit "
            "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
            "WHERE a.account_number = ?");
        account->bind(1, account_number);
        if (!account->step()) {
            throw not_found("Account " + account_number + " was not found.");
        }
        account_id = account->column_int(0);
        const auto* status_text = account->column_text(1);
        status = status_text ? reinterpret_cast<const char*>(status_text) : "";
        balance = account->column_int(2);
        minimum_balance = account->column_int(3);
        daily_limit = account->column_int(4);
    }

    if (status != "active") {
        throw business_rule("ACCOUNT_NOT_ACTIVE", "Account " + account_number + " is " + status + ".");
    }

    if (balance < minimum_balance || amount_paise > balance - minimum_balance) {
        const std::int64_t maximum = balance > minimum_balance ? balance - minimum_balance : 0;
        throw business_rule("BELOW_MINIMUM_BALANCE",
                            "The minimum balance is ₹" + format_money(minimum_balance) +
                            "; the maximum amount that could be withdrawn is ₹" + format_money(maximum) + ".");
    }

    std::int64_t withdrawn_today = 0;
    {
        auto total = database.prepare(
            "SELECT COALESCE(SUM(amount), 0) FROM transactions "
            "WHERE account_id = ? AND type = 'withdrawal' "
            "AND created_at >= date('now') AND created_at < date('now', '+1 day')");
        total->bind(1, account_id);
        total->step();
        withdrawn_today = total->column_int(0);
    }

    const std::int64_t remaining = withdrawn_today >= daily_limit ? 0 : daily_limit - withdrawn_today;
    if (amount_paise > remaining) {
        throw business_rule("DAILY_LIMIT_EXCEEDED",
                            "The remaining withdrawal limit for today is ₹" + format_money(remaining) + ".");
    }

    const std::int64_t resulting_balance = balance - amount_paise;
    {
        auto update = database.prepare(
            "UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP "
            "WHERE account_id = ? AND status = 'active'");
        update->bind(1, resulting_balance);
        update->bind(2, account_id);
        update->step();
    }
    {
        auto insert = database.prepare(
            "INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance) "
            "VALUES (?, ?, 'withdrawal', ?, ?)");
        insert->bind(1, account_id);
        insert->bind(2, operator_id);
        insert->bind(3, amount_paise);
        insert->bind(4, resulting_balance);
        insert->step();
    }

    std::int64_t transaction_id = 0;
    {
        auto query = database.prepare("SELECT last_insert_rowid()");
        query->step();
        transaction_id = query->column_int(0);
    }
    audit::record(database, operator_id, "WITHDRAWAL", "accounts", account_id,
                  "Withdrew " + std::to_string(amount_paise) +
                      " paise; resulting balance " + std::to_string(resulting_balance) + " paise.");

    std::string created_at;
    {
        auto query = database.prepare("SELECT created_at FROM transactions WHERE transaction_id = ?");
        query->bind(1, transaction_id);
        query->step();
        const auto* timestamp = query->column_text(0);
        created_at = timestamp ? reinterpret_cast<const char*>(timestamp) : "";
    }

    transaction.commit();
    return {transaction_id, account_id, operator_id, amount_paise, resulting_balance, created_at};
}

}  // namespace bms::transaction
