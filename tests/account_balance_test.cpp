#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "account/account_queries.hpp"
#include "account/routes.hpp"
#include "api/router.hpp"
#include "auth/routes.hpp"
#include "common/error.hpp"
#include "customer/records.hpp"
#include "support/login.hpp"
#include "support/seeded_database.hpp"
#include "transaction/deposit.hpp"
#include "transaction/withdraw.hpp"

namespace {

using nlohmann::json;

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        bms::account::register_routes(router_, database);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind account balance test server.");
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

class AccountBalanceTest : public bms::test::SeededDatabaseTest {};

TEST_F(AccountBalanceTest, TC11_FR9_CustomerSeesOnlyTheirSortedAccountsAndStatusTypes) {
    db().exec("UPDATE accounts SET customer_id = 2 WHERE account_number = 'ACC003'");
    TestServer server(db());
    const auto customer1 = bms::test::sign_in(server.client(), "customer1");
    const auto customer2 = bms::test::sign_in(server.client(), "customer2");

    const auto response = server.client().Get(
        "/customers/me/accounts?customerId=CUS-00000002", customer1);
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200) << response->body;
    const json body = json::parse(response->body);
    ASSERT_EQ(body["count"], 2);
    ASSERT_EQ(body["accounts"].size(), 2);
    EXPECT_EQ(body["accounts"][0]["accountNumber"], "ACC001");
    EXPECT_EQ(body["accounts"][0]["accountType"], "Savings");
    EXPECT_EQ(body["accounts"][0]["status"], "active");
    EXPECT_EQ(body["accounts"][0]["balance"], "50000.00");
    EXPECT_EQ(body["accounts"][1]["accountNumber"], "ACC004");
    EXPECT_EQ(body["accounts"][1]["accountType"], "Current");

    const auto other_response =
        server.client().Get("/customers/me/accounts", customer2);
    ASSERT_TRUE(other_response);
    ASSERT_EQ(other_response->status, 200) << other_response->body;
    const json other_body = json::parse(other_response->body);
    EXPECT_EQ(other_body["count"], 3);
    EXPECT_EQ(other_body["accounts"][0]["accountNumber"], "ACC002");
    EXPECT_EQ(other_body["accounts"][0]["status"], "frozen");
    EXPECT_EQ(other_body["accounts"][1]["accountNumber"], "ACC003");
    EXPECT_EQ(other_body["accounts"][1]["status"], "closed");

    const auto officer = bms::test::sign_in(server.client(), "officer1");
    const auto officer_response =
        server.client().Get("/customers/me/accounts", officer);
    ASSERT_TRUE(officer_response);
    EXPECT_EQ(officer_response->status, 403);
    const auto admin = bms::test::sign_in(server.client(), "admin");
    const auto admin_response =
        server.client().Get("/customers/me/accounts", admin);
    ASSERT_TRUE(admin_response);
    EXPECT_EQ(admin_response->status, 403);
    const auto unauthenticated = server.client().Get("/customers/me/accounts");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);
}

TEST_F(AccountBalanceTest, FR9_EmptyAccountListAndMissingCustomerProfileAreDistinct) {
    db().exec("UPDATE accounts SET customer_id = 1 WHERE customer_id = 2");
    TestServer server(db());
    const auto customer2 = bms::test::sign_in(server.client(), "customer2");

    const auto empty = server.client().Get("/customers/me/accounts", customer2);
    ASSERT_TRUE(empty);
    ASSERT_EQ(empty->status, 200) << empty->body;
    EXPECT_EQ(json::parse(empty->body)["count"], 0);
    EXPECT_TRUE(json::parse(empty->body)["accounts"].empty());

    db().exec("UPDATE customers SET user_id = NULL WHERE customer_id = 2");
    const auto missing_profile =
        server.client().Get("/customers/me/accounts", customer2);
    ASSERT_TRUE(missing_profile);
    EXPECT_EQ(missing_profile->status, 404);
    EXPECT_NE(json::parse(missing_profile->body)["message"].get<std::string>().find(
                  "customer profile"),
              std::string::npos);
}

TEST_F(AccountBalanceTest, TC11_FR9_OwnBalanceWorksAndOtherOrMissingAccountsAreIdentical403) {
    db().exec("UPDATE accounts SET balance = 1 WHERE account_number = 'ACC001'");
    db().exec("UPDATE accounts SET balance = 0 WHERE account_number = 'ACC003'");
    TestServer server(db());
    const auto customer1 = bms::test::sign_in(server.client(), "customer1");
    const auto own = server.client().Get("/accounts/ACC001/balance", customer1);
    ASSERT_TRUE(own);
    ASSERT_EQ(own->status, 200) << own->body;
    const json own_body = json::parse(own->body);
    EXPECT_EQ(own_body["accountNumber"], "ACC001");
    EXPECT_EQ(own_body["accountType"], "Savings");
    EXPECT_EQ(own_body["balance"], "0.01");
    EXPECT_EQ(own_body["currency"], "INR");
    EXPECT_EQ(own_body["status"], "active");
    EXPECT_FALSE(own_body["asOf"].get<std::string>().empty());

    const auto other =
        server.client().Get("/accounts/ACC002/balance", customer1);
    const auto missing =
        server.client().Get("/accounts/ACC999/balance", customer1);
    ASSERT_TRUE(other);
    ASSERT_TRUE(missing);
    EXPECT_EQ(other->status, 403);
    EXPECT_EQ(missing->status, 403);
    EXPECT_EQ(json::parse(other->body), json::parse(missing->body));

    const auto customer2 = bms::test::sign_in(server.client(), "customer2");
    const auto frozen =
        server.client().Get("/accounts/ACC002/balance", customer2);
    ASSERT_TRUE(frozen);
    ASSERT_EQ(frozen->status, 200) << frozen->body;
    EXPECT_EQ(json::parse(frozen->body)["status"], "frozen");

    const auto customer3 = bms::test::sign_in(server.client(), "customer3");
    const auto customer_closed =
        server.client().Get("/accounts/ACC003/balance", customer3);
    ASSERT_TRUE(customer_closed);
    ASSERT_EQ(customer_closed->status, 200) << customer_closed->body;
    EXPECT_EQ(json::parse(customer_closed->body)["status"], "closed");

    const auto officer = bms::test::sign_in(server.client(), "officer1");
    const auto closed =
        server.client().Get("/accounts/ACC003/balance", officer);
    ASSERT_TRUE(closed);
    ASSERT_EQ(closed->status, 200) << closed->body;
    EXPECT_EQ(json::parse(closed->body)["status"], "closed");

    const auto admin = bms::test::sign_in(server.client(), "admin");
    const auto admin_access =
        server.client().Get("/accounts/ACC002/balance", admin);
    ASSERT_TRUE(admin_access);
    EXPECT_EQ(admin_access->status, 200) << admin_access->body;
    const auto staff_missing =
        server.client().Get("/accounts/ACC999/balance", admin);
    ASSERT_TRUE(staff_missing);
    EXPECT_EQ(staff_missing->status, 404);
    const auto unauthenticated = server.client().Get("/accounts/ACC001/balance");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);
}

TEST_F(AccountBalanceTest, FR9_BalanceFormattingSupportsZeroAndLargePaiseValues) {
    db().exec("UPDATE accounts SET balance = 0 WHERE account_number = 'ACC003'");
    db().exec(
        "UPDATE accounts SET balance = 9000000000000000000 "
        "WHERE account_number = 'ACC001'");
    TestServer server(db());
    const auto admin = bms::test::sign_in(server.client(), "admin");

    const auto zero = server.client().Get("/accounts/ACC003/balance", admin);
    ASSERT_TRUE(zero);
    ASSERT_EQ(zero->status, 200) << zero->body;
    EXPECT_EQ(json::parse(zero->body)["balance"], "0.00");

    const auto large = server.client().Get("/accounts/ACC001/balance", admin);
    ASSERT_TRUE(large);
    ASSERT_EQ(large->status, 200) << large->body;
    EXPECT_EQ(json::parse(large->body)["balance"], "90000000000000000.00");
}

TEST_F(AccountBalanceTest, FR9_BalanceMatchesStoredLedgerResultAfterDepositAndWithdrawal) {
    db().exec("UPDATE accounts SET balance = 4700000 WHERE account_number = 'ACC001'");
    bms::transaction::deposit(db(), "ACC001", "0.01", 2);
    bms::transaction::withdraw(db(), "ACC001", "0.01", 2);

    auto latest = db().prepare(
        "SELECT a.balance, t.resulting_balance FROM accounts a "
        "JOIN transactions t ON t.account_id = a.account_id "
        "WHERE a.account_number = 'ACC001' "
        "ORDER BY t.transaction_id DESC LIMIT 1");
    ASSERT_TRUE(latest->step());
    EXPECT_EQ(latest->column_int(0), latest->column_int(1));
    const auto balance = bms::account::get_account_balance(db(), "ACC001");
    EXPECT_EQ(balance.balance_paise, latest->column_int(0));
}

TEST_F(AccountBalanceTest, FR9_DeactivatedCustomerCannotLogIn) {
    bms::customer::deactivate_customer(db(), "CUS-00000001", 1);
    TestServer server(db());
    const auto response = server.client().Post(
        "/auth/login",
        R"({"username":"customer1","password":"Password123!"})",
        "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 401);
}

}  // namespace
