#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "account/account_opening.hpp"
#include "account/account_status.hpp"
#include "account/routes.hpp"
#include "api/router.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
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
        if (port_ <= 0) throw std::runtime_error("Could not bind account-opening test server.");
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

class AccountOpeningTest : public bms::test::SeededDatabaseTest {
protected:
    std::int64_t count(const std::string& sql) {
        auto query = db().prepare(sql);
        EXPECT_TRUE(query->step());
        return query->column_int(0);
    }

    static json valid_body(const std::string& customer_id = "CUS-00000001",
                           std::int64_t type_id = 1,
                           const std::string& amount = "1000.00") {
        return {{"customerId", customer_id},
                {"accountType", type_id},
                {"initialDeposit", amount}};
    }
};

TEST(TC05_AccountNumberGenerator, UsesRandomFormattedUniqueNumbers) {
    std::set<std::string> generated;
    for (int index = 0; index < 2000; ++index) {
        const std::string account_number = bms::account::generate_account_number();
        ASSERT_EQ(account_number.size(), 15U);
        EXPECT_EQ(account_number.substr(0, 3), "ACC");
        EXPECT_TRUE(std::all_of(account_number.begin() + 3, account_number.end(),
                                [](unsigned char ch) {
                                    return ch >= '0' && ch <= '9';
                                }));
        EXPECT_TRUE(generated.insert(account_number).second);
    }
}

TEST_F(AccountOpeningTest, TC05_ForcedNumberCollisionRetriesAndExhaustionRollsBack) {
    db().exec("INSERT INTO accounts "
              "(account_number, customer_id, type_id, status, balance) "
              "VALUES ('ACC000000000001', 1, 1, 'active', 100000)");
    int generated = 0;
    const auto opened = bms::account::open_account(
        db(), {"CUS-00000001", 1, 100000}, 2,
        [&generated] {
            ++generated;
            return generated == 1 ? "ACC000000000001" : "ACC987654321012";
        });
    EXPECT_EQ(generated, 2);
    EXPECT_EQ(opened.account_number, "ACC987654321012");

    const auto accounts_before = count("SELECT COUNT(*) FROM accounts");
    const auto transactions_before = count("SELECT COUNT(*) FROM transactions");
    generated = 0;
    EXPECT_THROW(
        bms::account::open_account(
            db(), {"CUS-00000001", 1, 100000}, 2,
            [&generated] {
                ++generated;
                return "ACC000000000001";
            }),
        std::runtime_error);
    EXPECT_EQ(generated, 5);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), accounts_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions_before);
}

TEST_F(AccountOpeningTest, TC05_OfficerAndAdministratorCreateAtomicOpeningDeposit) {
    const auto account_rows_before = count("SELECT COUNT(*) FROM accounts");
    const auto transaction_rows_before = count("SELECT COUNT(*) FROM transactions");
    const auto audit_rows_before = count(
        "SELECT COUNT(*) FROM audit_log WHERE action = 'ACCOUNT_OPENED'");
    TestServer server(db());
    auto& client = server.client();
    const auto officer = bms::test::sign_in(client, "officer1");
    const auto admin = bms::test::sign_in(client, "admin");
    const auto customer = bms::test::sign_in(client, "customer1");

    auto response = client.Post("/accounts", officer, valid_body().dump(), "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 201);
    const json body = json::parse(response->body);
    const std::string account_number = body["account"]["accountNumber"].get<std::string>();
    EXPECT_EQ(body["account"]["type"], "Savings");
    EXPECT_EQ(body["account"]["status"], "active");
    EXPECT_EQ(body["account"]["balance"], "1000.00");
    EXPECT_EQ(response->get_header_value("Location"), "/accounts/" + account_number);
    EXPECT_EQ(body["transaction"]["type"], "deposit");
    EXPECT_EQ(body["transaction"]["amount"], "1000.00");
    EXPECT_EQ(body["transaction"]["resultingBalance"], "1000.00");
    EXPECT_EQ(body["transaction"]["referenceNumber"],
              "TXN-" + std::string(12 - std::to_string(body["transaction"]["id"].get<std::int64_t>()).size(), '0') +
                  std::to_string(body["transaction"]["id"].get<std::int64_t>()));

    auto linked = db().prepare(
        "SELECT a.customer_id, a.type_id, a.balance, t.transaction_id, t.operator_id, "
        "t.type, t.amount, t.resulting_balance FROM accounts a "
        "JOIN transactions t ON t.account_id = a.account_id "
        "WHERE a.account_number = ?");
    linked->bind(1, account_number);
    ASSERT_TRUE(linked->step());
    EXPECT_EQ(linked->column_int(0), 1);
    EXPECT_EQ(linked->column_int(1), 1);
    EXPECT_EQ(linked->column_int(2), 100000);
    EXPECT_EQ(linked->column_int(3), body["transaction"]["id"].get<std::int64_t>());
    EXPECT_EQ(linked->column_int(4), 2);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(linked->column_text(5))), "deposit");
    EXPECT_EQ(linked->column_int(6), 100000);
    EXPECT_EQ(linked->column_int(7), 100000);
    EXPECT_FALSE(linked->step());

    auto ledger = db().prepare(
        "SELECT COUNT(*), SUM(amount), MAX(resulting_balance) FROM transactions "
        "WHERE account_id = (SELECT account_id FROM accounts WHERE account_number = ?)");
    ledger->bind(1, account_number);
    ASSERT_TRUE(ledger->step());
    EXPECT_EQ(ledger->column_int(0), 1);
    EXPECT_EQ(ledger->column_int(1), 100000);
    EXPECT_EQ(ledger->column_int(2), 100000);

    auto audit = db().prepare(
        "SELECT user_id, entity, action, details FROM audit_log "
        "WHERE action = 'ACCOUNT_OPENED' ORDER BY log_id DESC LIMIT 1");
    ASSERT_TRUE(audit->step());
    EXPECT_EQ(audit->column_int(0), 2);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(1))), "accounts");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(2))), "ACCOUNT_OPENED");
    const std::string details = reinterpret_cast<const char*>(audit->column_text(3));
    EXPECT_NE(details.find("accountNumber=" + account_number), std::string::npos);
    EXPECT_NE(details.find("customerId=CUS-00000001"), std::string::npos);
    EXPECT_NE(details.find("accountType=Savings"), std::string::npos);
    EXPECT_NE(details.find("amountPaise=100000"), std::string::npos);

    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), account_rows_before + 1);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transaction_rows_before + 1);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log WHERE action = 'ACCOUNT_OPENED'"),
              audit_rows_before + 1);

    response = client.Post("/accounts", admin, valid_body().dump(), "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 201);
    EXPECT_NE(json::parse(response->body)["account"]["accountNumber"], account_number);
}

TEST_F(AccountOpeningTest, TC05_BelowMinimumReturnsRequiredMinimumAndChangesNoRows) {
    TestServer server(db());
    const auto officer = bms::test::sign_in(server.client(), "officer1");
    const auto account_rows = count("SELECT COUNT(*) FROM accounts");
    const auto transaction_rows = count("SELECT COUNT(*) FROM transactions");
    const auto audit_rows = count("SELECT COUNT(*) FROM audit_log");

    const auto response = server.client().Post(
        "/accounts", officer,
        valid_body("CUS-00000001", 1, "999.99").dump(), "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 422);
    const json body = json::parse(response->body);
    EXPECT_EQ(body["minimumBalance"], "1000.00");
    EXPECT_NE(body["message"].get<std::string>().find("1000.00"), std::string::npos);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), account_rows);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transaction_rows);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), audit_rows);
}

TEST_F(AccountOpeningTest, TC05_InactiveAndUnknownRecordsAreRejected) {
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");
    const auto inactive = client.Post("/accounts", admin,
        valid_body("CUS-00000001", 1, "1000.00").dump(), "application/json");
    ASSERT_TRUE(inactive);
    ASSERT_EQ(inactive->status, 201);

    db().exec("UPDATE customers SET status = 'inactive' WHERE customer_code = 'CUS-00000002'");
    const auto accounts_before = count("SELECT COUNT(*) FROM accounts");
    const auto transactions_before = count("SELECT COUNT(*) FROM transactions");
    const auto rejected = client.Post(
        "/accounts", admin,
        valid_body("CUS-00000002", 1, "1000.00").dump(), "application/json");
    ASSERT_TRUE(rejected);
    ASSERT_EQ(rejected->status, 409);
    const json rejection = json::parse(rejected->body);
    EXPECT_EQ(rejection["code"], "CUSTOMER_INACTIVE");
    EXPECT_NE(rejection["message"].get<std::string>().find("inactive"), std::string::npos);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), accounts_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions_before);

    const auto unknown_customer = client.Post(
        "/accounts", admin,
        valid_body("CUS-99999999").dump(), "application/json");
    ASSERT_TRUE(unknown_customer);
    EXPECT_EQ(unknown_customer->status, 404);
    EXPECT_NE(json::parse(unknown_customer->body)["message"].get<std::string>()
                  .find("CUS-99999999"),
              std::string::npos);

    const auto unknown_type = client.Post(
        "/accounts", admin,
        valid_body("CUS-00000001", 999, "1000.00").dump(), "application/json");
    ASSERT_TRUE(unknown_type);
    EXPECT_EQ(unknown_type->status, 404);
    EXPECT_EQ(json::parse(unknown_type->body)["field"], "accountType");
}

TEST_F(AccountOpeningTest, TC05_ZeroMinimumAllowsZeroAndRecordsOpeningTransaction) {
    db().exec("INSERT INTO account_types "
              "(type_name, min_balance, interest_rate, daily_withdrawal_limit) "
              "VALUES ('No minimum', 0, 0, 100000)");
    auto type = db().prepare("SELECT type_id FROM account_types WHERE type_name = 'No minimum'");
    ASSERT_TRUE(type->step());
    const std::int64_t type_id = type->column_int(0);

    TestServer server(db());
    const auto admin = bms::test::sign_in(server.client(), "admin");
    const auto response = server.client().Post(
        "/accounts", admin,
        valid_body("CUS-00000001", type_id, "0.00").dump(), "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 201);
    const json body = json::parse(response->body);
    EXPECT_EQ(body["account"]["balance"], "0.00");
    EXPECT_EQ(body["transaction"]["amount"], "0.00");
    auto row = db().prepare(
        "SELECT COUNT(*), SUM(amount), MAX(resulting_balance) FROM transactions "
        "WHERE account_id = (SELECT account_id FROM accounts WHERE account_number = ?)");
    row->bind(1, body["account"]["accountNumber"].get<std::string>());
    ASSERT_TRUE(row->step());
    EXPECT_EQ(row->column_int(0), 1);
    EXPECT_EQ(row->column_int(1), 0);
    EXPECT_EQ(row->column_int(2), 0);
}

TEST_F(AccountOpeningTest, TC05_ValidationReportsAllInvalidFields) {
    TestServer server(db());
    const auto admin = bms::test::sign_in(server.client(), "admin");
    const std::vector<std::string> invalid_amounts = {
        "-1", "1.001", "1e3", "NaN", "Infinity", "92233720368547759"};
    for (const std::string& amount : invalid_amounts) {
        const auto response = server.client().Post(
            "/accounts", admin, valid_body("CUS-00000001", 1, amount).dump(),
            "application/json");
        ASSERT_TRUE(response) << amount;
        EXPECT_EQ(response->status, 400) << amount;
        EXPECT_EQ(json::parse(response->body)["fields"][0]["field"], "initialDeposit")
            << amount;
    }

    json wrong_type = valid_body();
    wrong_type["initialDeposit"] = true;
    auto response = server.client().Post("/accounts", admin, wrong_type.dump(),
                                         "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_EQ(json::parse(response->body)["fields"][0]["field"], "initialDeposit");

    json blank_customer = valid_body(" \t ");
    response = server.client().Post("/accounts", admin, blank_customer.dump(),
                                    "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_EQ(json::parse(response->body)["fields"][0]["field"], "customerId");

    json missing = json::object();
    response = server.client().Post("/accounts", admin, missing.dump(), "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 400);
    const json errors = json::parse(response->body)["fields"];
    EXPECT_EQ(errors.size(), 3U);
    EXPECT_EQ(errors[0]["field"], "customerId");
    EXPECT_EQ(errors[1]["field"], "accountType");
    EXPECT_EQ(errors[2]["field"], "initialDeposit");

    json several = {{"customerId", 42}, {"accountType", "Savings"},
                    {"initialDeposit", "1.009"}, {"unexpected", true}};
    response = server.client().Post("/accounts", admin, several.dump(), "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 400);
    EXPECT_EQ(json::parse(response->body)["fields"].size(), 4U);
}

TEST_F(AccountOpeningTest, TC05_RolesRequireOfficerOrAdministrator) {
    TestServer server(db());
    auto& client = server.client();
    const auto customer = bms::test::sign_in(client, "customer1");
    const auto account_rows = count("SELECT COUNT(*) FROM accounts");
    const auto transaction_rows = count("SELECT COUNT(*) FROM transactions");

    auto response = client.Post("/accounts", customer, valid_body().dump(), "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 403);
    response = client.Post("/accounts", httplib::Headers{}, valid_body().dump(),
                           "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 401);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), account_rows);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transaction_rows);
}

TEST_F(AccountOpeningTest, TC05_TransactionOrAuditFailureRollsBackAllRows) {
    const auto accounts_before = count("SELECT COUNT(*) FROM accounts");
    const auto transactions_before = count("SELECT COUNT(*) FROM transactions");
    const auto audit_before = count("SELECT COUNT(*) FROM audit_log");
    db().exec("CREATE TRIGGER fail_opening_transaction BEFORE INSERT ON transactions "
              "WHEN NEW.type = 'deposit' AND NEW.amount = 123456 "
              "BEGIN SELECT RAISE(ABORT, 'forced transaction insert failure'); END");
    EXPECT_THROW(
        bms::account::open_account(db(), {"CUS-00000001", 1, 123456}, 2,
                                   [] { return "ACC777777777777"; }),
        bms::DbError);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), accounts_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), audit_before);
    db().exec("DROP TRIGGER fail_opening_transaction");

    db().exec("CREATE TRIGGER fail_opening_audit BEFORE INSERT ON audit_log "
              "WHEN NEW.action = 'ACCOUNT_OPENED' "
              "BEGIN SELECT RAISE(ABORT, 'forced audit insert failure'); END");
    EXPECT_THROW(
        bms::account::open_account(db(), {"CUS-00000001", 1, 100000}, 2,
                                   [] { return "ACC888888888888"; }),
        bms::DbError);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), accounts_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), audit_before);
}

TEST_F(AccountOpeningTest, TC05_WriteLockPreventsCustomerDeactivationRace) {
    std::atomic<bool> update_attempting{false};
    std::atomic<bool> update_completed{false};
    std::exception_ptr update_error;
    std::thread deactivator;
    const auto opened = bms::account::open_account(
        db(), {"CUS-00000001", 1, 100000}, 2,
        [this, &update_attempting, &update_completed, &update_error, &deactivator] {
            deactivator = std::thread([this, &update_attempting, &update_completed,
                                       &update_error] {
                try {
                    bms::Database competing(db_path().string());
                    update_attempting.store(true);
                    bms::Database::Transaction transaction(competing, true);
                    competing.exec(
                        "UPDATE customers SET status = 'inactive' "
                        "WHERE customer_code = 'CUS-00000001'");
                    transaction.commit();
                    update_completed.store(true);
                } catch (...) {
                    update_error = std::current_exception();
                }
            });
            while (!update_attempting.load()) std::this_thread::yield();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            EXPECT_FALSE(update_completed.load());
            return "ACC666666666666";
        });
    deactivator.join();
    if (update_error) std::rethrow_exception(update_error);

    EXPECT_EQ(opened.status, "active");
    auto customer = db().prepare(
        "SELECT status FROM customers WHERE customer_code = 'CUS-00000001'");
    ASSERT_TRUE(customer->step());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(customer->column_text(0))), "inactive");
    auto account = db().prepare(
        "SELECT status FROM accounts WHERE account_number = ?");
    account->bind(1, opened.account_number);
    ASSERT_TRUE(account->step());
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(account->column_text(0))), "active");
}

TEST_F(AccountOpeningTest, TC05_OpenedAccountWorksWithExistingTransactionsAndStatusFlow) {
    const auto opened = bms::account::open_account(
        db(), {"CUS-00000001", 1, 100000}, 2,
        [] { return "ACC555555555555"; });
    const auto deposit = bms::transaction::deposit(db(), opened.account_number, "100.00", 2);
    EXPECT_EQ(deposit.resulting_balance_paise, 110000);
    const auto withdrawal =
        bms::transaction::withdraw(db(), opened.account_number, "100.00", 2);
    EXPECT_EQ(withdrawal.resulting_balance_paise, 100000);
    const auto frozen =
        bms::account::change_status(db(), opened.account_number, "freeze", 1);
    EXPECT_EQ(frozen.status, "frozen");
}

}  // namespace
