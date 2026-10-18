#include "transaction/deposit.hpp"

#include <limits>
#include <string>

#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::transaction {
namespace {

std::int64_t parse_amount(const std::string& amount) {
    if (amount.empty()) {
        throw validation_error("amount", "Amount is required.");
    }

    const std::size_t decimal = amount.find('.');
    if (decimal != std::string::npos && amount.find('.', decimal + 1) != std::string::npos) {
        throw validation_error("amount", "Amount must be a number with at most 2 decimal places.");
    }

    const std::string whole = decimal == std::string::npos ? amount : amount.substr(0, decimal);
    const std::string fraction = decimal == std::string::npos ? "" : amount.substr(decimal + 1);
    if (whole.empty() || (decimal != std::string::npos && fraction.empty()) || fraction.size() > 2) {
        throw validation_error("amount", "Amount must be a number with at most 2 decimal places.");
    }

    const std::int64_t maximum_rupees = std::numeric_limits<std::int64_t>::max() / 100;
    std::int64_t rupees = 0;
    for (const char digit : whole) {
        if (digit < '0' || digit > '9') {
            throw validation_error("amount", "Amount must be a positive number.");
        }
        const int value = digit - '0';
        if (rupees > (maximum_rupees - value) / 10) {
            throw validation_error("amount", "Amount is too large.");
        }
        rupees = rupees * 10 + value;
    }

    int paise = 0;
    if (!fraction.empty()) {
        if (fraction[0] < '0' || fraction[0] > '9') {
            throw validation_error("amount", "Amount must be a positive number.");
        }
        paise = (fraction[0] - '0') * 10;
        if (fraction.size() == 2) {
            if (fraction[1] < '0' || fraction[1] > '9') {
                throw validation_error("amount", "Amount must be a positive number.");
            }
            paise += fraction[1] - '0';
        }
    }
    if (rupees > (std::numeric_limits<std::int64_t>::max() - paise) / 100) {
        throw validation_error("amount", "Amount is too large.");
    }

    const std::int64_t amount_paise = rupees * 100 + paise;
    if (amount_paise <= 0) {
        throw validation_error("amount", "Amount must be greater than zero.");
    }
    return amount_paise;
}

}  // namespace

DepositResult deposit(Database& database, const std::string& account_number,
                      const std::string& amount, std::int64_t operator_id) {
    const std::int64_t amount_paise = parse_amount(amount);
    if (account_number.empty()) {
        throw validation_error("account_number", "Account number is required.");
    }

    Database::Transaction transaction(database, true);
    std::int64_t account_id = 0;
    std::int64_t balance = 0;
    std::string status;
    {
        auto account = database.prepare(
            "SELECT account_id, status, balance FROM accounts WHERE account_number = ?");
        account->bind(1, account_number);
        if (!account->step()) {
            throw not_found("Account " + account_number + " was not found.");
        }

        account_id = account->column_int(0);
        const auto* status_text = account->column_text(1);
        status = status_text ? reinterpret_cast<const char*>(status_text) : "";
        balance = account->column_int(2);
    }

    if (status != "active") {
        throw business_rule("ACCOUNT_NOT_ACTIVE", "Account " + account_number + " is " + status + ".");
    }
    if (balance > std::numeric_limits<std::int64_t>::max() - amount_paise) {
        throw business_rule("BALANCE_LIMIT_EXCEEDED", "The deposit would exceed the supported balance.");
    }
    const std::int64_t resulting_balance = balance + amount_paise;

    {
        auto update = database.prepare("UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP "
                                       "WHERE account_id = ? AND status = 'active'");
        update->bind(1, resulting_balance);
        update->bind(2, account_id);
        update->step();
    }

    {
        auto insert_transaction = database.prepare(
            "INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance) "
            "VALUES (?, ?, 'deposit', ?, ?)");
        insert_transaction->bind(1, account_id);
        insert_transaction->bind(2, operator_id);
        insert_transaction->bind(3, amount_paise);
        insert_transaction->bind(4, resulting_balance);
        insert_transaction->step();
    }

    std::int64_t transaction_id = 0;
    {
        auto transaction_id_query = database.prepare("SELECT last_insert_rowid()");
        transaction_id_query->step();
        transaction_id = transaction_id_query->column_int(0);
    }

    {
        auto insert_audit = database.prepare(
            "INSERT INTO audit_log (user_id, action, entity, entity_id, details) "
            "VALUES (?, 'DEPOSIT', 'accounts', ?, ?)");
        insert_audit->bind(1, operator_id);
        insert_audit->bind(2, account_id);
        insert_audit->bind(3, "Deposited " + std::to_string(amount_paise) +
                                  " paise; resulting balance " + std::to_string(resulting_balance) + " paise.");
        insert_audit->step();
    }

    std::string created_at;
    {
        auto created_at_query = database.prepare(
            "SELECT created_at FROM transactions WHERE transaction_id = ?");
        created_at_query->bind(1, transaction_id);
        created_at_query->step();
        const auto* timestamp_text = created_at_query->column_text(0);
        created_at = timestamp_text ? reinterpret_cast<const char*>(timestamp_text) : "";
    }

    transaction.commit();
    return {transaction_id, account_id, operator_id, amount_paise, resulting_balance, created_at};
}

std::string format_money(std::int64_t amount_paise) {
    const std::int64_t rupees = amount_paise / 100;
    const std::int64_t paise = amount_paise % 100;
    return std::to_string(rupees) + (paise < 10 ? ".0" : ".") + std::to_string(paise);
}

}  // namespace bms::transaction
