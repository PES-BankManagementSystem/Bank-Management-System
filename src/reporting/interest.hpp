#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bms {
class Database;
}

namespace bms::reporting {

struct InterestPeriod {
    std::string label;
    std::string end_exclusive;
    std::string end_date;
    int days_in_period;
    int days_in_year;
};

struct InterestAccountResult {
    std::int64_t account_id;
    std::string account_number;
    std::int64_t balance_basis_paise;
    std::int64_t interest_paise;
    std::int64_t resulting_balance_paise;
};

struct InterestRunResult {
    std::string period;
    std::int64_t run_id;
    std::int64_t accounts_credited;
    std::int64_t total_interest_paise;
    std::vector<InterestAccountResult> accounts;
};

InterestPeriod parse_interest_period(const std::string& period);

// Uses annual basis points and actual calendar days. Exact half-paise results
// round up to the next paisa; all arithmetic remains integral.
std::int64_t calculate_interest_paise(std::int64_t balance_paise,
                                      std::int64_t annual_rate_basis_points,
                                      int days_in_period, int days_in_year);

InterestRunResult run_interest(Database& database, const std::string& period,
                                std::int64_t administrator_id);

}  // namespace bms::reporting
