#include <chrono>
#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "account/account_status.hpp"
#include "account/routes.hpp"
#include "api/router.hpp"
#include "auth/routes.hpp"
#include "common/error.hpp"
#include "customer/routes.hpp"
#include "reporting/routes.hpp"
#include "support/login.hpp"
#include "support/seeded_database.hpp"
#include "transaction/deposit.hpp"
#include "transaction/transfer.hpp"
#include "transaction/withdraw.hpp"

namespace {

using nlohmann::json;

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        bms::customer::register_routes(router_, database, true);
        bms::account::register_routes(router_, database);
        bms::reporting::register_routes(router_, database);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind account-status test server.");
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

class AccountStatusTest : public bms::test::SeededDatabaseTest {
protected:
    std::int64_t count(const std::string& sql) {
        auto query = db().prepare(sql);
        EXPECT_TRUE(query->step());
        return query->column_int(0);
    }

};

TEST(AccountStatusStateMachine, FR5_TransitionsAreTableDriven) {
    const std::vector<std::tuple<std::string, std::string, std::string>> transitions = {
        {"active", "freeze", "frozen"},
        {"active", "unfreeze", "active"},
        {"active", "close", "closed"},
        {"frozen", "freeze", "frozen"},
        {"frozen", "unfreeze", "active"},
        {"frozen", "close", "closed"},
        {"closed", "freeze", ""},
        {"closed", "unfreeze", ""},
        {"closed", "close", "closed"},
    };
    for (const auto& [current, action, expected] : transitions) {
        if (expected.empty()) {
            try {
                (void)bms::account::transition_status(current, action, "ACC001");
                FAIL() << current << " / " << action << " should conflict";
            } catch (const bms::Error& error) {
                EXPECT_EQ(error.kind(), bms::ErrorKind::Conflict) << current << " / " << action;
            }
            continue;
        }
        EXPECT_EQ(bms::account::transition_status(current, action, "ACC001"), expected)
            << current << " / " << action;
    }
    EXPECT_THROW(bms::account::transition_status("active", "reopen", "ACC001"),
                 bms::Error);
}

TEST_F(AccountStatusTest, FR5_StatusEndpointEnforcesRolesAndValidatesRequests) {
    const auto original_account_rows = count("SELECT COUNT(*) FROM accounts");
    const auto original_transaction_rows = count("SELECT COUNT(*) FROM transactions");
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");
    const auto officer = bms::test::sign_in(client, "officer1");
    const auto customer = bms::test::sign_in(client, "customer1");

    auto response = client.Patch("/accounts/MISSING/status", admin,
                                 R"({"action":"freeze"})", "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 404);

    response = client.Patch("/accounts/ACC001/status", admin, R"({"action":"invalid"})",
                            "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_EQ(json::parse(response->body)["fields"][0]["field"], "action");

    response = client.Patch("/accounts/ACC001/status", admin, R"({})",
                            "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 400);
    EXPECT_EQ(json::parse(response->body)["fields"][0]["field"], "action");

    response = client.Patch("/accounts/ACC001/status", httplib::Headers{},
                            R"({"action":"freeze"})", "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 401);

    response = client.Patch("/accounts/ACC001/status", customer,
                            R"({"action":"freeze"})", "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 403);

    response = client.Patch("/accounts/ACC001/status", officer,
                            R"({"action":"freeze"})", "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200);
    EXPECT_EQ(json::parse(response->body)["account"]["status"], "frozen");
    auto audit = db().prepare(
        "SELECT user_id, details, timestamp FROM audit_log "
        "WHERE action = 'ACCOUNT_STATUS_CHANGED' ORDER BY log_id DESC LIMIT 1");
    ASSERT_TRUE(audit->step());
    EXPECT_EQ(audit->column_int(0), 2);
    const std::string audit_details =
        reinterpret_cast<const char*>(audit->column_text(1));
    EXPECT_NE(audit_details.find("accountNumber=ACC001"), std::string::npos);
    EXPECT_NE(audit_details.find("oldStatus=active"), std::string::npos);
    EXPECT_NE(audit_details.find("newStatus=frozen"), std::string::npos);
    EXPECT_NE(std::string(reinterpret_cast<const char*>(audit->column_text(2))).size(), 0U);

    const auto audit_count = count(
        "SELECT COUNT(*) FROM audit_log WHERE action = 'ACCOUNT_STATUS_CHANGED'");
    response = client.Patch("/accounts/ACC001/status", admin,
                            R"({"action":"freeze"})", "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 200);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log WHERE action = 'ACCOUNT_STATUS_CHANGED'"),
              audit_count);

    response = client.Patch("/accounts/ACC001/status", admin,
                            R"({"action":"unfreeze"})", "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200);
    EXPECT_EQ(json::parse(response->body)["account"]["status"], "active");

    db().exec("UPDATE accounts SET balance = 0 WHERE account_number = 'ACC001'");
    response = client.Patch("/accounts/ACC001/status", admin,
                            R"({"action":"close"})", "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200);
    EXPECT_EQ(json::parse(response->body)["account"]["status"], "closed");
    const auto account_rows = count("SELECT COUNT(*) FROM accounts");
    const auto transaction_rows = count("SELECT COUNT(*) FROM transactions");
    EXPECT_EQ(account_rows, original_account_rows);
    EXPECT_EQ(transaction_rows, original_transaction_rows);

    response = client.Patch("/accounts/ACC001/status", admin,
                            R"({"action":"freeze"})", "application/json");
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 409);
    const std::string transition_message = json::parse(response->body)["message"];
    EXPECT_NE(transition_message.find("closed"), std::string::npos);
    EXPECT_NE(transition_message.find("freeze"), std::string::npos);

    const auto customer_session = bms::test::sign_in(client, "customer1");
    const auto profile = client.Get("/customers/me", customer_session);
    ASSERT_TRUE(profile);
    ASSERT_EQ(profile->status, 200);
    bool found_closed_account = false;
    const json profile_body = json::parse(profile->body);
    for (const auto& account : profile_body["accounts"]) {
        if (account["accountNumber"].get<std::string>() == "ACC001") {
            found_closed_account = true;
            EXPECT_EQ(account["status"], "closed");
        }
    }
    EXPECT_TRUE(found_closed_account) << profile->body;

    const auto statement = client.Get(
        "/accounts/ACC001/statement?from=2026-01-01&to=2026-12-31", customer_session);
    ASSERT_TRUE(statement);
    EXPECT_EQ(statement->status, 200);
    EXPECT_FALSE(json::parse(statement->body)["transactions"].empty());
    const auto officer_statement = client.Get(
        "/accounts/ACC001/statement?from=2026-01-01&to=2026-12-31", officer);
    ASSERT_TRUE(officer_statement);
    EXPECT_EQ(officer_statement->status, 200);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), account_rows);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transaction_rows);

    response = client.Delete("/accounts/ACC001");
    ASSERT_TRUE(response);
    EXPECT_TRUE(response->status == 404 || response->status == 405);
}

TEST_F(AccountStatusTest, SRS55_CloseRequiresExactZeroMinorUnitBalance) {
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");
    db().exec("UPDATE accounts SET balance = 1 WHERE account_number = 'ACC001'");

    const auto account_rows = count("SELECT COUNT(*) FROM accounts");
    const auto transaction_rows = count("SELECT COUNT(*) FROM transactions");
    const auto audit_rows = count("SELECT COUNT(*) FROM audit_log");
    const auto response = client.Patch("/accounts/ACC001/status", admin,
                                        R"({"action":"close"})", "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 409);
    const std::string message = json::parse(response->body)["message"];
    EXPECT_NE(message.find("0.01"), std::string::npos);
    EXPECT_NE(message.find("Withdraw or transfer"), std::string::npos);
    EXPECT_EQ(count("SELECT balance FROM accounts WHERE account_number = 'ACC001'"), 1);
    EXPECT_EQ(count("SELECT status = 'active' FROM accounts WHERE account_number = 'ACC001'"), 1);
    EXPECT_EQ(count("SELECT COUNT(*) FROM accounts"), account_rows);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transaction_rows);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), audit_rows);
}

TEST_F(AccountStatusTest, FR5_ZeroBalanceFrozenAccountCanBeClosed) {
    EXPECT_THROW(bms::account::change_status(db(), "ACC002", "close", 1), bms::Error);
    EXPECT_EQ(count("SELECT status = 'frozen' FROM accounts WHERE account_number = 'ACC002'"), 1);
    db().exec("UPDATE accounts SET balance = 0 WHERE account_number = 'ACC002'");
    const auto closed = bms::account::change_status(db(), "ACC002", "close", 1);
    EXPECT_EQ(closed.status, "closed");
    EXPECT_EQ(closed.balance_paise, 0);
}

TEST_F(AccountStatusTest, SRS55_AtomicCloseCannotRaceWithDeposit) {
    db().exec("UPDATE accounts SET balance = 0 WHERE account_number = 'ACC001'");
    bms::Database deposit_database(db_path().string());
    bms::Database::Transaction deposit_transaction(deposit_database, true);
    auto update = deposit_database.prepare(
        "UPDATE accounts SET balance = 1 WHERE account_number = 'ACC001'");
    update->step();
    auto record = deposit_database.prepare(
        "INSERT INTO transactions (account_id, operator_id, type, amount, resulting_balance) "
        "SELECT account_id, 2, 'deposit', 1, 1 FROM accounts WHERE account_number = 'ACC001'");
    record->step();

    std::atomic<bool> close_started{false};
    std::atomic<bool> close_conflicted{false};
    std::thread closer([&] {
        close_started.store(true);
        try {
            (void)bms::account::change_status(db(), "ACC001", "close", 1);
        } catch (const bms::Error& error) {
            close_conflicted.store(error.kind() == bms::ErrorKind::Conflict);
        } catch (...) {
        }
    });
    while (!close_started.load()) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    deposit_transaction.commit();
    closer.join();

    EXPECT_TRUE(close_conflicted.load());
    EXPECT_EQ(count("SELECT balance FROM accounts WHERE account_number = 'ACC001'"), 1);
    EXPECT_EQ(count("SELECT status = 'active' FROM accounts WHERE account_number = 'ACC001'"), 1);
}

TEST_F(AccountStatusTest, FR5_StatusAndAuditChangeRollBackTogether) {
    db().exec("CREATE TRIGGER fail_account_status_audit BEFORE INSERT ON audit_log "
              "WHEN NEW.action = 'ACCOUNT_STATUS_CHANGED' "
              "BEGIN SELECT RAISE(ABORT, 'forced audit failure'); END");
    EXPECT_THROW(bms::account::change_status(db(), "ACC001", "freeze", 2),
                 bms::DbError);
    EXPECT_EQ(count("SELECT status = 'active' FROM accounts WHERE account_number = 'ACC001'"), 1);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log WHERE action = 'ACCOUNT_STATUS_CHANGED'"), 0);
}

TEST_F(AccountStatusTest, FR5_FrozenAndClosedAccountsRejectEveryTransactionPath) {
    const auto transactions = count("SELECT COUNT(*) FROM transactions");
    const auto frozen_balance = count(
        "SELECT balance FROM accounts WHERE account_number = 'ACC002'");
    const auto closed_balance = count(
        "SELECT balance FROM accounts WHERE account_number = 'ACC003'");

    EXPECT_THROW(bms::transaction::deposit(db(), "ACC002", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::withdraw(db(), "ACC002", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::transfer(db(), "ACC002", "ACC001", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::transfer(db(), "ACC001", "ACC002", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::deposit(db(), "ACC003", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::withdraw(db(), "ACC003", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::transfer(db(), "ACC003", "ACC001", "1.00", 2), bms::Error);
    EXPECT_THROW(bms::transaction::transfer(db(), "ACC001", "ACC003", "1.00", 2), bms::Error);

    EXPECT_EQ(count("SELECT balance FROM accounts WHERE account_number = 'ACC002'"), frozen_balance);
    EXPECT_EQ(count("SELECT balance FROM accounts WHERE account_number = 'ACC003'"), closed_balance);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions);

    bms::account::change_status(db(), "ACC002", "unfreeze", 1);
    EXPECT_NO_THROW(bms::transaction::deposit(db(), "ACC002", "1.00", 2));
    EXPECT_NO_THROW(bms::transaction::withdraw(db(), "ACC002", "1.00", 2));
    EXPECT_NO_THROW(bms::transaction::transfer(db(), "ACC002", "ACC001", "1.00", 2));
    EXPECT_NO_THROW(bms::transaction::transfer(db(), "ACC001", "ACC002", "1.00", 2));
}

}  // namespace
