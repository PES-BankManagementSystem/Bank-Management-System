#include "reporting/statement.hpp"

#include <cctype>
#include <charconv>
#include <limits>
#include <optional>
#include <string>
#include <utility>

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

std::string stored_utc_datetime(const std::string& value) {
    std::string result = value;
    if (result.size() >= 10 && result[10] == ' ') result[10] = 'T';
    if (result.size() == 19) result.push_back('Z');
    return result;
}

std::optional<std::string> nonempty_date(const std::optional<std::string>& value) {
    if (!value || value->empty()) return std::nullopt;
    return value;
}

bool parse_page_number(const std::optional<std::string>& value, const std::string& field,
                       std::int64_t fallback, std::int64_t minimum,
                       std::int64_t maximum, std::int64_t& result,
                       Error::FieldErrors& errors, std::size_t maximum_digits = 9) {
    if (!value || value->empty()) {
        result = fallback;
        return true;
    }
    if (value->size() > maximum_digits ||
        value->find_first_not_of("0123456789") != std::string::npos) {
        errors.emplace_back(field, "Must be a whole number.");
        return false;
    }
    const auto parsed = std::from_chars(value->data(), value->data() + value->size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value->data() + value->size() ||
        result < minimum || result > maximum) {
        errors.emplace_back(field, "Must be between " + std::to_string(minimum) + " and " +
                                       std::to_string(maximum) + ".");
        return false;
    }
    return true;
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

AccountTransactionHistory get_account_transaction_history(
    Database& database, const std::string& account_number,
    std::optional<std::int64_t> customer_user_id,
    const std::optional<std::string>& from_value, const std::optional<std::string>& to_value,
    const std::optional<std::string>& limit_value, const std::optional<std::string>& offset_value,
    const std::optional<std::string>& snapshot_value) {
    validate_account_number(account_number, "account_number");

    const std::optional<std::string> from = nonempty_date(from_value);
    const std::optional<std::string> to = nonempty_date(to_value);
    Error::FieldErrors errors;
    if (from && !valid_date(*from)) {
        errors.emplace_back("from", "From must be a valid YYYY-MM-DD date.");
    }
    if (to && !valid_date(*to)) {
        errors.emplace_back("to", "To must be a valid YYYY-MM-DD date.");
    }

    std::int64_t limit = 20;
    std::int64_t offset = 0;
    std::int64_t requested_snapshot = 0;
    parse_page_number(limit_value, "limit", 20, 1, 100, limit, errors);
    parse_page_number(offset_value, "offset", 0, 0, 999999999, offset, errors);
    const bool has_snapshot = snapshot_value && !snapshot_value->empty();
    if (has_snapshot) {
        parse_page_number(snapshot_value, "snapshot", 0, 0,
                          std::numeric_limits<std::int64_t>::max(),
                          requested_snapshot, errors, 19);
    }
    if (from && to && valid_date(*from) && valid_date(*to) && *from > *to) {
        errors.emplace_back("from", "From must be on or before to.");
    }
    if (!errors.empty()) throw validation_errors(std::move(errors));

    Database::Transaction read_snapshot(database);
    std::int64_t account_id = 0;
    std::int64_t current_balance = 0;
    std::int64_t customer_id = 0;
    {
        auto account = database.prepare(
            "SELECT account_id, balance, customer_id FROM accounts WHERE account_number = ?");
        account->bind(1, account_number);
        if (!account->step()) {
            if (!customer_user_id) {
                throw not_found("Account " + account_number + " was not found.");
            }
            read_snapshot.rollback();
            return {account_number, 0, 0, 0, limit, offset, 0, true, {}};
        }
        account_id = account->column_int(0);
        current_balance = account->column_int(1);
        customer_id = account->column_int(2);
    }

    if (customer_user_id) {
        auto owner = database.prepare(
            "SELECT 1 FROM customers WHERE customer_id = ? AND user_id = ?");
        owner->bind(1, customer_id);
        owner->bind(2, *customer_user_id);
        if (!owner->step()) {
            read_snapshot.rollback();
            return {account_number, account_id, 0, 0, limit, offset, 0, true, {}};
        }
    }

    const std::optional<std::string> from_start =
        from ? std::optional<std::string>(*from + " 00:00:00") : std::nullopt;
    const std::optional<std::string> to_exclusive =
        to ? std::optional<std::string>(next_day_start(*to)) : std::nullopt;
    std::int64_t snapshot = requested_snapshot;
    if (!has_snapshot) {
        auto latest = database.prepare(
            "SELECT COALESCE(MAX(transaction_id), 0) FROM transactions WHERE account_id = ?");
        latest->bind(1, account_id);
        if (latest->step()) snapshot = latest->column_int(0);
    }
    AccountTransactionHistory result{
        account_number, account_id, current_balance, 0, limit, offset, snapshot, false, {}};

    auto count = database.prepare(
        "SELECT COUNT(*) FROM transactions "
        "WHERE account_id = ? AND transaction_id <= ? "
        "AND (? IS NULL OR created_at >= ?) "
        "AND (? IS NULL OR created_at < ?)");
    count->bind(1, account_id);
    count->bind(2, snapshot);
    if (from_start) {
        count->bind(3, *from_start);
        count->bind(4, *from_start);
    } else {
        count->bind(3, nullptr);
        count->bind(4, nullptr);
    }
    if (to_exclusive) {
        count->bind(5, *to_exclusive);
        count->bind(6, *to_exclusive);
    } else {
        count->bind(5, nullptr);
        count->bind(6, nullptr);
    }
    if (count->step()) result.total = count->column_int(0);

    auto entries = database.prepare(
        "SELECT t.transaction_id, t.type, t.amount, t.resulting_balance, t.created_at, "
        "(SELECT peer_account.account_number FROM transactions peer "
        "JOIN accounts peer_account ON peer_account.account_id = peer.account_id "
        "WHERE t.transfer_ref IS NOT NULL AND peer.transfer_ref = t.transfer_ref "
        "AND peer.account_id <> t.account_id LIMIT 1) "
        "FROM transactions t WHERE t.account_id = ? "
        "AND t.transaction_id <= ? "
        "AND (? IS NULL OR t.created_at >= ?) "
        "AND (? IS NULL OR t.created_at < ?) "
        "ORDER BY t.created_at DESC, t.transaction_id DESC LIMIT ? OFFSET ?");
    entries->bind(1, account_id);
    entries->bind(2, snapshot);
    if (from_start) {
        entries->bind(3, *from_start);
        entries->bind(4, *from_start);
    } else {
        entries->bind(3, nullptr);
        entries->bind(4, nullptr);
    }
    if (to_exclusive) {
        entries->bind(5, *to_exclusive);
        entries->bind(6, *to_exclusive);
    } else {
        entries->bind(5, nullptr);
        entries->bind(6, nullptr);
    }
    entries->bind(7, limit);
    entries->bind(8, offset);
    while (entries->step()) {
        const std::string type = column_string(*entries, 1);
        std::optional<std::string> counterpart;
        if (!entries->column_null(5)) counterpart = column_string(*entries, 5);
        result.transactions.push_back({
            entries->column_int(0),
            type,
            signed_amount(type, entries->column_int(2)),
            entries->column_int(3),
            stored_utc_datetime(column_string(*entries, 4)),
            std::move(counterpart),
        });
    }

    read_snapshot.commit();
    return result;
}

}  // namespace bms::reporting
