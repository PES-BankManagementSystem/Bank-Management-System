#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <numeric>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/router.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "reporting/routes.hpp"
#include "reporting/statement.hpp"
#include "support/login.hpp"
#include "support/seeded_database.hpp"

namespace {

using nlohmann::json;

std::int64_t entry_total(const bms::reporting::AccountStatement& statement) {
    return std::accumulate(statement.transactions.begin(), statement.transactions.end(),
                           std::int64_t{0}, [](std::int64_t total, const auto& entry) {
                               return total + entry.amount_paise;
                           });
}

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        bms::reporting::register_routes(router_, database);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind statement test server.");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        client_ = std::make_unique<httplib::Client>("127.0.0.1", port_);
        client_->set_connection_timeout(2);
        client_->set_read_timeout(10);
        for (int attempt = 0; attempt < 50 && !server_.is_running(); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    ~TestServer() {
        server_.stop();
        if (thread_.joinable()) thread_.join();
    }

    httplib::Client& client() { return *client_; }

private:
    httplib::Server server_;
    bms::api::Router router_;
    int port_ = 0;
    std::thread thread_;
    std::unique_ptr<httplib::Client> client_;
};

class StatementTest : public bms::test::SeededDatabaseTest {};

TEST_F(StatementTest, ReturnsOpeningTransactionsAndClosingBalanceForInclusiveDates) {
    TestServer server(db());
    const auto admin = bms::test::sign_in(server.client(), "admin");

    const auto response = server.client().Get(
        "/accounts/ACC001/statement?from=2026-01-18&to=2026-01-18", admin);
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200) << response->body;
    const json statement = json::parse(response->body);
    EXPECT_EQ(statement["account_number"], "ACC001");
    EXPECT_EQ(statement["from"], "2026-01-18");
    EXPECT_EQ(statement["to"], "2026-01-18");
    EXPECT_EQ(statement["opening_balance"], "49000.00");
    ASSERT_EQ(statement["transactions"].size(), 1);
    EXPECT_EQ(statement["transactions"][0]["type"], "transfer_debit");
    EXPECT_EQ(statement["transactions"][0]["amount"], "-2000.00");
    EXPECT_EQ(statement["transactions"][0]["resulting_balance"], "47000.00");
    EXPECT_EQ(statement["closing_balance"], "47000.00");
}

TEST_F(StatementTest, EmptyPeriodReturnsUnchangedBalanceAndNoEntries) {
    const auto statement = bms::reporting::get_account_statement(
        db(), "ACC001", "2026-02-01", "2026-02-02");
    EXPECT_EQ(statement.opening_balance_paise, 4700000);
    EXPECT_TRUE(statement.transactions.empty());
    EXPECT_EQ(statement.closing_balance_paise, 4700000);
}

TEST_F(StatementTest, CustomerCanReadOwnAccountButNotAnotherCustomersAccount) {
    TestServer server(db());
    const auto own_customer = bms::test::sign_in(server.client(), "customer1");
    const auto other_customer = bms::test::sign_in(server.client(), "customer2");

    const auto own = server.client().Get(
        "/accounts/ACC001/statement?from=2026-01-18&to=2026-01-18", own_customer);
    ASSERT_TRUE(own);
    EXPECT_EQ(own->status, 200) << own->body;

    const auto other = server.client().Get(
        "/accounts/ACC001/statement?from=2026-01-18&to=2026-01-18", other_customer);
    ASSERT_TRUE(other);
    EXPECT_EQ(other->status, 403) << other->body;

    const auto unauthenticated = server.client().Get(
        "/accounts/ACC001/statement?from=2026-01-18&to=2026-01-18");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);

    const auto officer = bms::test::sign_in(server.client(), "officer1");
    const auto staff_access = server.client().Get(
        "/accounts/ACC001/statement?from=2026-01-18&to=2026-01-18", officer);
    ASSERT_TRUE(staff_access);
    EXPECT_EQ(staff_access->status, 200) << staff_access->body;
}

TEST_F(StatementTest, EndpointRequiresBothValidDateParameters) {
    TestServer server(db());
    const auto admin = bms::test::sign_in(server.client(), "admin");

    const auto no_dates = server.client().Get("/accounts/ACC001/statement", admin);
    ASSERT_TRUE(no_dates);
    ASSERT_EQ(no_dates->status, 400) << no_dates->body;
    const json no_dates_error = json::parse(no_dates->body);
    EXPECT_EQ(no_dates_error["code"], "VALIDATION_ERROR");
    EXPECT_TRUE(no_dates_error.contains("field"));
    EXPECT_TRUE(no_dates_error.contains("message"));

    const auto missing = server.client().Get(
        "/accounts/ACC001/statement?from=2026-01-18", admin);
    ASSERT_TRUE(missing);
    ASSERT_EQ(missing->status, 400) << missing->body;
    const json missing_error = json::parse(missing->body);
    EXPECT_EQ(missing_error["code"], "VALIDATION_ERROR");
    EXPECT_EQ(missing_error["field"], "to");
    EXPECT_TRUE(missing_error.contains("message"));

    const auto invalid = server.client().Get(
        "/accounts/ACC001/statement?from=2026-02-30&to=2026-03-01", admin);
    ASSERT_TRUE(invalid);
    ASSERT_EQ(invalid->status, 400) << invalid->body;
    const json invalid_error = json::parse(invalid->body);
    EXPECT_EQ(invalid_error["code"], "VALIDATION_ERROR");
    EXPECT_EQ(invalid_error["field"], "from");
    EXPECT_TRUE(invalid_error.contains("message"));

    const auto reversed = server.client().Get(
        "/accounts/ACC001/statement?from=2026-03-02&to=2026-03-01", admin);
    ASSERT_TRUE(reversed);
    ASSERT_EQ(reversed->status, 400) << reversed->body;
    EXPECT_EQ(json::parse(reversed->body)["code"], "VALIDATION_ERROR");
}

TEST_F(StatementTest, ValidatesDatesAndReconstructsOpeningBalanceWithoutPriorRows) {
    EXPECT_THROW(bms::reporting::get_account_statement(
                     db(), "ACC001", "2026-02-30", "2026-03-01"),
                 bms::Error);
    EXPECT_THROW(bms::reporting::get_account_statement(
                     db(), "ACC001", "2026-03-02", "2026-03-01"),
                 bms::Error);

    db().exec("INSERT INTO accounts (account_id, account_number, customer_id, type_id, status, balance, opened_at) "
              "VALUES (6, 'ACC006', 1, 1, 'active', 800, '2026-05-01')");
    db().exec("INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
              "VALUES (6, 2, 'withdrawal', 100, 900, '2026-05-31 23:59:59')");
    db().exec("INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
              "VALUES (6, 2, 'deposit', 100, 1000, '2026-06-01 00:00:00')");
    db().exec("INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
              "VALUES (6, 2, 'transfer_debit', 200, 800, '2026-06-01 23:59:59')");
    db().exec("INSERT INTO accounts (account_id, account_number, customer_id, type_id, status, balance, opened_at) "
              "VALUES (7, 'ACC007', 1, 1, 'active', 7500, '2026-06-01')");
    db().exec("INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance, created_at) "
              "VALUES (7, 2, 'deposit', 500, 7500, '2026-06-05 10:00:00')");
    db().exec("INSERT INTO accounts (account_id, account_number, customer_id, type_id, status, balance, opened_at) "
              "VALUES (8, 'ACC008', 1, 1, 'active', 12345, '2026-04-01')");

    const auto day_edges = bms::reporting::get_account_statement(
        db(), "ACC006", "2026-06-01", "2026-06-01");
    ASSERT_EQ(day_edges.transactions.size(), 2);
    EXPECT_EQ(day_edges.transactions[0].created_at, "2026-06-01 00:00:00");
    EXPECT_EQ(day_edges.transactions[0].amount_paise, 100);
    EXPECT_EQ(day_edges.transactions[1].created_at, "2026-06-01 23:59:59");
    EXPECT_EQ(day_edges.transactions[1].amount_paise, -200);
    EXPECT_EQ(day_edges.opening_balance_paise, 900);
    EXPECT_EQ(day_edges.closing_balance_paise,
              day_edges.opening_balance_paise + entry_total(day_edges));

    const auto future_entry = bms::reporting::get_account_statement(
        db(), "ACC006", "2026-04-01", "2026-04-30");
    EXPECT_TRUE(future_entry.transactions.empty());
    EXPECT_EQ(future_entry.opening_balance_paise, 1000);
    EXPECT_EQ(future_entry.closing_balance_paise, 1000);

    const auto no_ledger = bms::reporting::get_account_statement(
        db(), "ACC008", "2026-04-01", "2026-04-30");
    EXPECT_TRUE(no_ledger.transactions.empty());
    EXPECT_EQ(no_ledger.opening_balance_paise, 12345);
    EXPECT_EQ(no_ledger.closing_balance_paise, 12345);
}

TEST_F(StatementTest, TransferAmountsUseOppositeSignsAndReconcileClosingBalances) {
    const auto debit = bms::reporting::get_account_statement(
        db(), "ACC001", "2026-01-18", "2026-01-18");
    const auto credit = bms::reporting::get_account_statement(
        db(), "ACC004", "2026-01-18", "2026-01-18");

    ASSERT_EQ(debit.transactions.size(), 1);
    ASSERT_EQ(credit.transactions.size(), 1);
    EXPECT_EQ(debit.transactions[0].type, "transfer_debit");
    EXPECT_EQ(credit.transactions[0].type, "transfer_credit");
    EXPECT_EQ(debit.transactions[0].amount_paise, -200000);
    EXPECT_EQ(credit.transactions[0].amount_paise, 200000);
    EXPECT_EQ(debit.closing_balance_paise, debit.opening_balance_paise + entry_total(debit));
    EXPECT_EQ(credit.closing_balance_paise, credit.opening_balance_paise + entry_total(credit));
}

}  // namespace
