#include "reporting/statement.hpp"

#include <cctype>
#include <limits>
#include <string>

#include "common/db.hpp"
#include "common/error.hpp"
#include "common/input_validation.hpp"

namespace bms::reporting {
namespace {

std::string column_string(const Statement& row, int column) {
    const auto* value = row.column_text(column);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

bool is_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

bool valid_date(const std::string& value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-') return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(value[i]))) return false;
    }

    const int year = std::stoi(value.substr(0, 4));
    const int month = std::stoi(value.substr(5, 2));
    const int day = std::stoi(value.substr(8, 2));
    if (year == 0 || month < 1 || month > 12) return false;

    static constexpr int month_lengths[] = {31, 28, 31, 30, 31, 30,
                                             31, 31, 30, 31, 30, 31};
    const int days = month == 2 && is_leap_year(year) ? 29 : month_lengths[month - 1];
    return day >= 1 && day <= days;
}

std::string next_day_start(const std::string& date) {
    int year = std::stoi(date.substr(0, 4));
    int month = std::stoi(date.substr(5, 2));
    int day = std::stoi(date.substr(8, 2));
    static constexpr int month_lengths[] = {31, 28, 31, 30, 31, 30,
                                             31, 31, 30, 31, 30, 31};
    const int days = month == 2 && is_leap_year(year) ? 29 : month_lengths[month - 1];
    if (day < days) {
        ++day;
    } else if (month < 12) {
        ++month;
        day = 1;
    } else if (year < 9999) {
        ++year;
        month = 1;
        day = 1;
    } else {
        // This lexical sentinel is later than every valid timestamp on 9999-12-31.
        return "9999-12-32 00:00:00";
    }

    const auto two_digits = [](int number) {
        return number < 10 ? "0" + std::to_string(number) : std::to_string(number);
    };
    const std::string year_text = std::to_string(year);
    return std::string(4 - year_text.size(), '0') + year_text + "-" + two_digits(month) +
           "-" + two_digits(day) + " 00:00:00";
}

std::int64_t opening_balance_from_first_entry(const StatementEntry& entry) {
    if (entry.type == "deposit" || entry.type == "transfer_credit" || entry.type == "interest") {
        if (entry.resulting_balance_paise < entry.amount_paise) {
            throw std::runtime_error("Transaction history contains an invalid resulting balance.");
        }
        return entry.resulting_balance_paise - entry.amount_paise;
    }
    if (entry.type == "withdrawal" || entry.type == "transfer_debit") {
        if (entry.resulting_balance_paise >
            std::numeric_limits<std::int64_t>::max() - entry.amount_paise) {
            throw std::overflow_error("Historical account balance exceeds the supported amount.");
        }
        return entry.resulting_balance_paise + entry.amount_paise;
    }
    throw std::runtime_error("Transaction history contains an unsupported balance change.");
}

std::int64_t signed_amount(const std::string& type, std::int64_t amount_paise) {
    if (type == "withdrawal" || type == "transfer_debit") return -amount_paise;
    return amount_paise;
}

}  // namespace

AccountStatement get_account_statement(Database& database, const std::string& account_number,
                                      const std::string& from, const std::string& to) {
    if (!valid_date(from)) throw validation_error("from", "From must be a valid YYYY-MM-DD date.");
    if (!valid_date(to)) throw validation_error("to", "To must be a valid YYYY-MM-DD date.");
    if (from > to) throw validation_error("from", "From must be on or before to.");
    validate_account_number(account_number, "account_number");

    Database::Transaction read_snapshot(database);
    std::int64_t account_id = 0;
    std::int64_t current_balance = 0;
    {
        auto account = database.prepare(
            "SELECT account_id, balance FROM accounts WHERE account_number = ?");
        account->bind(1, account_number);
        if (!account->step()) throw not_found("Account " + account_number + " was not found.");
        account_id = account->column_int(0);
        current_balance = account->column_int(1);
    }

    AccountStatement result{account_number, from, to, current_balance, current_balance, {}};
    const std::string from_start = from + " 00:00:00";
    const std::string to_exclusive = next_day_start(to);
    bool has_previous = false;

    // The last transaction before the requested period gives the exact opening balance.
    {
        auto previous = database.prepare(
            "SELECT resulting_balance FROM transactions "
            "WHERE account_id = ? AND created_at < ? "
            "ORDER BY created_at DESC, transaction_id DESC LIMIT 1");
        previous->bind(1, account_id);
        previous->bind(2, from_start);
        if (previous->step()) {
            result.opening_balance_paise = previous->column_int(0);
            has_previous = true;
        }
    }

    auto entries = database.prepare(
        "SELECT transaction_id, type, amount, resulting_balance, created_at "
        "FROM transactions WHERE account_id = ? AND created_at >= ? AND created_at < ? "
        "ORDER BY created_at, transaction_id");
    entries->bind(1, account_id);
    entries->bind(2, from_start);
    entries->bind(3, to_exclusive);
    while (entries->step()) {
        const std::string type = column_string(*entries, 1);
        result.transactions.push_back({entries->column_int(0), type,
                                       signed_amount(type, entries->column_int(2)),
                                       entries->column_int(3), column_string(*entries, 4)});
    }

    if (!has_previous) {
        // If no prior ledger row exists, reverse the account's first row. This also
        // handles a first transaction after the requested period. With no ledger
        // rows at all, current_balance is the only available unchanged balance.
        auto first = database.prepare(
            "SELECT transaction_id, type, amount, resulting_balance, created_at "
            "FROM transactions WHERE account_id = ? "
            "ORDER BY created_at, transaction_id LIMIT 1");
        first->bind(1, account_id);
        if (first->step()) {
            const StatementEntry first_entry{first->column_int(0), column_string(*first, 1),
                                             first->column_int(2), first->column_int(3),
                                             column_string(*first, 4)};
            result.opening_balance_paise = opening_balance_from_first_entry(first_entry);
        }
    }

    result.closing_balance_paise = result.transactions.empty()
                                       ? result.opening_balance_paise
                                       : result.transactions.back().resulting_balance_paise;
    read_snapshot.commit();
    return result;
}

}  // namespace bms::reporting
