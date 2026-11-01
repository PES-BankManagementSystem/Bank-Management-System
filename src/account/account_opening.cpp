#include "account/account_opening.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include <sodium.h>

#include "audit/record.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "customer/records.hpp"

namespace bms::account {
namespace {

constexpr int kAccountNumberAttempts = 5;

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

std::string money_string(std::int64_t paise) {
    return std::to_string(paise / 100) + "." +
           (paise % 100 < 10 ? "0" : "") + std::to_string(paise % 100);
}

std::string transaction_reference(std::int64_t transaction_id) {
    std::ostringstream reference;
    reference << "TXN-" << std::setw(12) << std::setfill('0') << transaction_id;
    return reference.str();
}

bool valid_account_number(const std::string& value) {
    if (value.size() != 15 || value.compare(0, 3, "ACC") != 0) return false;
    for (std::size_t index = 3; index < value.size(); ++index) {
        if (value[index] < '0' || value[index] > '9') return false;
    }
    return true;
}

}  // namespace

std::string generate_account_number() {
    if (sodium_init() < 0) {
        throw std::runtime_error("Secure account-number generation is unavailable.");
    }

    std::array<char, 16> value{};
    const unsigned int first = randombytes_uniform(1000000U);
    const unsigned int second = randombytes_uniform(1000000U);
    const int written = std::snprintf(value.data(), value.size(), "ACC%06u%06u", first, second);
    if (written != 15) {
        throw std::runtime_error("Secure account-number generation failed.");
    }
    return std::string(value.data(), static_cast<std::size_t>(written));
}

OpenAccountResult open_account(Database& database, const OpenAccountInput& input,
                               std::int64_t operator_id) {
    return open_account(database, input, operator_id, generate_account_number);
}

OpenAccountResult open_account(Database& database, const OpenAccountInput& input,
                               std::int64_t operator_id,
                               const AccountNumberGenerator& account_number_generator) {
    if (input.customer_id.empty() ||
        std::all_of(input.customer_id.begin(), input.customer_id.end(),
                    [](unsigned char ch) { return std::isspace(ch) != 0; })) {
        throw validation_error("customerId", "Customer ID is required.");
    }
    if (input.account_type_id <= 0) {
        throw validation_error("accountType", "Select a valid account type.");
    }
    if (input.initial_deposit_paise < 0) {
        throw validation_error("initialDeposit", "Initial deposit cannot be negative.");
    }
    if (!account_number_generator) {
        throw std::invalid_argument("An account-number generator is required.");
    }

    auth::rbac::require_role(database, operator_id, auth::rbac::Operation::OpenAccount);

    Database::Transaction transaction(database, true);
    customer::require_active_customer(database, input.customer_id);

    std::int64_t customer_key = 0;
    std::string customer_code;
    {
        auto query = database.prepare(
            "SELECT customer_id, customer_code, status FROM customers "
            "WHERE customer_code = ? COLLATE NOCASE");
        query->bind(1, input.customer_id);
        if (!query->step()) {
            throw not_found("Customer " + input.customer_id + " was not found.");
        }
        customer_key = query->column_int(0);
        customer_code = column_string(*query, 1);
        if (column_string(*query, 2) != "active") {
            throw conflict("CUSTOMER_INACTIVE",
                           "Customer " + customer_code +
                               " has status inactive and cannot open a new account.");
        }
    }

    std::string account_type;
    std::int64_t minimum_balance_paise = 0;
    {
        auto query = database.prepare(
            "SELECT type_name, min_balance FROM account_types WHERE type_id = ?");
        query->bind(1, input.account_type_id);
        if (!query->step()) {
            throw Error(ErrorKind::NotFound, "NOT_FOUND",
                        "Account type " + std::to_string(input.account_type_id) +
                            " was not found.",
                        "accountType");
        }
        account_type = column_string(*query, 0);
        minimum_balance_paise = query->column_int(1);
    }
    if (input.initial_deposit_paise < minimum_balance_paise) {
        throw business_rule(
            "BELOW_MINIMUM_BALANCE",
            "The initial deposit is below the required minimum balance of INR " +
                money_string(minimum_balance_paise) + ".",
            money_string(minimum_balance_paise));
    }

    OpenAccountResult result{};
    result.account_type = account_type;
    result.status = "active";
    result.balance_paise = input.initial_deposit_paise;

    bool account_created = false;
    for (int attempt = 0; attempt < kAccountNumberAttempts; ++attempt) {
        const std::string candidate = account_number_generator();
        if (!valid_account_number(candidate)) {
            throw std::runtime_error("Account-number generator returned an invalid value.");
        }
        try {
            {
                auto insert = database.prepare(
                    "INSERT INTO accounts (account_number, customer_id, type_id, status, balance) "
                    "VALUES (?, ?, ?, 'active', ?)");
                insert->bind(1, candidate);
                insert->bind(2, customer_key);
                insert->bind(3, input.account_type_id);
                insert->bind(4, input.initial_deposit_paise);
                insert->step();
            }
            result.account_number = candidate;
            account_created = true;
        } catch (const DbError&) {
            auto collision = database.prepare(
                "SELECT 1 FROM accounts WHERE account_number = ?");
            collision->bind(1, candidate);
            if (!collision->step()) throw;
            if (attempt + 1 == kAccountNumberAttempts) {
                throw std::runtime_error("Unable to allocate a unique account number.");
            }
        }
        if (account_created) break;
    }
    if (!account_created) {
        throw std::runtime_error("Unable to allocate a unique account number.");
    }

    {
        auto query = database.prepare("SELECT last_insert_rowid()");
        query->step();
        result.account_id = query->column_int(0);
    }
    {
        auto insert = database.prepare(
            "INSERT INTO transactions "
            "(account_id, operator_id, type, amount, resulting_balance) "
            "VALUES (?, ?, 'deposit', ?, ?)");
        insert->bind(1, result.account_id);
        insert->bind(2, operator_id);
        insert->bind(3, input.initial_deposit_paise);
        insert->bind(4, input.initial_deposit_paise);
        insert->step();
    }
    {
        auto query = database.prepare("SELECT last_insert_rowid()");
        query->step();
        result.transaction_id = query->column_int(0);
    }
    result.transaction_reference = transaction_reference(result.transaction_id);
    {
        auto query = database.prepare(
            "SELECT created_at FROM transactions WHERE transaction_id = ?");
        query->bind(1, result.transaction_id);
        if (!query->step()) {
            throw std::runtime_error("The opening transaction could not be read.");
        }
        result.transaction_created_at = column_string(*query, 0);
    }

    audit::record(database, operator_id, "ACCOUNT_OPENED", "accounts", result.account_id,
                  "accountNumber=" + result.account_number + ";customerId=" + customer_code +
                      ";accountType=" + result.account_type + ";amountPaise=" +
                      std::to_string(input.initial_deposit_paise));
    transaction.commit();
    return result;
}

}  // namespace bms::account
