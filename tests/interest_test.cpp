#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "common/config.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "reporting/interest.hpp"
#include "reporting/routes.hpp"
#include "support/check.hpp"
#include "support/seeded_db.hpp"
#include "transaction/deposit.hpp"

namespace {

struct ErrorSnapshot {
    std::string code;
    std::string message;
    std::optional<std::string> field;
};

ErrorSnapshot capture_error(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const bms::Error& error) {
        return {error.code(), error.what(), error.field()};
    }
    return {};
}

std::int64_t balance(bms::Database& database, const std::string& number) {
    auto query = database.prepare("SELECT balance FROM accounts WHERE account_number = ?");
    query->bind(1, number);
    query->step();
    return query->column_int(0);
}

std::int64_t row_count(bms::Database& database, const std::string& table) {
    auto query = database.prepare("SELECT COUNT(*) FROM " + table);
    query->step();
    return query->column_int(0);
}

void test_calculator_uses_days_and_rounds_half_up() {
    using bms::reporting::calculate_interest_paise;
    CHECK_EQ(calculate_interest_paise(1000000, 400, 28, 365), 3068);
    CHECK_EQ(calculate_interest_paise(1000000, 400, 29, 365), 3178);
    CHECK_EQ(calculate_interest_paise(1000000, 400, 30, 365), 3288);
    CHECK_EQ(calculate_interest_paise(1000000, 400, 31, 365), 3397);
    CHECK_EQ(calculate_interest_paise(1000000, 400, 29, 366), 3169);
    CHECK_EQ(calculate_interest_paise(1824999, 1, 1, 365), 0);
    CHECK_EQ(calculate_interest_paise(1825000, 1, 1, 365), 1);
    CHECK_EQ(calculate_interest_paise(1825001, 1, 1, 365), 1);
    CHECK_EQ(calculate_interest_paise(5000000, 0, 31, 365), 0);
}

void test_period_parser_handles_month_lengths_and_leap_years() {
    const auto february = bms::reporting::parse_interest_period("2024-02");
    CHECK_EQ(february.days_in_period, 29);
    CHECK_EQ(february.days_in_year, 366);
    CHECK_EQ(february.end_date, std::string("2024-02-29"));
    CHECK_EQ(february.end_exclusive, std::string("2024-03-01"));
    CHECK_EQ(bms::reporting::parse_interest_period("2026-04").days_in_period, 30);

    for (const std::string invalid : {"2026-1", "2026-13", "26-01", "2026/01"}) {
        const auto error = capture_error([&] { bms::reporting::parse_interest_period(invalid); });
        CHECK_EQ(error.code, std::string("VALIDATION_ERROR"));
        CHECK_EQ(error.field.value_or(""), std::string("period"));
    }
}

void test_interest_run_requires_a_completed_utc_month() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    auto current_period = database.prepare("SELECT strftime('%Y-%m', 'now')");
    current_period->step();
    const auto* current_text = current_period->column_text(0);
    const std::string label = reinterpret_cast<const char*>(current_text);
    const auto before = row_count(database, "interest_runs");
    const auto error = capture_error([&] { bms::reporting::run_interest(database, label, 1); });
    CHECK_EQ(error.code, std::string("VALIDATION_ERROR"));
    CHECK_EQ(error.field.value_or(""), std::string("period"));
    CHECK_EQ(row_count(database, "interest_runs"), before);
}

void test_run_credits_each_eligible_account_and_records_ledger_and_audit() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto run = bms::reporting::run_interest(database, "2026-04", 1);

    CHECK_EQ(run.accounts_credited, 2);
    CHECK_EQ(run.total_interest_paise, 17918);
    CHECK_EQ(balance(database, "ACC001"), 5015452);
    CHECK_EQ(balance(database, "ACC005"), 752466);
    CHECK_EQ(balance(database, "ACC002"), 2500000);  // Frozen.
    CHECK_EQ(balance(database, "ACC003"), 0);        // Closed.
    CHECK_EQ(balance(database, "ACC004"), 10000000); // Zero-rate Current.

    auto run_row = database.prepare(
        "SELECT run_by, accounts_credited, total_interest FROM interest_runs WHERE run_id = ?");
    run_row->bind(1, run.run_id);
    CHECK(run_row->step());
    CHECK_EQ(run_row->column_int(0), 1);
    CHECK_EQ(run_row->column_int(1), 2);
    CHECK_EQ(run_row->column_int(2), 17918);

    auto credits = database.prepare(
        "SELECT account_id, operator_id, type, amount, resulting_balance, interest_run_id, created_at "
        "FROM transactions WHERE interest_run_id = ? ORDER BY account_id");
    credits->bind(1, run.run_id);
    CHECK(credits->step());
    CHECK_EQ(credits->column_int(0), 1);
    CHECK_EQ(credits->column_int(1), 1);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(credits->column_text(2))), std::string("interest"));
    CHECK_EQ(credits->column_int(3), 15452);
    CHECK_EQ(credits->column_int(4), 5015452);
    CHECK_EQ(credits->column_int(5), run.run_id);
    CHECK(credits->column_text(6) != nullptr);
    CHECK(credits->step());
    CHECK_EQ(credits->column_int(0), 5);
    CHECK_EQ(credits->column_int(1), 1);
    CHECK_EQ(credits->column_int(3), 2466);
    CHECK_EQ(credits->column_int(4), 752466);
    CHECK(!credits->step());

    auto audits = database.prepare(
        "SELECT user_id, action, entity, entity_id, details FROM audit_log "
        "WHERE action = 'INTEREST_CREDIT' ORDER BY entity_id");
    CHECK(audits->step());
    CHECK_EQ(audits->column_int(0), 1);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audits->column_text(1))), std::string("INTEREST_CREDIT"));
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audits->column_text(2))), std::string("accounts"));
    CHECK_EQ(audits->column_int(3), 1);
    CHECK(audits->column_text(4) != nullptr);
    CHECK(audits->step());
    CHECK_EQ(audits->column_int(3), 5);
    CHECK(!audits->step());
}

void test_historical_opening_balance_is_inferred_and_later_opening_is_skipped() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    database.exec(R"(
        INSERT INTO accounts (account_id, account_number, customer_id, type_id, status, balance, opened_at)
        VALUES (6, 'ACC006', 1, 1, 'active', 3000000, '2026-01-10');
        INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at)
        VALUES (6, 2, 'deposit', 500000, 3000000, '2026-03-01 00:00:00');
    )");

    const auto run = bms::reporting::run_interest(database, "2026-02", 1);
    const auto account = std::find_if(run.accounts.begin(), run.accounts.end(), [](const auto& item) {
        return item.account_number == "ACC006";
    });
    CHECK(account != run.accounts.end());
    if (account != run.accounts.end()) {
        CHECK_EQ(account->balance_basis_paise, 2500000);
        CHECK_EQ(account->interest_paise, 7671);
        CHECK_EQ(account->resulting_balance_paise, 3007671);
    }
    CHECK_EQ(balance(database, "ACC005"), 750000); // Opened after February ended.
}

void test_zero_rounded_interest_creates_no_credit_row() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    database.exec(R"(
        INSERT INTO account_types (type_id, type_name, min_balance, interest_rate, daily_withdrawal_limit)
        VALUES (3, 'TinyRate', 0, 1, 5000000);
        INSERT INTO accounts (account_id, account_number, customer_id, type_id, status, balance, opened_at)
        VALUES (6, 'ACC006', 1, 3, 'active', 1, '2026-01-10');
    )");

    const auto run = bms::reporting::run_interest(database, "2026-02", 1);
    CHECK_EQ(balance(database, "ACC006"), 1);
    auto credit = database.prepare(
        "SELECT COUNT(*) FROM transactions WHERE account_id = 6 AND interest_run_id = ?");
    credit->bind(1, run.run_id);
    credit->step();
    CHECK_EQ(credit->column_int(0), 0);
}

void test_duplicate_period_is_rejected_without_mutation_and_other_period_works() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto first = bms::reporting::run_interest(database, "2026-04", 1);
    const auto transaction_count = row_count(database, "transactions");
    const auto audit_count = row_count(database, "audit_log");
    const auto account_one_balance = balance(database, "ACC001");
    const auto account_five_balance = balance(database, "ACC005");

    const auto duplicate = capture_error([&] {
        bms::reporting::run_interest(database, "2026-04", 1);
    });
    CHECK_EQ(duplicate.code, std::string("INTEREST_ALREADY_RUN"));
    CHECK(duplicate.message.find("2026-04") != std::string::npos);
    CHECK_EQ(row_count(database, "transactions"), transaction_count);
    CHECK_EQ(row_count(database, "audit_log"), audit_count);
    CHECK_EQ(balance(database, "ACC001"), account_one_balance);
    CHECK_EQ(balance(database, "ACC005"), account_five_balance);

    const auto next_period = bms::reporting::run_interest(database, "2026-05", 1);
    CHECK(next_period.run_id != first.run_id);
    CHECK_EQ(next_period.period, std::string("2026-05"));
}

void test_failure_on_second_account_audit_rolls_back_whole_run() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto account_one_balance = balance(database, "ACC001");
    const auto account_five_balance = balance(database, "ACC005");
    const auto transaction_count = row_count(database, "transactions");
    const auto audit_count = row_count(database, "audit_log");
    database.exec(R"(
        CREATE TRIGGER fail_second_interest_audit
        BEFORE INSERT ON audit_log
        WHEN NEW.action = 'INTEREST_CREDIT' AND NEW.entity_id = 5
        BEGIN SELECT RAISE(ABORT, 'forced second-account audit failure'); END;
    )");

    CHECK_THROWS_AS(bms::reporting::run_interest(database, "2026-04", 1), bms::DbError);
    CHECK_EQ(balance(database, "ACC001"), account_one_balance);
    CHECK_EQ(balance(database, "ACC005"), account_five_balance);
    CHECK_EQ(row_count(database, "transactions"), transaction_count);
    CHECK_EQ(row_count(database, "audit_log"), audit_count);
    auto run = database.prepare("SELECT COUNT(*) FROM interest_runs WHERE period = '2026-04'");
    run->step();
    CHECK_EQ(run->column_int(0), 0);
}

void test_api_success_duplicate_and_malformed_period() {
    bms::test::SeededDatabase fixture;
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    bms::api::Server server(config);
    bms::reporting::register_routes(server.router(), fixture.database(), 1);
    const int port = server.bind();
    std::thread server_thread([&server] { server.run(); });
    server.wait_until_ready();
    httplib::Client client("127.0.0.1", port);

    const auto success = client.Post("/interest/run", R"({"period":"2026-04"})", "application/json");
    CHECK(success);
    if (success) {
        const auto body = nlohmann::json::parse(success->body);
        CHECK_EQ(success->status, 201);
        CHECK_EQ(body["period"].get<std::string>(), std::string("2026-04"));
        CHECK_EQ(body["accounts_credited"].get<int>(), 2);
        CHECK_EQ(body["total_interest"].get<std::string>(), std::string("179.18"));
        CHECK_EQ(body["accounts"].size(), 2U);
    }

    const auto duplicate = client.Post("/interest/run", R"({"period":"2026-04"})", "application/json");
    CHECK(duplicate);
    if (duplicate) {
        const auto body = nlohmann::json::parse(duplicate->body);
        CHECK_EQ(duplicate->status, 409);
        CHECK_EQ(body["code"].get<std::string>(), std::string("INTEREST_ALREADY_RUN"));
        CHECK(body["message"].get<std::string>().find("2026-04") != std::string::npos);
    }

    const auto malformed = client.Post("/interest/run", R"({"period":"2026-13"})", "application/json");
    CHECK(malformed);
    if (malformed) {
        const auto body = nlohmann::json::parse(malformed->body);
        CHECK_EQ(malformed->status, 400);
        CHECK_EQ(body["code"].get<std::string>(), std::string("VALIDATION_ERROR"));
        CHECK_EQ(body["field"].get<std::string>(), std::string("period"));
    }

    server.stop();
    server_thread.join();
}

}  // namespace

int main() {
    bms::test::run("interest calculator uses actual days and half-up rounding", test_calculator_uses_days_and_rounds_half_up);
    bms::test::run("interest period parser validates months and leap years", test_period_parser_handles_month_lengths_and_leap_years);
    bms::test::run("interest runs require a completed UTC month", test_interest_run_requires_a_completed_utc_month);
    bms::test::run("interest run credits eligible accounts and records audit", test_run_credits_each_eligible_account_and_records_ledger_and_audit);
    bms::test::run("historical opening balance is inferred and later accounts are skipped", test_historical_opening_balance_is_inferred_and_later_opening_is_skipped);
    bms::test::run("zero-rounded interest has no transaction", test_zero_rounded_interest_creates_no_credit_row);
    bms::test::run("duplicate period is rejected while another period can run", test_duplicate_period_is_rejected_without_mutation_and_other_period_works);
    bms::test::run("second-account failure rolls back the whole run", test_failure_on_second_account_audit_rolls_back_whole_run);
    bms::test::run("interest API returns success, duplicate and validation errors", test_api_success_duplicate_and_malformed_period);
    return bms::test::finish();
}
