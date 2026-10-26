#include <functional>
#include <string>
#include <thread>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/config.hpp"
#include "common/error.hpp"
#include "support/check.hpp"
#include "support/login.hpp"
#include "support/seeded_db.hpp"
#include "transaction/deposit.hpp"
#include "transaction/routes.hpp"

namespace {

struct ErrorSnapshot {
    std::string code;
    std::string message;
};

ErrorSnapshot capture_error(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const bms::Error& error) {
        return {error.code(), error.what()};
    }
    return {};
}

std::int64_t account_balance(bms::Database& database, const std::string& account_number) {
    auto query = database.prepare("SELECT balance FROM accounts WHERE account_number = ?");
    query->bind(1, account_number);
    query->step();
    return query->column_int(0);
}

std::int64_t row_count(bms::Database& database, const std::string& table) {
    auto query = database.prepare("SELECT COUNT(*) FROM " + table);
    query->step();
    return query->column_int(0);
}

void test_deposit_updates_all_three_records_atomically() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();

    const auto result = bms::transaction::deposit(database, "ACC001", "125.50", 2);
    CHECK_EQ(result.amount_paise, 12550);
    CHECK_EQ(result.resulting_balance_paise, 5012550);
    CHECK_EQ(bms::transaction::format_money(result.resulting_balance_paise), std::string("50125.50"));
    CHECK_EQ(account_balance(database, "ACC001"), 5012550);

    auto transaction = database.prepare(
        "SELECT account_id, operator_id, type, amount, resulting_balance, created_at "
        "FROM transactions WHERE transaction_id = ?");
    transaction->bind(1, result.transaction_id);
    CHECK(transaction->step());
    CHECK_EQ(transaction->column_int(0), result.account_id);
    CHECK_EQ(transaction->column_int(1), 2);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(transaction->column_text(2))),
             std::string("deposit"));
    CHECK_EQ(transaction->column_int(3), 12550);
    CHECK_EQ(transaction->column_int(4), 5012550);
    CHECK(transaction->column_text(5) != nullptr);
    CHECK(!result.created_at.empty());

    auto audit = database.prepare(
        "SELECT user_id, action, entity, entity_id, details, timestamp "
        "FROM audit_log WHERE action = 'DEPOSIT' ORDER BY log_id DESC LIMIT 1");
    CHECK(audit->step());
    CHECK_EQ(audit->column_int(0), 2);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(1))), std::string("DEPOSIT"));
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(2))), std::string("accounts"));
    CHECK_EQ(audit->column_int(3), result.account_id);
    CHECK(audit->column_text(4) != nullptr);
    CHECK(audit->column_text(5) != nullptr);
}

void test_zero_negative_and_malformed_amounts_are_rejected() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const std::int64_t original_balance = account_balance(database, "ACC001");
    const std::int64_t original_transactions = row_count(database, "transactions");
    const std::int64_t original_audit = row_count(database, "audit_log");

    for (const std::string amount : {"0", "0.00", "-1.00", "abc", "1.001", "", "1..0"}) {
        const auto error = capture_error([&] {
            bms::transaction::deposit(database, "ACC001", amount, 2);
        });
        CHECK_EQ(error.code, std::string("VALIDATION_ERROR"));
    }

    CHECK_EQ(account_balance(database, "ACC001"), original_balance);
    CHECK_EQ(row_count(database, "transactions"), original_transactions);
    CHECK_EQ(row_count(database, "audit_log"), original_audit);
}

void test_frozen_and_closed_accounts_are_rejected_with_status() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const std::int64_t frozen_balance = account_balance(database, "ACC002");
    const auto frozen = capture_error([&] {
        bms::transaction::deposit(database, "ACC002", "10.00", 2);
    });
    CHECK_EQ(frozen.code, std::string("ACCOUNT_NOT_ACTIVE"));
    CHECK(frozen.message.find("frozen") != std::string::npos);
    CHECK_EQ(account_balance(database, "ACC002"), frozen_balance);

    const std::int64_t closed_balance = account_balance(database, "ACC003");
    const auto closed = capture_error([&] {
        bms::transaction::deposit(database, "ACC003", "10.00", 2);
    });
    CHECK_EQ(closed.code, std::string("ACCOUNT_NOT_ACTIVE"));
    CHECK(closed.message.find("closed") != std::string::npos);
    CHECK_EQ(account_balance(database, "ACC003"), closed_balance);
}

void test_unknown_account_returns_not_found() {
    bms::test::SeededDatabase fixture;
    const auto error = capture_error([&] {
        bms::transaction::deposit(fixture.database(), "UNKNOWN", "10.00", 2);
    });
    CHECK_EQ(error.code, std::string("NOT_FOUND"));
    CHECK(error.message.find("UNKNOWN") != std::string::npos);
}

void test_deposit_route_returns_json_success_and_account_errors() {
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
                                     R"({"type":"deposit","account_number":"ACC001","amount":"125.50"})",
                                     "application/json");
    CHECK(success);
    if (success) {
        const auto body = nlohmann::json::parse(success->body);
        CHECK_EQ(success->status, 201);
        CHECK_EQ(body["balance"].get<std::string>(), std::string("50125.50"));
        CHECK_EQ(body["transaction"]["type"].get<std::string>(), std::string("deposit"));
        CHECK_EQ(body["transaction"]["amount"].get<std::string>(), std::string("125.50"));
        CHECK_EQ(body["transaction"]["operator_id"].get<int>(), 2);
        CHECK(body["transaction"]["created_at"].is_string());
    }

    const auto frozen = client.Post("/transactions", officer,
                                    R"({"type":"deposit","account_number":"ACC002","amount":"10.00"})",
                                    "application/json");
    CHECK(frozen);
    if (frozen) {
        const auto body = nlohmann::json::parse(frozen->body);
        CHECK_EQ(frozen->status, 422);
        CHECK_EQ(body["code"].get<std::string>(), std::string("ACCOUNT_NOT_ACTIVE"));
        CHECK(body["message"].get<std::string>().find("frozen") != std::string::npos);
    }

    const auto unknown = client.Post("/transactions", officer,
                                     R"({"type":"deposit","account_number":"UNKNOWN","amount":"10.00"})",
                                     "application/json");
    CHECK(unknown);
    if (unknown) CHECK_EQ(unknown->status, 404);

    server.stop();
    server_thread.join();
}

void test_audit_failure_rolls_back_balance_and_transaction_row() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const std::int64_t original_balance = account_balance(database, "ACC001");
    const std::int64_t original_transactions = row_count(database, "transactions");
    const std::int64_t original_audit = row_count(database, "audit_log");
    database.exec(R"(
        CREATE TRIGGER fail_deposit_audit
        BEFORE INSERT ON audit_log
        WHEN NEW.action = 'DEPOSIT'
        BEGIN
            SELECT RAISE(ABORT, 'forced audit failure');
        END;
    )");

    CHECK_THROWS_AS(bms::transaction::deposit(database, "ACC001", "125.50", 2), bms::DbError);
    CHECK_EQ(account_balance(database, "ACC001"), original_balance);
    CHECK_EQ(row_count(database, "transactions"), original_transactions);
    CHECK_EQ(row_count(database, "audit_log"), original_audit);
}

}  // namespace

int main() {
    bms::test::run("deposit updates balance, transaction and audit records", test_deposit_updates_all_three_records_atomically);
    bms::test::run("invalid deposit amounts are rejected", test_zero_negative_and_malformed_amounts_are_rejected);
    bms::test::run("frozen and closed accounts are rejected", test_frozen_and_closed_accounts_are_rejected_with_status);
    bms::test::run("unknown account is not found", test_unknown_account_returns_not_found);
    bms::test::run("deposit route returns JSON success and account errors",
                   test_deposit_route_returns_json_success_and_account_errors);
    bms::test::run("audit failure rolls back all deposit changes", test_audit_failure_rolls_back_balance_and_transaction_row);
    return bms::test::finish();
}
