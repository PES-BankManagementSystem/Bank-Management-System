// S-24 (#56): Administrators create, update and deactivate Bank Officer
// accounts (FR-13, TC-13).

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/router.hpp"
#include "auth/password.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "staff/routes.hpp"
#include "support/seeded_database.hpp"

namespace {

using nlohmann::json;
constexpr char kSeedPassword[] = "Password123!";

class TestServer {
public:
    TestServer(bms::Database& database, bool allow_delivery) : router_(server_) {
        bms::auth::register_routes(router_, database, allow_delivery);
        bms::staff::register_routes(router_, database, allow_delivery);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind staff test server.");
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

class StaffTest : public bms::test::SeededDatabaseTest {
protected:
    httplib::Result login(TestServer& server, const std::string& username,
                          const std::string& password) {
        return server.client().Post(
            "/auth/login", json({{"username", username}, {"password", password}}).dump(),
            "application/json");
    }

    httplib::Headers bearer(TestServer& server, const std::string& username,
                            const std::string& password = kSeedPassword) {
        auto response = login(server, username, password);
        EXPECT_TRUE(response);
        EXPECT_EQ(response->status, 200) << response->body;
        return {{"Authorization",
                 "Bearer " + json::parse(response->body)["accessToken"].get<std::string>()}};
    }

    std::int64_t scalar(const std::string& sql) {
        auto query = db().prepare(sql);
        EXPECT_TRUE(query->step());
        return query->column_int(0);
    }
};

TEST_F(StaffTest, AdministratorCreatesOfficerWithOneTimeInitialCredentials) {
    TestServer server(db(), true);
    const auto admin = bearer(server, "admin");

    auto created = server.client().Post("/staff", admin, R"({"username":"officer.priya"})",
                                        "application/json");
    ASSERT_TRUE(created);
    ASSERT_EQ(created->status, 201) << created->body;
    const json body = json::parse(created->body);
    EXPECT_EQ(body["username"], "officer.priya");
    EXPECT_EQ(body["role"], "bank_officer");
    EXPECT_EQ(body["status"], "active");
    const std::string temporary = body["login"]["temporaryPassword"];
    EXPECT_EQ(temporary.size(), 20U);
    EXPECT_EQ(created->get_header_value("Location"),
              "/staff/" + std::to_string(body["userId"].get<std::int64_t>()));

    // Only the hash is stored, and a password change is forced at first login.
    auto stored = db().prepare(
        "SELECT password_hash, must_change_password FROM users WHERE username = 'officer.priya'");
    ASSERT_TRUE(stored->step());
    const std::string hash = reinterpret_cast<const char*>(stored->column_text(0));
    EXPECT_EQ(hash.find(temporary), std::string::npos);
    EXPECT_TRUE(bms::auth::verify_password(temporary, hash));
    EXPECT_EQ(stored->column_int(1), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM audit_log WHERE action = 'STAFF_CREATED'"), 1);

    auto first_login = login(server, "officer.priya", temporary);
    ASSERT_TRUE(first_login);
    ASSERT_EQ(first_login->status, 200) << first_login->body;
    EXPECT_EQ(json::parse(first_login->body)["role"], "bank_officer");
    EXPECT_TRUE(json::parse(first_login->body)["mustChangePassword"]);

    // The listing never shows credentials again.
    auto listed = server.client().Get("/staff", admin);
    ASSERT_TRUE(listed);
    EXPECT_EQ(listed->body.find(temporary), std::string::npos);
    EXPECT_EQ(listed->body.find("password"), std::string::npos);
}

TEST_F(StaffTest, ListShowsOnlyBankOfficers) {
    TestServer server(db(), true);
    auto listed = server.client().Get("/staff", bearer(server, "admin"));
    ASSERT_TRUE(listed);
    ASSERT_EQ(listed->status, 200) << listed->body;
    const json officers = json::parse(listed->body)["officers"];
    ASSERT_EQ(officers.size(), 2U);
    EXPECT_EQ(officers[0]["username"], "officer1");
    EXPECT_EQ(officers[1]["username"], "officer2");
}

TEST_F(StaffTest, StaffRoutesAreAdministratorOnly) {
    TestServer server(db(), true);
    auto anonymous = server.client().Get("/staff");
    ASSERT_TRUE(anonymous);
    EXPECT_EQ(anonymous->status, 401);

    for (const char* user : {"officer1", "customer1"}) {
        const auto headers = bearer(server, user);
        auto list = server.client().Get("/staff", headers);
        ASSERT_TRUE(list);
        EXPECT_EQ(list->status, 403) << user;
        auto create = server.client().Post("/staff", headers, R"({"username":"sneaky"})",
                                           "application/json");
        ASSERT_TRUE(create);
        EXPECT_EQ(create->status, 403) << user;
        auto update = server.client().Patch("/staff/3", headers, R"({"status":"inactive"})",
                                            "application/json");
        ASSERT_TRUE(update);
        EXPECT_EQ(update->status, 403) << user;
    }
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE username = 'sneaky'"), 0);
}

TEST_F(StaffTest, InvalidOrDuplicateUsernamesAreRejected) {
    TestServer server(db(), true);
    const auto admin = bearer(server, "admin");

    for (const char* payload : {R"({})", R"({"username":"ab"})", R"({"username":"Officer"})",
                                R"({"username":"1officer"})", R"({"username":"has space"})",
                                R"({"username":42})", R"({"username":"ok.name","role":"administrator"})"}) {
        auto rejected = server.client().Post("/staff", admin, payload, "application/json");
        ASSERT_TRUE(rejected);
        EXPECT_EQ(rejected->status, 400) << payload;
    }

    auto duplicate = server.client().Post("/staff", admin, R"({"username":"officer1"})",
                                          "application/json");
    ASSERT_TRUE(duplicate);
    EXPECT_EQ(duplicate->status, 409);
    EXPECT_EQ(json::parse(duplicate->body)["field"], "username");
}

TEST_F(StaffTest, DeactivatedOfficerCannotLogInAndLosesTheirSession) {
    TestServer server(db(), true);
    const auto admin = bearer(server, "admin");
    const auto officer = bearer(server, "officer1");

    auto before = server.client().Get("/staff", officer);
    ASSERT_TRUE(before);
    EXPECT_EQ(before->status, 403);  // authenticated, but not an Administrator

    auto deactivated = server.client().Patch("/staff/2", admin, R"({"status":"inactive"})",
                                             "application/json");
    ASSERT_TRUE(deactivated);
    ASSERT_EQ(deactivated->status, 200) << deactivated->body;
    EXPECT_EQ(json::parse(deactivated->body)["status"], "inactive");
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM audit_log WHERE action = 'STAFF_DEACTIVATED'"), 1);

    auto after = server.client().Get("/staff", officer);
    ASSERT_TRUE(after);
    EXPECT_EQ(after->status, 401);  // the existing session no longer works

    auto relogin = login(server, "officer1", kSeedPassword);
    ASSERT_TRUE(relogin);
    EXPECT_EQ(relogin->status, 401);
}

TEST_F(StaffTest, DeactivatedOfficerIdStaysOnPastTransactionsAndAuditEntries) {
    const auto transactions_before =
        scalar("SELECT COUNT(*) FROM transactions WHERE operator_id = 2");
    ASSERT_GT(transactions_before, 0);

    TestServer server(db(), true);
    auto deactivated = server.client().Patch("/staff/2", bearer(server, "admin"),
                                             R"({"status":"inactive"})", "application/json");
    ASSERT_TRUE(deactivated);
    ASSERT_EQ(deactivated->status, 200);

    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE user_id = 2"), 1);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM transactions WHERE operator_id = 2"),
              transactions_before);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM audit_log WHERE entity = 'users' AND entity_id = 2"),
              2);  // seeded CREATE_USER entry + STAFF_DEACTIVATED
}

TEST_F(StaffTest, AdministratorRenamesAndReactivatesAnOfficer) {
    TestServer server(db(), true);
    const auto admin = bearer(server, "admin");

    auto deactivated = server.client().Patch("/staff/3", admin, R"({"status":"inactive"})",
                                             "application/json");
    ASSERT_TRUE(deactivated);
    ASSERT_EQ(deactivated->status, 200);

    auto updated = server.client().Patch(
        "/staff/3", admin, R"({"username":"officer.two","status":"active"})", "application/json");
    ASSERT_TRUE(updated);
    ASSERT_EQ(updated->status, 200) << updated->body;
    const json body = json::parse(updated->body);
    EXPECT_EQ(body["username"], "officer.two");
    EXPECT_EQ(body["status"], "active");

    auto relogin = login(server, "officer.two", kSeedPassword);
    ASSERT_TRUE(relogin);
    EXPECT_EQ(relogin->status, 200);

    auto taken = server.client().Patch("/staff/3", admin, R"({"username":"officer1"})",
                                       "application/json");
    ASSERT_TRUE(taken);
    EXPECT_EQ(taken->status, 409);
    auto bad_status = server.client().Patch("/staff/3", admin, R"({"status":"locked"})",
                                            "application/json");
    ASSERT_TRUE(bad_status);
    EXPECT_EQ(bad_status->status, 400);
    auto empty = server.client().Patch("/staff/3", admin, R"({})", "application/json");
    ASSERT_TRUE(empty);
    EXPECT_EQ(empty->status, 400);
}

TEST_F(StaffTest, OnlyBankOfficersCanBeManaged) {
    TestServer server(db(), true);
    const auto admin = bearer(server, "admin");
    for (const char* path : {"/staff/1", "/staff/4", "/staff/999", "/staff/abc"}) {
        auto response = server.client().Patch(path, admin, R"({"status":"inactive"})",
                                              "application/json");
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 404) << path;
    }
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE status = 'inactive'"), 0);
}

TEST_F(StaffTest, CreatingStaffIsDisabledOnNetworkListeners) {
    TestServer server(db(), false);
    auto response = server.client().Post("/staff", R"({"username":"officer.net"})",
                                         "application/json");
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 401);
    EXPECT_EQ(scalar("SELECT COUNT(*) FROM users WHERE username = 'officer.net'"), 0);
}

}  // namespace
