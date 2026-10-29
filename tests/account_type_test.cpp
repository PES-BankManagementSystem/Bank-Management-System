#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "account/account_types.hpp"
#include "account/routes.hpp"
#include "api/router.hpp"
#include "auth/routes.hpp"
#include "common/error.hpp"
#include "support/login.hpp"
#include "support/seeded_database.hpp"
#include "transaction/withdraw.hpp"

namespace {

using nlohmann::json;

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        bms::account::register_routes(router_, database);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind account-type test server.");
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

class AccountTypeTest : public bms::test::SeededDatabaseTest {
protected:
    static json valid_input() {
        return {{"name", "Student"}, {"minimumBalance", 250.50},
                {"interestRate", 5.25}, {"dailyWithdrawalLimit", 5000.00}};
    }

    static bool has_field(const json& response, const std::string& field) {
        if (response.value("field", "") == field) return true;
        if (!response.contains("fields") || !response["fields"].is_array()) return false;
        for (const json& error : response["fields"]) {
            if (error.value("field", "") == field) return true;
        }
        return false;
    }

    std::int64_t count(const std::string& sql) {
        auto query = db().prepare(sql);
        EXPECT_TRUE(query->step());
        return query->column_int(0);
    }
};

TEST_F(AccountTypeTest, S14_ListCreateAndUpdateTypes) {
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");

    const auto list = client.Get("/account-types", admin);
    ASSERT_TRUE(list);
    ASSERT_EQ(list->status, 200);
    const json initial = json::parse(list->body);
    ASSERT_EQ(initial["accountTypes"].size(), 2);
    EXPECT_EQ(initial["accountTypes"][0]["name"], "Savings");
    EXPECT_EQ(initial["accountTypes"][0]["minimumBalance"], "1000.00");
    EXPECT_EQ(initial["accountTypes"][0]["interestRate"], "4.00");
    EXPECT_EQ(initial["accountTypes"][0]["dailyWithdrawalLimit"], "50000.00");

    const auto created = client.Post("/account-types", admin, valid_input().dump(), "application/json");
    ASSERT_TRUE(created);
    ASSERT_EQ(created->status, 201);
    const json created_body = json::parse(created->body);
    const std::int64_t id = created_body["accountType"]["typeId"].get<std::int64_t>();
    EXPECT_EQ(created_body["accountType"]["name"], "Student");
    EXPECT_EQ(created_body["accountType"]["minimumBalance"], "250.50");
    EXPECT_EQ(created_body["accountType"]["interestRate"], "5.25");
    EXPECT_EQ(created_body["accountType"]["dailyWithdrawalLimit"], "5000.00");
    EXPECT_EQ(created->get_header_value("Location"), "/account-types/" + std::to_string(id));
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log WHERE entity = 'account_types' "
                    "AND action = 'ACCOUNT_TYPE_CREATED'"), 1);

    json update = valid_input();
    update["name"] = "Student Plus";
    update["minimumBalance"] = 1000.25;
    update["interestRate"] = 6.00;
    update["dailyWithdrawalLimit"] = 10000.00;
    const auto changed = client.Put("/account-types/" + std::to_string(id), admin,
                                    update.dump(), "application/json");
    ASSERT_TRUE(changed);
    ASSERT_EQ(changed->status, 200);
    const json updated_body = json::parse(changed->body);
    EXPECT_EQ(updated_body["accountType"]["name"], "Student Plus");
    EXPECT_EQ(updated_body["accountType"]["minimumBalance"], "1000.25");
    EXPECT_EQ(updated_body["accountType"]["interestRate"], "6.00");
    EXPECT_EQ(updated_body["accountType"]["dailyWithdrawalLimit"], "10000.00");
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log WHERE entity = 'account_types' "
                    "AND action = 'ACCOUNT_TYPE_UPDATED'"), 1);
    auto audit = db().prepare(
        "SELECT details FROM audit_log WHERE entity = 'account_types' "
        "AND action = 'ACCOUNT_TYPE_UPDATED'");
    ASSERT_TRUE(audit->step());
    const std::string audit_details = reinterpret_cast<const char*>(audit->column_text(0));
    EXPECT_NE(audit_details.find("\"name\":{\"new\":\"Student Plus\",\"old\":\"Student\"}"),
              std::string::npos);
    EXPECT_NE(audit_details.find("\"minimumBalancePaise\":{\"new\":100025,\"old\":25050}"),
              std::string::npos);

    const auto not_found = client.Put("/account-types/9999", admin, valid_input().dump(),
                                      "application/json");
    ASSERT_TRUE(not_found);
    EXPECT_EQ(not_found->status, 404);
    const std::string not_found_message =
        json::parse(not_found->body)["message"].get<std::string>();
    EXPECT_NE(not_found_message.find("9999"), std::string::npos);
}

TEST_F(AccountTypeTest, S14_ValidationRejectsInvalidFieldsTogether) {
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");
    const auto submit = [&client, &admin](const json& body) {
        return client.Post("/account-types", admin, body.dump(), "application/json");
    };

    const json complete = valid_input();
    for (const std::string field : {"name", "minimumBalance", "interestRate",
                                    "dailyWithdrawalLimit"}) {
        json missing = complete;
        missing.erase(field);
        const auto response = submit(missing);
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 400) << field;
        EXPECT_TRUE(has_field(json::parse(response->body), field)) << field;
    }

    const std::vector<std::pair<std::string, json>> invalid_values = {
        {"name", ""}, {"name", "   "}, {"name", std::string(51, 'x')},
        {"minimumBalance", "1.00"}, {"minimumBalance", -1.00},
        {"minimumBalance", 10000000.01}, {"minimumBalance", 1.001},
        {"interestRate", "1.00"}, {"interestRate", -0.01},
        {"interestRate", 20.01}, {"interestRate", 0.001},
        {"dailyWithdrawalLimit", "1.00"}, {"dailyWithdrawalLimit", -1.00},
        {"dailyWithdrawalLimit", 10000000.01}, {"dailyWithdrawalLimit", 1.001},
        {"minimumBalance", "NaN"}, {"interestRate", "Infinity"},
    };
    for (const auto& test_case : invalid_values) {
        json input = complete;
        input[test_case.first] = test_case.second;
        const auto response = submit(input);
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 400) << test_case.first << " " << test_case.second;
        EXPECT_TRUE(has_field(json::parse(response->body), test_case.first)) << test_case.first;
    }

    const auto exponent = client.Post(
        "/account-types", admin,
        R"({"name":"Exponent","minimumBalance":1e2,"interestRate":1,"dailyWithdrawalLimit":10})",
        "application/json");
    ASSERT_TRUE(exponent);
    EXPECT_EQ(exponent->status, 400);
    EXPECT_TRUE(has_field(json::parse(exponent->body), "minimumBalance"));

    const auto multiple = submit(json::object());
    ASSERT_TRUE(multiple);
    ASSERT_EQ(multiple->status, 400);
    const json multiple_body = json::parse(multiple->body);
    EXPECT_GE(multiple_body["fields"].size(), 4);

    json above_interest = complete;
    above_interest["interestRate"] = 20.00;
    above_interest["minimumBalance"] = 10000000.00;
    above_interest["dailyWithdrawalLimit"] = 10000000.00;
    const auto maximum = submit(above_interest);
    ASSERT_TRUE(maximum);
    EXPECT_EQ(maximum->status, 201);

    json zero = complete;
    zero["name"] = "Zero";
    zero["minimumBalance"] = 0;
    zero["interestRate"] = 0;
    zero["dailyWithdrawalLimit"] = 0;
    const auto zero_allowed = submit(zero);
    ASSERT_TRUE(zero_allowed);
    EXPECT_EQ(zero_allowed->status, 201);

    json unknown = complete;
    unknown["unexpected"] = 1;
    const auto unknown_response = submit(unknown);
    ASSERT_TRUE(unknown_response);
    EXPECT_EQ(unknown_response->status, 400);
    EXPECT_TRUE(has_field(json::parse(unknown_response->body), "unexpected"));

    json immutable = complete;
    immutable["typeId"] = 1;
    const auto immutable_response = client.Put("/account-types/1", admin, immutable.dump(),
                                               "application/json");
    ASSERT_TRUE(immutable_response);
    EXPECT_EQ(immutable_response->status, 400);
    EXPECT_TRUE(has_field(json::parse(immutable_response->body), "typeId"));
}

TEST_F(AccountTypeTest, S14_NamesAreUniqueIgnoringCaseAndMayKeepTheirOwnName) {
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");

    json duplicate = valid_input();
    duplicate["name"] = "sAVINGS";
    const auto conflict = client.Post("/account-types", admin, duplicate.dump(), "application/json");
    ASSERT_TRUE(conflict);
    ASSERT_EQ(conflict->status, 409);
    EXPECT_TRUE(has_field(json::parse(conflict->body), "name"));

    json own_name = {
        {"name", "Savings"}, {"minimumBalance", 1000.00}, {"interestRate", 4.00},
        {"dailyWithdrawalLimit", 50000.00},
    };
    const auto updated = client.Put("/account-types/1", admin, own_name.dump(),
                                    "application/json");
    ASSERT_TRUE(updated);
    EXPECT_EQ(updated->status, 200);
}

TEST_F(AccountTypeTest, S14_AuditFailureRollsBackCreateAndUpdate) {
    const std::int64_t initial_count = count("SELECT COUNT(*) FROM account_types");
    db().exec(
        "CREATE TRIGGER fail_account_type_audit BEFORE INSERT ON audit_log "
        "WHEN NEW.entity = 'account_types' "
        "BEGIN SELECT RAISE(ABORT, 'forced audit failure'); END");
    const bms::account::AccountTypeInput input{"Student", 100, 100, 1000};

    EXPECT_THROW(bms::account::create_account_type(db(), input, 1), bms::DbError);
    EXPECT_EQ(count("SELECT COUNT(*) FROM account_types"), initial_count);
    EXPECT_THROW(bms::account::update_account_type(
                     db(), 1, {"Updated Savings", 200, 200, 2000}, 1),
                 bms::DbError);
    auto type = db().prepare("SELECT type_name FROM account_types WHERE type_id = 1");
    ASSERT_TRUE(type->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(type->column_text(0)), "Savings");
}

TEST_F(AccountTypeTest, S14_UpdatesAffectLaterWithdrawalsWithoutChangingPastRows) {
    const auto before = bms::transaction::withdraw(db(), "ACC001", "10.00", 1);
    auto balance_before_update = db().prepare(
        "SELECT balance FROM accounts WHERE account_number = 'ACC001'");
    ASSERT_TRUE(balance_before_update->step());
    const std::int64_t saved_balance = balance_before_update->column_int(0);
    auto saved = db().prepare(
        "SELECT transaction_id, account_id, operator_id, type, amount, resulting_balance, "
        "created_at, transfer_ref, interest_run_id FROM transactions WHERE transaction_id = ?");
    saved->bind(1, before.transaction_id);
    ASSERT_TRUE(saved->step());
    const json original = {
        saved->column_int(0), saved->column_int(1), saved->column_int(2),
        reinterpret_cast<const char*>(saved->column_text(3)), saved->column_int(4),
        saved->column_int(5), reinterpret_cast<const char*>(saved->column_text(6)),
        nullptr, nullptr,
    };

    const bms::account::AccountTypeInput new_rules{"Savings", 100000, 400, 1000};
    EXPECT_NO_THROW(bms::account::update_account_type(db(), 1, new_rules, 1));
    auto balance_after_update = db().prepare(
        "SELECT balance FROM accounts WHERE account_number = 'ACC001'");
    ASSERT_TRUE(balance_after_update->step());
    EXPECT_EQ(balance_after_update->column_int(0), saved_balance);
    try {
        bms::transaction::withdraw(db(), "ACC001", "0.01", 1);
        FAIL() << "The updated daily limit should reject the later withdrawal.";
    } catch (const bms::Error& error) {
        EXPECT_EQ(error.code(), "DAILY_LIMIT_EXCEEDED");
    }

    auto unchanged = db().prepare(
        "SELECT transaction_id, account_id, operator_id, type, amount, resulting_balance, "
        "created_at, transfer_ref, interest_run_id FROM transactions WHERE transaction_id = ?");
    unchanged->bind(1, before.transaction_id);
    ASSERT_TRUE(unchanged->step());
    const json after = {
        unchanged->column_int(0), unchanged->column_int(1), unchanged->column_int(2),
        reinterpret_cast<const char*>(unchanged->column_text(3)), unchanged->column_int(4),
        unchanged->column_int(5), reinterpret_cast<const char*>(unchanged->column_text(6)),
        nullptr, nullptr,
    };
    EXPECT_EQ(after, original);
}

TEST_F(AccountTypeTest, S14_DeleteIsNotRegistered) {
    TestServer server(db());
    auto& client = server.client();
    const auto admin = bms::test::sign_in(client, "admin");
    const auto response = client.Delete("/account-types/1", admin);
    ASSERT_TRUE(response);
    EXPECT_TRUE(response->status == 404 || response->status == 405);
}

}  // namespace
