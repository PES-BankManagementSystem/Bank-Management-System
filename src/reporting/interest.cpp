#include "reporting/interest.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "audit/record.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "transaction/deposit.hpp"

namespace bms::reporting {
namespace {

bool is_leap_year(int year) {
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}

std::string two_digits(int value) {
    return value < 10 ? "0" + std::to_string(value) : std::to_string(value);
}

std::int64_t checked_multiply(std::int64_t left, std::int64_t right) {
    if (left != 0 && right > std::numeric_limits<std::int64_t>::max() / left) {
        throw std::overflow_error("Calculated interest exceeds the supported amount.");
    }
    return left * right;
}

std::int64_t checked_add(std::int64_t left, std::int64_t right) {
    if (right > std::numeric_limits<std::int64_t>::max() - left) {
        throw std::overflow_error("Calculated interest exceeds the supported amount.");
    }
    return left + right;
}

// Computes quotient and remainder for (a * b) / denominator without forming
// the potentially overflowing product. This is used with 0 <= a < denominator.
std::pair<std::int64_t, std::int64_t> multiply_divide_remainder(
    std::int64_t a, std::int64_t b, std::int64_t denominator) {
    const std::int64_t whole_b = b / denominator;
    const std::int64_t remainder_b = b % denominator;
    std::int64_t quotient = checked_multiply(a, whole_b);
    const std::int64_t remainder_product = a * remainder_b;
    quotient = checked_add(quotient, remainder_product / denominator);
    return {quotient, remainder_product % denominator};
}

std::int64_t month_day_count(int year, int month) {
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

std::string current_utc_date(Database& database) {
    auto query = database.prepare("SELECT date('now')");
    query->step();
    const auto* value = query->column_text(0);
    return value ? reinterpret_cast<const char*>(value) : "";
}

struct EligibleAccount {
    std::int64_t id;
    std::string number;
    std::string opened_at;
    std::int64_t current_balance;
    std::int64_t annual_rate;
};

std::int64_t period_end_balance(Database& database, std::int64_t account_id,
                                const std::string& end_exclusive,
                                std::int64_t unchanged_opening_balance) {
    {
        auto previous = database.prepare(
            "SELECT resulting_balance FROM transactions "
            "WHERE account_id = ? AND created_at < ? "
            "ORDER BY created_at DESC, transaction_id DESC LIMIT 1");
        previous->bind(1, account_id);
        previous->bind(2, end_exclusive);
        if (previous->step()) return previous->column_int(0);
    }

    // No ledger entry exists by period end. If a later first transaction is
    // present, reverse its signed amount to reconstruct the opening balance.
    {
        auto next = database.prepare(
            "SELECT type, amount, resulting_balance FROM transactions "
            "WHERE account_id = ? AND created_at >= ? "
            "ORDER BY created_at ASC, transaction_id ASC LIMIT 1");
        next->bind(1, account_id);
        next->bind(2, end_exclusive);
        if (next->step()) {
            const auto* type_text = next->column_text(0);
            const std::string type = type_text ? reinterpret_cast<const char*>(type_text) : "";
            const std::int64_t amount = next->column_int(1);
            const std::int64_t resulting_balance = next->column_int(2);
            if (type == "deposit" || type == "transfer_credit" || type == "interest") {
                if (resulting_balance < amount) {
                    throw std::runtime_error("Transaction history has an invalid resulting balance.");
                }
                return resulting_balance - amount;
            }
            if (type == "withdrawal" || type == "transfer_debit") {
                if (resulting_balance > std::numeric_limits<std::int64_t>::max() - amount) {
                    throw std::overflow_error("Historical account balance exceeds the supported amount.");
                }
                return resulting_balance + amount;
            }
            throw std::runtime_error("Transaction history contains an unsupported balance change.");
        }
    }
    return unchanged_opening_balance;
}

}  // namespace

InterestPeriod parse_interest_period(const std::string& period) {
    if (period.size() != 7 || period[4] != '-' ||
        !std::all_of(period.begin(), period.begin() + 4, [](unsigned char c) { return std::isdigit(c); }) ||
        !std::all_of(period.begin() + 5, period.end(), [](unsigned char c) { return std::isdigit(c); })) {
        throw validation_error("period", "Period must use YYYY-MM format.");
    }
    const int year = std::stoi(period.substr(0, 4));
    const int month = std::stoi(period.substr(5, 2));
    if (year == 0 || month < 1 || month > 12) {
        throw validation_error("period", "Period must be a valid calendar month in YYYY-MM format.");
    }

    const int days = static_cast<int>(month_day_count(year, month));
    const int year_days = is_leap_year(year) ? 366 : 365;
    const int next_month = month == 12 ? 1 : month + 1;
    const int next_year = month == 12 ? year + 1 : year;
    const std::string end_date = period + "-" + two_digits(days);
    return {period, std::to_string(next_year) + "-" + two_digits(next_month) + "-01",
            end_date, days, year_days};
}

std::int64_t calculate_interest_paise(std::int64_t balance_paise,
                                      std::int64_t annual_rate_basis_points,
                                      int days_in_period, int days_in_year) {
    if (balance_paise < 0 || annual_rate_basis_points < 0 || days_in_period < 1 ||
        days_in_period > 31 || (days_in_year != 365 && days_in_year != 366) ||
        days_in_period > days_in_year) {
        throw std::invalid_argument("Interest calculation inputs are outside their valid ranges.");
    }
    if (balance_paise == 0 || annual_rate_basis_points == 0) return 0;

    const std::int64_t denominator = 10000LL * days_in_year;
    const std::int64_t whole_balance = balance_paise / denominator;
    const std::int64_t remainder_balance = balance_paise % denominator;
    const std::int64_t whole_interest = checked_multiply(
        checked_multiply(whole_balance, annual_rate_basis_points), days_in_period);

    const auto [rate_quotient, rate_remainder] =
        multiply_divide_remainder(remainder_balance, annual_rate_basis_points, denominator);
    const std::int64_t remainder_times_days = rate_remainder * days_in_period;
    const std::int64_t fractional_interest = checked_add(
        checked_multiply(rate_quotient, days_in_period), remainder_times_days / denominator);
    std::int64_t interest = checked_add(whole_interest, fractional_interest);
    const std::int64_t fractional_remainder = remainder_times_days % denominator;

    // Round half up to the nearest paisa: an exact half-paisa is rounded upward.
    if (fractional_remainder * 2 >= denominator) {
        interest = checked_add(interest, 1);
    }
    return interest;
}

InterestRunResult run_interest(Database& database, const std::string& period_label,
                                std::int64_t administrator_id) {
    auth::rbac::require_role(database, administrator_id, auth::rbac::Operation::RunInterest);
    const InterestPeriod period = parse_interest_period(period_label);
    Database::Transaction transaction(database, true);

    if (period.end_exclusive > current_utc_date(database)) {
        throw validation_error("period", "Interest can only be run after the UTC calendar month has ended.");
    }

    {
        auto existing = database.prepare("SELECT 1 FROM interest_runs WHERE period = ?");
        existing->bind(1, period.label);
        if (existing->step()) {
            throw conflict("INTEREST_ALREADY_RUN", "Interest has already been run for period " + period.label + ".");
        }
    }

    {
        auto insert_run = database.prepare(
            "INSERT INTO interest_runs (period, run_by, accounts_credited, total_interest) "
            "VALUES (?, ?, 0, 0)");
        insert_run->bind(1, period.label);
        insert_run->bind(2, administrator_id);
        insert_run->step();
    }
    std::int64_t run_id = 0;
    {
        auto id = database.prepare("SELECT last_insert_rowid()");
        id->step();
        run_id = id->column_int(0);
    }

    std::vector<EligibleAccount> eligible;
    {
        auto accounts = database.prepare(
            "SELECT a.account_id, a.account_number, a.opened_at, a.balance, t.interest_rate "
            "FROM accounts a JOIN account_types t ON t.type_id = a.type_id "
            "WHERE a.status = 'active' AND t.interest_rate > 0 ORDER BY a.account_id");
        while (accounts->step()) {
            const auto* number = accounts->column_text(1);
            const auto* opened_at = accounts->column_text(2);
            eligible.push_back({accounts->column_int(0),
                                number ? reinterpret_cast<const char*>(number) : "",
                                opened_at ? reinterpret_cast<const char*>(opened_at) : "",
                                accounts->column_int(3), accounts->column_int(4)});
        }
    }

    InterestRunResult result{period.label, run_id, 0, 0, {}};
    for (const EligibleAccount& account : eligible) {
        if (account.opened_at.size() >= 10 && account.opened_at.substr(0, 10) >= period.end_exclusive) {
            continue;
        }
        const std::int64_t basis_balance = period_end_balance(
            database, account.id, period.end_exclusive, account.current_balance);
        const std::int64_t interest = calculate_interest_paise(
            basis_balance, account.annual_rate, period.days_in_period, period.days_in_year);
        if (interest == 0) continue;
        if (account.current_balance > std::numeric_limits<std::int64_t>::max() - interest) {
            throw std::overflow_error("Crediting interest would exceed the supported account balance.");
        }
        const std::int64_t resulting_balance = account.current_balance + interest;

        {
            auto update = database.prepare(
                "UPDATE accounts SET balance = ?, updated_at = CURRENT_TIMESTAMP "
                "WHERE account_id = ? AND status = 'active'");
            update->bind(1, resulting_balance);
            update->bind(2, account.id);
            update->step();
        }
        {
            auto insert_transaction = database.prepare(
                "INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, interest_run_id) "
                "VALUES (?, ?, 'interest', ?, ?, ?)");
            insert_transaction->bind(1, account.id);
            insert_transaction->bind(2, administrator_id);
            insert_transaction->bind(3, interest);
            insert_transaction->bind(4, resulting_balance);
            insert_transaction->bind(5, run_id);
            insert_transaction->step();
        }
        audit::record(database, administrator_id, "INTEREST_CREDIT", "accounts", account.id,
                      "Credited " + transaction::format_money(interest) +
                          " interest for period " + period.label + ".");

        result.accounts.push_back({account.id, account.number, basis_balance, interest, resulting_balance});
        result.accounts_credited = checked_add(result.accounts_credited, 1);
        result.total_interest_paise = checked_add(result.total_interest_paise, interest);
    }

    {
        auto update_run = database.prepare(
            "UPDATE interest_runs SET accounts_credited = ?, total_interest = ? WHERE run_id = ?");
        update_run->bind(1, result.accounts_credited);
        update_run->bind(2, result.total_interest_paise);
        update_run->bind(3, run_id);
        update_run->step();
    }

    transaction.commit();
    return result;
}

}  // namespace bms::reporting
