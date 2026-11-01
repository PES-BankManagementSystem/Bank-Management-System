#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

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

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        bms::reporting::register_routes(router_, database);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind transaction-history test server.");
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

class AccountHistoryTest : public bms::test::SeededDatabaseTest {
protected:
    std::int64_t create_account(const std::string& number, std::int64_t customer_id,
                                std::int64_t balance) {
        auto insert = db().prepare(
            "INSERT INTO accounts (account_number, customer_id, type_id, status, balance) "
            "VALUES (?, ?, 1, 'active', ?)");
        insert->bind(1, number);
        insert->bind(2, customer_id);
        insert->bind(3, balance);
        insert->step();
        auto id = db().prepare("SELECT last_insert_rowid()");
        id->step();
        return id->column_int(0);
    }

    void add_transaction(std::int64_t account_id, const std::string& type,
                         std::int64_t amount, std::int64_t balance,
                         const std::string& created_at, const std::string& transfer_ref = {}) {
        auto insert = db().prepare(
            "INSERT INTO transactions "
            "(account_id, operator_id, type, amount, resulting_balance, created_at, transfer_ref) "
            "VALUES (?, 2, ?, ?, ?, ?, ?)");
        insert->bind(1, account_id);
        insert->bind(2, type);
        insert->bind(3, amount);
        insert->bind(4, balance);
        insert->bind(5, created_at);
        if (transfer_ref.empty()) insert->bind(6, nullptr);
        else insert->bind(6, transfer_ref);
        insert->step();
    }
};

TEST_F(AccountHistoryTest, TC11_CustomerCanReadOwnedTransactionsButNotAnotherAccount) {
    TestServer server(db());
    const auto own = bms::test::sign_in(server.client(), "customer1");
    const auto other = bms::test::sign_in(server.client(), "customer2");

    const auto allowed = server.client().Get("/accounts/ACC001/transactions", own);
    ASSERT_TRUE(allowed);
    ASSERT_EQ(allowed->status, 200) << allowed->body;
    const json body = json::parse(allowed->body);
    EXPECT_EQ(body["accountNumber"], "ACC001");
    EXPECT_GT(body["total"].get<int>(), 0);

    const auto denied = server.client().Get("/accounts/ACC001/transactions", other);
    ASSERT_TRUE(denied);
    EXPECT_EQ(denied->status, 403);
    EXPECT_EQ(json::parse(denied->body)["message"], "You are not allowed to perform this action.");

    const auto nonexistent = server.client().Get("/accounts/ACC999/transactions", other);
    ASSERT_TRUE(nonexistent);
    EXPECT_EQ(nonexistent->status, 403);
    EXPECT_EQ(nonexistent->body, denied->body);

    auto audit = db().prepare(
        "SELECT user_id, action, entity, entity_id, details FROM audit_log "
        "WHERE action = 'VIEW_TRANSACTIONS_DENIED' ORDER BY log_id");
    ASSERT_TRUE(audit->step());
    EXPECT_EQ(audit->column_int(0), 5);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(1))),
              "VIEW_TRANSACTIONS_DENIED");
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(audit->column_text(2))), "accounts");
    EXPECT_NE(std::string(reinterpret_cast<const char*>(audit->column_text(4))).find("ACC001"),
              std::string::npos);
    ASSERT_TRUE(audit->step());
    EXPECT_EQ(audit->column_int(0), 5);
    EXPECT_EQ(audit->column_int(3), 0);
    EXPECT_NE(std::string(reinterpret_cast<const char*>(audit->column_text(4))).find("ACC999"),
              std::string::npos);
    EXPECT_FALSE(audit->step());
}

TEST_F(AccountHistoryTest, TC11_DeniedHistoryIsAuditedEvenWhenAuditInsertFails) {
    TestServer server(db());
    const auto customer = bms::test::sign_in(server.client(), "customer2");
    db().exec(
        "CREATE TRIGGER fail_history_audit BEFORE INSERT ON audit_log "
        "WHEN NEW.action = 'VIEW_TRANSACTIONS_DENIED' "
        "BEGIN SELECT RAISE(ABORT, 'test failure'); END");

    const auto denied = server.client().Get("/accounts/ACC001/transactions", customer);
    ASSERT_TRUE(denied);
    EXPECT_EQ(denied->status, 403);
    auto count = db().prepare(
        "SELECT COUNT(*) FROM audit_log WHERE action = 'VIEW_TRANSACTIONS_DENIED'");
    ASSERT_TRUE(count->step());
    EXPECT_EQ(count->column_int(0), 0);
}

TEST_F(AccountHistoryTest, NewestFirstUsesTransactionIdTieBreakerAndUtcDateTime) {
    const std::int64_t account_id = create_account("ACC006", 1, 1200);
    add_transaction(account_id, "deposit", 500, 500, "2026-03-01 10:00:00");
    add_transaction(account_id, "deposit", 700, 1200, "2026-03-01 10:00:00");
    add_transaction(account_id, "withdrawal", 100, 1100, "2026-02-28 10:00:00");

    const auto history = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, std::nullopt, std::nullopt, std::nullopt, std::nullopt);
    ASSERT_EQ(history.total, 3);
    ASSERT_EQ(history.transactions.size(), 3U);
    EXPECT_GT(history.transactions[0].transaction_id, history.transactions[1].transaction_id);
    EXPECT_EQ(history.transactions[0].resulting_balance_paise, history.current_balance_paise);
    EXPECT_EQ(history.transactions[0].date_time, "2026-03-01T10:00:00Z");
    EXPECT_EQ(history.transactions[0].amount_paise, 700);
    EXPECT_EQ(history.transactions[1].amount_paise, 500);
    EXPECT_EQ(history.transactions[2].amount_paise, -100);
}

TEST_F(AccountHistoryTest, DateFiltersAreInclusiveAndPagingHasStableTotal) {
    const std::int64_t account_id = create_account("ACC006", 1, 400);
    add_transaction(account_id, "deposit", 100, 100, "2026-03-01 00:00:00");
    add_transaction(account_id, "deposit", 100, 200, "2026-03-01 23:59:59");
    add_transaction(account_id, "deposit", 100, 300, "2026-03-02 00:00:00");
    add_transaction(account_id, "deposit", 100, 400, "2026-03-03 23:59:59");

    const auto one_day = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, "2026-03-01", "2026-03-01", "20", "0");
    EXPECT_EQ(one_day.total, 2);
    ASSERT_EQ(one_day.transactions.size(), 2U);
    EXPECT_EQ(one_day.transactions[0].date_time, "2026-03-01T23:59:59Z");
    EXPECT_EQ(one_day.transactions[1].date_time, "2026-03-01T00:00:00Z");

    const auto from_only = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, "2026-03-02", std::nullopt, "20", "0");
    const auto to_only = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, std::nullopt, "2026-03-01", "20", "0");
    EXPECT_EQ(from_only.total, 2);
    EXPECT_EQ(to_only.total, 2);

    const auto first_page = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, std::nullopt, std::nullopt, "2", "0");
    add_transaction(account_id, "deposit", 100, 500, "2026-03-04 10:00:00");
    const auto second_page = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, std::nullopt, std::nullopt, "2", "2",
        std::to_string(first_page.snapshot));
    EXPECT_EQ(first_page.total, 4);
    EXPECT_EQ(second_page.total, 4);
    ASSERT_EQ(first_page.transactions.size(), 2U);
    ASSERT_EQ(second_page.transactions.size(), 2U);
    EXPECT_NE(first_page.transactions[1].transaction_id,
              second_page.transactions[0].transaction_id);

    const auto empty = bms::reporting::get_account_transaction_history(
        db(), "ACC006", std::nullopt, "2030-01-01", "2030-01-02", "20", "0");
    EXPECT_EQ(empty.total, 0);
    EXPECT_TRUE(empty.transactions.empty());
}

TEST_F(AccountHistoryTest, FR10_RejectsMultipleInvalidFiltersTogether) {
    TestServer server(db());
    const auto admin = bms::test::sign_in(server.client(), "admin");
    const auto response = server.client().Get(
        "/accounts/ACC001/transactions?from=2026-02-30&to=not-a-date&limit=abc&offset=-1",
        admin);
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 400) << response->body;
    const json error = json::parse(response->body);
    ASSERT_TRUE(error.contains("fields")) << response->body;
    EXPECT_EQ(error["fields"].size(), 4U);

    const auto reversed = server.client().Get(
        "/accounts/ACC001/transactions?from=2026-03-02&to=2026-03-01", admin);
    ASSERT_TRUE(reversed);
    ASSERT_EQ(reversed->status, 400) << reversed->body;
    EXPECT_EQ(json::parse(reversed->body)["fields"][0]["field"], "from");
}

TEST_F(AccountHistoryTest, StaffAndAdministratorsCanReadAnyAccountAndUnknownAccountIs404) {
    TestServer server(db());
    for (const char* username : {"officer1", "admin"}) {
        const auto staff = bms::test::sign_in(server.client(), username);
        const auto response = server.client().Get("/accounts/ACC002/transactions", staff);
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 200) << response->body;

        const auto missing = server.client().Get("/accounts/ACC999/transactions", staff);
        ASSERT_TRUE(missing);
        EXPECT_EQ(missing->status, 404) << missing->body;
    }
    const auto anonymous = server.client().Get("/accounts/ACC001/transactions");
    ASSERT_TRUE(anonymous);
    EXPECT_EQ(anonymous->status, 401);
}

TEST_F(AccountHistoryTest, FrozenAndClosedAccountsAndTransferCounterpartAreIncluded) {
    TestServer server(db());
    const auto officer = bms::test::sign_in(server.client(), "officer1");
    for (const char* account : {"ACC002", "ACC003"}) {
        const auto response = server.client().Get(
            std::string("/accounts/") + account + "/transactions", officer);
        ASSERT_TRUE(response);
        ASSERT_EQ(response->status, 200) << response->body;
        EXPECT_GE(json::parse(response->body)["total"].get<int>(), 0);
    }

    const auto response = server.client().Get("/accounts/ACC001/transactions", officer);
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200) << response->body;
    const json history = json::parse(response->body);
    bool found_transfer_out = false;
    for (const auto& entry : history["transactions"]) {
        if (entry["type"] == "transfer_debit") {
            found_transfer_out = true;
            EXPECT_EQ(entry["amount"], "-2000.00");
            EXPECT_EQ(entry["counterpartAccount"], "ACC004");
        }
    }
    EXPECT_TRUE(found_transfer_out);
}

TEST_F(AccountHistoryTest, SuccessfulReadsDoNotCreateAuditRows) {
    TestServer server(db());
    const auto customer = bms::test::sign_in(server.client(), "customer1");
    auto before = db().prepare(
        "SELECT COUNT(*) FROM audit_log WHERE action = 'VIEW_TRANSACTIONS_DENIED'");
    ASSERT_TRUE(before->step());
    const auto response = server.client().Get("/accounts/ACC001/transactions", customer);
    ASSERT_TRUE(response);
    ASSERT_EQ(response->status, 200) << response->body;
    auto after = db().prepare(
        "SELECT COUNT(*) FROM audit_log WHERE action = 'VIEW_TRANSACTIONS_DENIED'");
    ASSERT_TRUE(after->step());
    EXPECT_EQ(after->column_int(0), before->column_int(0));
}

TEST_F(AccountHistoryTest, FR10_FormatsOnePaiseLargeAmountsAndEmptyHistory) {
    TestServer server(db());
    const auto officer = bms::test::sign_in(server.client(), "officer1");
    const std::int64_t paise_account = create_account("ACC006", 1, 1);
    add_transaction(paise_account, "deposit", 1, 1, "2026-04-01 00:00:00");
    const std::int64_t large_account = create_account("ACC007", 1, 123456789012345);
    add_transaction(large_account, "deposit", 123456789012345, 123456789012345,
                    "2026-04-02 00:00:00");
    create_account("ACC008", 1, 0);

    const auto one_paise = server.client().Get("/accounts/ACC006/transactions", officer);
    ASSERT_TRUE(one_paise);
    ASSERT_EQ(one_paise->status, 200) << one_paise->body;
    EXPECT_EQ(json::parse(one_paise->body)["transactions"][0]["amount"], "0.01");
    EXPECT_EQ(json::parse(one_paise->body)["transactions"][0]["balanceAfter"], "0.01");

    const auto large = server.client().Get("/accounts/ACC007/transactions", officer);
    ASSERT_TRUE(large);
    ASSERT_EQ(large->status, 200) << large->body;
    EXPECT_EQ(json::parse(large->body)["transactions"][0]["amount"], "1234567890123.45");

    const auto empty = server.client().Get("/accounts/ACC008/transactions", officer);
    ASSERT_TRUE(empty);
    ASSERT_EQ(empty->status, 200) << empty->body;
    EXPECT_EQ(json::parse(empty->body)["total"], 0);
    EXPECT_TRUE(json::parse(empty->body)["transactions"].empty());
}

}  // namespace
