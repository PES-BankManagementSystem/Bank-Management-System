#include "account/account_status.hpp"

#include "audit/record.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::account {
namespace {

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::string money_string(std::int64_t paise) {
    return std::to_string(paise / 100) + "." +
           (paise % 100 < 10 ? "0" : "") + std::to_string(paise % 100);
}

}  // namespace

std::string transition_status(const std::string& current_status,
                              const std::string& action,
                              const std::string& account_number) {
    if (action != "freeze" && action != "unfreeze" && action != "close") {
        throw validation_error("action", "Action must be freeze, unfreeze, or close.");
    }

    if ((current_status == "active" && action == "unfreeze") ||
        (current_status == "frozen" && action == "freeze") ||
        (current_status == "closed" && action == "close")) {
        return current_status;
    }
    if (current_status == "active" && action == "freeze") return "frozen";
    if (current_status == "frozen" && action == "unfreeze") return "active";
    if ((current_status == "active" || current_status == "frozen") && action == "close") {
        return "closed";
    }

    throw conflict("ACCOUNT_STATUS_CONFLICT",
                   "Cannot " + action + " account " + account_number +
                       " while its status is " + current_status + ".");
}

void require_active_status(const std::string& status,
                           const std::string& account_number,
                           const std::string& account_role) {
    if (status != "active") {
        const std::string label =
            account_role.empty() ? "Account " : account_role + " account ";
        throw business_rule("ACCOUNT_NOT_ACTIVE",
                            label + account_number + " is " + status +
                                " and cannot be used for this transaction.");
    }
}

AccountStatus change_status(Database& database, const std::string& account_number,
                            const std::string& action, std::int64_t operator_id) {
    if (action != "freeze" && action != "unfreeze" && action != "close") {
        throw validation_error("action", "Action must be freeze, unfreeze, or close.");
    }
    auth::rbac::require_role(database, operator_id,
                             action == "close" ? auth::rbac::Operation::CloseAccount
                                               : auth::rbac::Operation::FreezeAccount);

    Database::Transaction transaction(database, true);
    AccountStatus account;
    std::int64_t account_id = 0;
    {
        auto query = database.prepare(
            "SELECT a.account_id, a.account_number, t.type_name, a.status, a.balance "
            "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
            "WHERE a.account_number = ?");
        query->bind(1, account_number);
        if (!query->step()) throw not_found("Account " + account_number + " was not found.");
        account_id = query->column_int(0);
        account.account_number = column_string(*query, 1);
        account.type = column_string(*query, 2);
        account.status = column_string(*query, 3);
        account.balance_paise = query->column_int(4);
    }

    const std::string old_status = account.status;
    const std::string new_status = transition_status(old_status, action, account_number);
    if (action == "close" && old_status != "closed" && account.balance_paise != 0) {
        throw conflict("ACCOUNT_BALANCE_NONZERO",
                       "Account " + account_number + " has a balance of INR " +
                           money_string(account.balance_paise) +
                           ". Withdraw or transfer the remaining balance before closing it.");
    }

    if (new_status != old_status) {
        auto update = database.prepare(
            "UPDATE accounts SET status = ?, updated_at = CURRENT_TIMESTAMP "
            "WHERE account_id = ? AND status = ? AND (? <> 'closed' OR balance = 0)");
        update->bind(1, new_status);
        update->bind(2, account_id);
        update->bind(3, old_status);
        update->bind(4, new_status);
        update->step();
        audit::record(database, operator_id, "ACCOUNT_STATUS_CHANGED", "accounts", account_id,
                      "accountNumber=" + account_number + ";oldStatus=" + old_status +
                          ";newStatus=" + new_status);
        account.status = new_status;
    }

    transaction.commit();
    return account;
}

}  // namespace bms::account
