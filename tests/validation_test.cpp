#include <array>
#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "auth/routes.hpp"
#include "common/config.hpp"
#include "common/error.hpp"
#include "customer/records.hpp"
#include "customer/routes.hpp"
#include "support/login.hpp"
#include "support/seeded_database.hpp"
#include "transaction/deposit.hpp"
#include "transaction/routes.hpp"
#include "transaction/transfer.hpp"
#include "transaction/withdraw.hpp"

namespace {
using Json = nlohmann::json;

struct Snapshot {
    std::array<std::int64_t, 3> balances{};
    std::int64_t transactions = 0;
    bool operator==(const Snapshot& other) const {
        return balances == other.balances && transactions == other.transactions;
    }
};

class TransactionValidationTest : public bms::test::SeededDatabaseTest {
protected:
    void SetUp() override {
        SeededDatabaseTest::SetUp();
        bms::Config config;
        config.host = "127.0.0.1";
        config.port = 0;
        server_ = std::make_unique<bms::api::Server>(config);
        bms::auth::register_routes(server_->router(), db(), true);
        bms::customer::register_routes(server_->router(), db(), true);
        bms::transaction::register_routes(server_->router(), db());
        port_ = server_->bind();
        server_thread_ = std::thread([this] { server_->run(); });
        server_->wait_until_ready();
        client_ = std::make_unique<httplib::Client>("127.0.0.1", port_);
        client_->set_connection_timeout(5);
        client_->set_read_timeout(5);
        officer_ = bms::test::sign_in(*client_, "officer1");
    }

    void TearDown() override {
        client_.reset();
        if (server_) server_->stop();
        if (server_thread_.joinable()) server_thread_.join();
        server_.reset();
        SeededDatabaseTest::TearDown();
    }

    Snapshot snapshot() {
        Snapshot result;
        const std::array<const char*, 3> numbers = {"ACC001", "ACC004", "ACC005"};
        for (std::size_t i = 0; i < numbers.size(); ++i) {
            auto query = db().prepare("SELECT balance FROM accounts WHERE account_number = ?");
            query->bind(1, numbers[i]);
            EXPECT_TRUE(query->step());
            result.balances[i] = query->column_int(0);
        }
        auto count = db().prepare("SELECT COUNT(*) FROM transactions");
        EXPECT_TRUE(count->step());
        result.transactions = count->column_int(0);
        return result;
    }

    void expect_rejection(const httplib::Result& response) {
        ASSERT_TRUE(response);
        EXPECT_GE(response->status, 400) << response->body;
        EXPECT_LT(response->status, 500) << response->body;
        const Json error = Json::parse(response->body, nullptr, false);
        ASSERT_TRUE(error.is_object()) << response->body;
        EXPECT_TRUE(error.contains("code") && error["code"].is_string()) << response->body;
        EXPECT_TRUE(error.contains("message") && error["message"].is_string()) << response->body;
    }

    httplib::Result post(const std::string& path, const Json& body) {
        return client_->Post(path, officer_, body.dump(), "application/json");
    }

    std::unique_ptr<bms::api::Server> server_;
    std::unique_ptr<httplib::Client> client_;
    std::thread server_thread_;
    httplib::Headers officer_;
    int port_ = 0;
};

TEST_F(TransactionValidationTest, CoreRejectsBadInputsWithoutChangingState) {
    const Snapshot before = snapshot();
    for (const std::string& account : std::vector<std::string>{
             "", "   ", "ACC/001", "<script>", "ACC001' OR 1=1 --", std::string(33, 'A')}) {
        EXPECT_THROW(bms::transaction::deposit(db(), account, "1.00", 2), bms::Error);
        EXPECT_THROW(bms::transaction::withdraw(db(), account, "1.00", 2), bms::Error);
        EXPECT_THROW(bms::transaction::transfer(db(), account, "ACC004", "1.00", 2), bms::Error);
    }
    for (const std::string& amount : {"", "   ", "abc", "1e3", "NaN", "Infinity",
                                      "-1.00", "0", "1.001", "999999999999999999999999999"}) {
        EXPECT_THROW(bms::transaction::deposit(db(), "ACC001", amount, 2), bms::Error);
        EXPECT_THROW(bms::transaction::withdraw(db(), "ACC001", amount, 2), bms::Error);
        EXPECT_THROW(bms::transaction::transfer(db(), "ACC001", "ACC004", amount, 2), bms::Error);
    }
    EXPECT_EQ(snapshot(), before);
}

TEST_F(TransactionValidationTest, CustomerSearchLimitsAreCheckedAtBothBoundaries) {
    const std::string overlong_query(101, 'x');
    EXPECT_THROW(bms::customer::search_customers(db(), overlong_query, 20, 0), bms::Error);
    const auto response = client_->Get("/customers?query=" + overlong_query, officer_);
    expect_rejection(response);
}

TEST_F(TransactionValidationTest, DeterministicFuzzRequestsDoNotCrashOrMutateState) {
    const Snapshot before = snapshot();
    const std::vector<Json> amounts = {
        "", "   ", "-1", "0", "1.001", "abc", "1e3", "NaN", "Infinity", "-Infinity",
        "💸", std::string(4096, '9'), 0, -1, 1.5, true, false, nullptr,
        Json::array({"1.00"}), Json{{"amount", "1.00"}},
    };
    const std::vector<std::string> accounts = {
        "", " ", "\tACC001", "ACC001 ", "ACC/001", "ACC.001", "<script>alert(1)</script>",
        "ACC001' OR '1'='1", "💸", std::string(4096, 'A'), "UNKNOWN", "ACC999",
    };
    std::mt19937 random(0x640064U);
    for (int attempt = 0; attempt < 300; ++attempt) {
        const Json amount = amounts[random() % amounts.size()];
        expect_rejection(post("/transactions", Json{{"type", "deposit"},
            {"account_number", "ACC001"}, {"amount", amount}}));
        expect_rejection(post("/transactions", Json{{"type", "withdraw"},
            {"account_number", "ACC001"}, {"amount", amount}}));
        expect_rejection(post("/transfers", Json{{"from_account_number", "ACC001"},
            {"to_account_number", "ACC004"}, {"amount", amount}}));

        const std::string account = accounts[random() % accounts.size()];
        expect_rejection(post("/transactions", Json{{"type", "deposit"},
            {"account_number", account}, {"amount", "1.00"}}));
        expect_rejection(post("/transactions", Json{{"type", "withdraw"},
            {"account_number", account}, {"amount", "1.00"}}));
        expect_rejection(post("/transfers", Json{{"from_account_number", "ACC001"},
            {"to_account_number", account}, {"amount", "1.00"}}));
    }
    expect_rejection(post("/transactions", Json::object()));
    expect_rejection(post("/transfers", Json::object()));
    expect_rejection(post("/transactions", Json{{"type", "deposit"},
        {"account_number", "ACC999"}, {"amount", "1.00"}}));
    expect_rejection(post("/transfers", Json{{"from_account_number", "ACC999"},
        {"to_account_number", "ACC004"}, {"amount", "1.00"}}));
    expect_rejection(post("/transfers", Json{{"from_account_number", "ACC001"},
        {"to_account_number", "ACC999"}, {"amount", "1.00"}}));
    expect_rejection(post("/transfers", Json{{"from_account_number", "ACC001"},
        {"to_account_number", "ACC001"}, {"amount", "1.00"}}));
    for (const std::string& path : {"/transactions", "/transfers"}) {
        expect_rejection(client_->Post(path, officer_, "{not-json", "application/json"));
    }
    EXPECT_EQ(snapshot(), before);
}

}  // namespace
