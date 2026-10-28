#include <functional>
#include <string>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "auth/routes.hpp"
#include "common/config.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "support/check.hpp"
#include "support/login.hpp"
#include "support/seeded_db.hpp"
#include "transaction/withdraw.hpp"
#include "transaction/routes.hpp"

namespace {

struct ErrorSnapshot { std::string code; std::string message; };

ErrorSnapshot capture_error(const std::function<void()>& operation) {
    try { operation(); }
    catch (const bms::Error& error) { return {error.code(), error.what()}; }
    return {};
}

std::int64_t balance(bms::Database& db, const std::string& number) {
    auto query = db.prepare("SELECT balance FROM accounts WHERE account_number = ?");
    query->bind(1, number);
    query->step();
    return query->column_int(0);
}

std::int64_t count(bms::Database& db, const std::string& table) {
    auto query = db.prepare("SELECT COUNT(*) FROM " + table);
    query->step();
    return query->column_int(0);
}

void set_balance(bms::Database& db, const std::string& number, std::int64_t value) {
    auto update = db.prepare("UPDATE accounts SET balance = ? WHERE account_number = ?");
    update->bind(1, value);
    update->bind(2, number);
    update->step();
}

void insert_today_withdrawal(bms::Database& db, std::int64_t account_id, std::int64_t amount) {
    auto insert = db.prepare(
        "INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
        "VALUES (?, 2, 'withdrawal', ?, 1, datetime('now'))");
    insert->bind(1, account_id);
    insert->bind(2, amount);
    insert->step();
}

void test_success_records_withdrawal_and_audit_atomically() {
    bms::test::SeededDatabase fixture;
    auto& db = fixture.database();
    const auto result = bms::transaction::withdraw(db, "ACC001", "10.25", 2);
    CHECK_EQ(result.amount_paise, 1025);
    CHECK_EQ(result.resulting_balance_paise, 4998975);
    CHECK_EQ(balance(db, "ACC001"), 4998975);
    auto tx = db.prepare("SELECT type, amount, resulting_balance, operator_id FROM transactions WHERE transaction_id = ?");
    tx->bind(1, result.transaction_id);
    CHECK(tx->step());
    CHECK_EQ(std::string(reinterpret_cast<const char*>(tx->column_text(0))), std::string("withdrawal"));
    CHECK_EQ(tx->column_int(1), 1025);
    CHECK_EQ(tx->column_int(2), 4998975);
    CHECK_EQ(tx->column_int(3), 2);
    auto audit = db.prepare("SELECT action, entity_id FROM audit_log WHERE action = 'WITHDRAWAL' ORDER BY log_id DESC LIMIT 1");
    CHECK(audit->step());
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(0))), std::string("WITHDRAWAL"));
    CHECK_EQ(audit->column_int(1), result.account_id);
}

void test_minimum_balance_boundary_and_rejection() {
    bms::test::SeededDatabase fixture;
    auto& db = fixture.database();
    set_balance(db, "ACC001", 5000000);
    const auto allowed = bms::transaction::withdraw(db, "ACC001", "49000.00", 2);
    CHECK_EQ(allowed.resulting_balance_paise, 100000);

    set_balance(db, "ACC001", 5000000);
    const auto before_transactions = count(db, "transactions");
    const auto error = capture_error([&] { bms::transaction::withdraw(db, "ACC001", "49000.01", 2); });
    CHECK_EQ(error.code, std::string("BELOW_MINIMUM_BALANCE"));
    CHECK(error.message.find("1000.00") != std::string::npos);
    CHECK_EQ(balance(db, "ACC001"), 5000000);
    CHECK_EQ(count(db, "transactions"), before_transactions);
}

void test_daily_limit_boundary_cumulative_and_remaining_message() {
    bms::test::SeededDatabase fixture;
    auto& db = fixture.database();
    set_balance(db, "ACC004", 30000000);
    const auto exact = bms::transaction::withdraw(db, "ACC004", "200000.00", 2);
    CHECK_EQ(exact.resulting_balance_paise, 10000000);

    bms::test::SeededDatabase cumulative_fixture;
    auto& cumulative_db = cumulative_fixture.database();
    set_balance(cumulative_db, "ACC004", 30000000);
    auto account = cumulative_db.prepare("SELECT account_id FROM accounts WHERE account_number = 'ACC004'");
    account->step();
    insert_today_withdrawal(cumulative_db, account->column_int(0), 19999900);
    const auto rejected = capture_error([&] { bms::transaction::withdraw(cumulative_db, "ACC004", "1.01", 2); });
    CHECK_EQ(rejected.code, std::string("DAILY_LIMIT_EXCEEDED"));
    CHECK(rejected.message.find("1.00") != std::string::npos);
    (void)bms::transaction::withdraw(cumulative_db, "ACC004", "1.00", 2);
}

void test_only_successful_withdrawals_today_count() {
    bms::test::SeededDatabase fixture;
    auto& db = fixture.database();
    // Neither a same-day deposit nor a previous-day withdrawal counts toward today's total.
    db.exec("INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
            "VALUES (1, 2, 'deposit', 5000000, 10000000, datetime('now'))");
    db.exec("INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
            "VALUES (1, 2, 'withdrawal', 5000000, 100000, datetime('now', '-1 day'))");
    const auto result = bms::transaction::withdraw(db, "ACC001", "49000.00", 2);
    CHECK_EQ(result.resulting_balance_paise, 100000);
}

void test_invalid_amounts_status_and_unknown_account() {
    bms::test::SeededDatabase fixture;
    auto& db = fixture.database();
    const auto original = balance(db, "ACC001");
    for (const std::string amount : {"0", "-1", "1.001", "abc"}) {
        const auto error = capture_error([&] { bms::transaction::withdraw(db, "ACC001", amount, 2); });
        CHECK_EQ(error.code, std::string("VALIDATION_ERROR"));
    }
    CHECK_EQ(balance(db, "ACC001"), original);
    for (const std::string number : {"ACC002", "ACC003"}) {
        const auto error = capture_error([&] { bms::transaction::withdraw(db, number, "1.00", 2); });
        CHECK_EQ(error.code, std::string("ACCOUNT_NOT_ACTIVE"));
    }
    const auto missing = capture_error([&] { bms::transaction::withdraw(db, "UNKNOWN", "1.00", 2); });
    CHECK_EQ(missing.code, std::string("NOT_FOUND"));
}

void test_audit_failure_rolls_back_every_write() {
    bms::test::SeededDatabase fixture;
    auto& db = fixture.database();
    const auto old_balance = balance(db, "ACC001");
    const auto old_transactions = count(db, "transactions");
    const auto old_audit = count(db, "audit_log");
    db.exec("CREATE TRIGGER fail_withdrawal_audit BEFORE INSERT ON audit_log "
            "WHEN NEW.action = 'WITHDRAWAL' BEGIN SELECT RAISE(ABORT, 'forced audit failure'); END");
    CHECK_THROWS_AS(bms::transaction::withdraw(db, "ACC001", "10.00", 2), bms::DbError);
    CHECK_EQ(balance(db, "ACC001"), old_balance);
    CHECK_EQ(count(db, "transactions"), old_transactions);
    CHECK_EQ(count(db, "audit_log"), old_audit);
}

void test_api_success_and_business_rule_responses() {
    bms::test::SeededDatabase fixture;
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    bms::api::Server server(config);
    bms::auth::register_routes(server.router(), fixture.database(), true);
    bms::transaction::register_routes(server.router(), fixture.database());
    const int port = server.bind();
    std::thread server_thread([&server] { server.run(); });
    server.wait_until_ready();
    httplib::Client client("127.0.0.1", port);
    const auto officer = bms::test::sign_in(client, "officer1");
    const auto success = client.Post("/transactions", officer,
        R"({"type":"withdraw","account_number":"ACC001","amount":"10.00"})", "application/json");
    CHECK(success);
    if (success) {
        const auto body = nlohmann::json::parse(success->body);
        CHECK_EQ(success->status, 201);
        CHECK_EQ(body["transaction"]["type"].get<std::string>(), std::string("withdrawal"));
        CHECK_EQ(body["balance"].get<std::string>(), std::string("49990.00"));
    }
    const auto below_minimum = client.Post("/transactions", officer,
        R"({"type":"withdraw","account_number":"ACC005","amount":"7000.01"})", "application/json");
    CHECK(below_minimum);
    if (below_minimum) {
        const auto body = nlohmann::json::parse(below_minimum->body);
        CHECK_EQ(below_minimum->status, 422);
        CHECK_EQ(body["code"].get<std::string>(), std::string("BELOW_MINIMUM_BALANCE"));
        CHECK(body["message"].get<std::string>().find("1000.00") != std::string::npos);
    }
    set_balance(fixture.database(), "ACC004", 30000000);
    const auto daily_limit = client.Post("/transactions", officer,
        R"({"type":"withdraw","account_number":"ACC004","amount":"200000.01"})", "application/json");
    CHECK(daily_limit);
    if (daily_limit) {
        const auto body = nlohmann::json::parse(daily_limit->body);
        CHECK_EQ(daily_limit->status, 422);
        CHECK_EQ(body["code"].get<std::string>(), std::string("DAILY_LIMIT_EXCEEDED"));
        CHECK(body["message"].get<std::string>().find("200000.00") != std::string::npos);
    }
    server.stop();
    server_thread.join();
}

}  // namespace

int main() {
    bms::test::run("withdrawal records balance, transaction and audit", test_success_records_withdrawal_and_audit_atomically);
    bms::test::run("minimum balance boundary is enforced", test_minimum_balance_boundary_and_rejection);
    bms::test::run("daily limit boundary and remaining amount are enforced", test_daily_limit_boundary_cumulative_and_remaining_message);
    bms::test::run("only today's withdrawals count", test_only_successful_withdrawals_today_count);
    bms::test::run("invalid input and account states are rejected", test_invalid_amounts_status_and_unknown_account);
    bms::test::run("audit failure rolls back withdrawal", test_audit_failure_rolls_back_every_write);
    bms::test::run("withdrawal API returns success and business rule errors", test_api_success_and_business_rule_responses);
    return bms::test::finish();
}
