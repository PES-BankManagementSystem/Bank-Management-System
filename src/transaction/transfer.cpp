#include "transaction/transfer.hpp"

#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>

#include "common/db.hpp"
#include "common/error.hpp"
#include "transaction/deposit.hpp"

namespace bms::transaction {
namespace {

struct Account {
    std::int64_t id;
    std::int64_t balance;
    std::int64_t minimum_balance;
    std::string status;
};

Account load_account(Database& database, const std::string& number) {
    auto query = database.prepare(
        "SELECT a.account_id, a.balance, t.min_balance, a.status "
        "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
        "WHERE a.account_number = ?");
    query->bind(1, number);
    if (!query->step()) {
        throw not_found("Account " + number + " was not found.");
    }
    const auto* status_text = query->column_text(3);
    return {query->column_int(0), query->column_int(1), query->column_int(2),
            status_text ? reinterpret_cast<const char*>(status_text) : ""};
}

std::int64_t insert_transaction(Database& database, std::int64_t account_id,
                                std::int64_t operator_id, const char* type,
                                std::int64_t amount_paise, std::int64_t resulting_balance,
                                const std::string& created_at, const std::string& transfer_reference) {
    auto insert = database.prepare(
        "INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at, transfer_ref) "
        "VALUES (?, ?, ?, ?, ?, ?, ?)");
    insert->bind(1, account_id);
    insert->bind(2, operator_id);
    insert->bind(3, std::string(type));
    insert->bind(4, amount_paise);
    insert->bind(5, resulting_balance);
    insert->bind(6, created_at);
    insert->bind(7, transfer_reference);
    insert->step();

    auto id = database.prepare("SELECT last_insert_rowid()");
    id->step();
    return id->column_int(0);
}

void update_balance(Database& database, std::int64_t account_id, std::int64_t balance) {
    auto update = database.prepare(
        "UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP "
        "WHERE account_id = ? AND status = 'active'");
    update->bind(1, balance);
    update->bind(2, account_id);
    update->step();
}

std::int64_t next_transaction_id(Database& database) {
    auto query = database.prepare("SELECT COALESCE(MAX(transaction_id), 0) + 1 FROM transactions");
    query->step();
    return query->column_int(0);
}

std::string current_utc_timestamp(Database& database) {
    auto query = database.prepare("SELECT strftime('%Y-%m-%d %H:%M:%S', 'now')");
    query->step();
    const auto* timestamp = query->column_text(0);
    return timestamp ? reinterpret_cast<const char*>(timestamp) : "";
}

void insert_audit(Database& database, std::int64_t operator_id, std::int64_t account_id,
                  const std::string& action, const std::string& details) {
    auto insert = database.prepare(
        "INSERT INTO audit_log (user_id, action, entity, entity_id, details) VALUES (?, ?, 'accounts', ?, ?)");
    insert->bind(1, operator_id);
    insert->bind(2, action);
    insert->bind(3, account_id);
    insert->bind(4, details);
    insert->step();
}

}  // namespace

std::string make_transfer_reference(const std::string& created_at,
                                    std::int64_t source_transaction_id) {
    std::string timestamp;
    timestamp.reserve(created_at.size());
    for (const unsigned char ch : created_at) {
        if (std::isdigit(ch)) timestamp.push_back(static_cast<char>(ch));
    }
    return "TRF-" + timestamp + "Z-" + std::to_string(source_transaction_id);
}

TransferResult transfer(Database& database, const std::string& from_account_number,
                        const std::string& to_account_number, const std::string& amount,
                        std::int64_t operator_id) {
    const std::int64_t amount_paise = parse_amount_paise(amount);
    if (from_account_number.empty()) {
        throw validation_error("from_account_number", "Source account number is required.");
    }
    if (to_account_number.empty()) {
        throw validation_error("to_account_number", "Destination account number is required.");
    }
    if (from_account_number == to_account_number) {
        throw validation_error("to_account_number", "Source and destination accounts must be different.");
    }

    // SQLite serializes writers at the database level. Taking this lock before
    // reading either balance prevents concurrent A-to-B and B-to-A checks from
    // interleaving or deadlocking on separate account locks.
    Database::Transaction transaction(database, true);
    const Account source = load_account(database, from_account_number);
    const Account destination = load_account(database, to_account_number);

    if (source.status != "active") {
        throw business_rule("ACCOUNT_NOT_ACTIVE",
                            "Source account " + from_account_number + " is " + source.status + ".");
    }
    if (destination.status != "active") {
        throw business_rule("ACCOUNT_NOT_ACTIVE",
                            "Destination account " + to_account_number + " is " + destination.status + ".");
    }

    if (source.balance < source.minimum_balance ||
        amount_paise > source.balance - source.minimum_balance) {
        const std::int64_t maximum = source.balance > source.minimum_balance
            ? source.balance - source.minimum_balance : 0;
        throw business_rule("BELOW_MINIMUM_BALANCE",
                            "The source account minimum balance is ₹" + format_money(source.minimum_balance) +
                            "; the maximum amount that could be transferred is ₹" + format_money(maximum) + ".");
    }
    if (destination.balance > std::numeric_limits<std::int64_t>::max() - amount_paise) {
        throw business_rule("BALANCE_LIMIT_EXCEEDED", "The destination balance would exceed the supported balance.");
    }

    // Under BEGIN IMMEDIATE, no other writer can consume this next row ID.
    // Include it in the reference so it remains unique even within one second.
    const std::int64_t expected_source_transaction_id = next_transaction_id(database);
    const std::string created_at = current_utc_timestamp(database);
    const std::string reference = make_transfer_reference(created_at, expected_source_transaction_id);
    const std::int64_t source_balance = source.balance - amount_paise;
    const std::int64_t destination_balance = destination.balance + amount_paise;
    update_balance(database, source.id, source_balance);

    const std::int64_t source_transaction_id =
        insert_transaction(database, source.id, operator_id, "transfer_debit", amount_paise,
                           source_balance, created_at, reference);
    if (source_transaction_id != expected_source_transaction_id) {
        throw std::runtime_error("Unable to allocate a unique transfer reference.");
    }

    // Any failure from this point is rolled back with the debit by the scope guard.
    update_balance(database, destination.id, destination_balance);
    const std::int64_t destination_transaction_id =
        insert_transaction(database, destination.id, operator_id, "transfer_credit", amount_paise,
                           destination_balance, created_at, reference);

    const std::string details = "Transfer " + reference + " of " + format_money(amount_paise) +
        "; resulting balance " + format_money(source_balance) + ".";
    insert_audit(database, operator_id, source.id, "TRANSFER_OUT", details);
    insert_audit(database, operator_id, destination.id,
                 "TRANSFER_IN", "Transfer " + reference + " of " + format_money(amount_paise) +
                    "; resulting balance " + format_money(destination_balance) + ".");

    transaction.commit();
    return {reference, source_transaction_id, destination_transaction_id, source.id, destination.id,
            operator_id, amount_paise, source_balance, destination_balance, created_at};
}

}  // namespace bms::transaction
