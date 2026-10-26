// S-09 (#39): lock accounts after three consecutive failed logins, and let an
// Administrator unlock any locked user (TC-02).

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
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "support/seeded_database.hpp"

namespace {

using nlohmann::json;
constexpr char kSeedPassword[] = "Password123!";

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind auth test server.");
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

class LockoutTest : public bms::test::SeededDatabaseTest {
protected:
    httplib::Result login(TestServer& server, const std::string& username,
                          const std::string& password) {
        return server.client().Post(
            "/auth/login", json({{"username", username}, {"password", password}}).dump(),
            "application/json");
    }

    httplib::Headers bearer(TestServer& server, const std::string& username) {
        auto response = login(server, username, kSeedPassword);
        EXPECT_TRUE(response);
        EXPECT_EQ(response->status, 200) << response->body;
        return {{"Authorization",
                 "Bearer " + json::parse(response->body)["accessToken"].get<std::string>()}};
    }

    std::string status_of(const std::string& username) {
        auto query = db().prepare("SELECT status FROM users WHERE username = ?");
        query->bind(1, username);
        EXPECT_TRUE(query->step());
        return reinterpret_cast<const char*>(query->column_text(0));
    }

    std::int64_t failed_attempts_of(const std::string& username) {
        auto query = db().prepare("SELECT failed_attempts FROM users WHERE username = ?");
        query->bind(1, username);
        EXPECT_TRUE(query->step());
        return query->column_int(0);
    }

    std::int64_t audit_count(const std::string& action) {
        auto query = db().prepare("SELECT COUNT(*) FROM audit_log WHERE action = ?");
        query->bind(1, action);
        EXPECT_TRUE(query->step());
        return query->column_int(0);
    }

    void lock(TestServer& server, const std::string& username) {
        for (int i = 0; i < 3; ++i) login(server, username, "wrong-password");
        ASSERT_EQ(status_of(username), "locked");
    }
};

TEST_F(LockoutTest, ThirdConsecutiveFailureLocksTheAccount) {
    TestServer server(db());

    for (int attempt = 1; attempt <= 2; ++attempt) {
        auto failed = login(server, "officer1", "wrong-password");
        ASSERT_TRUE(failed);
        EXPECT_EQ(failed->status, 401);
        EXPECT_EQ(json::parse(failed->body)["code"], "UNAUTHENTICATED");
        EXPECT_EQ(status_of("officer1"), "active");
        EXPECT_EQ(failed_attempts_of("officer1"), attempt);
    }

    auto third = login(server, "officer1", "wrong-password");
    ASSERT_TRUE(third);
    EXPECT_EQ(third->status, 401);
    const json body = json::parse(third->body);
    EXPECT_EQ(body["code"], "ACCOUNT_LOCKED");
    EXPECT_NE(body["message"].get<std::string>().find("Administrator"), std::string::npos);
    EXPECT_EQ(status_of("officer1"), "locked");
    EXPECT_EQ(audit_count("ACCOUNT_LOCKED"), 1);
}

TEST_F(LockoutTest, SuccessfulLoginResetsTheCounter) {
    TestServer server(db());
    login(server, "customer1", "wrong-password");
    login(server, "customer1", "wrong-password");
    ASSERT_EQ(failed_attempts_of("customer1"), 2);

    auto ok = login(server, "customer1", kSeedPassword);
    ASSERT_TRUE(ok);
    ASSERT_EQ(ok->status, 200) << ok->body;
    EXPECT_EQ(failed_attempts_of("customer1"), 0);

    login(server, "customer1", "wrong-password");
    login(server, "customer1", "wrong-password");
    EXPECT_EQ(status_of("customer1"), "active");
}

TEST_F(LockoutTest, LockedAccountIsBlockedEvenWithTheCorrectPassword) {
    TestServer server(db());
    lock(server, "customer1");

    auto blocked = login(server, "customer1", kSeedPassword);
    ASSERT_TRUE(blocked);
    EXPECT_EQ(blocked->status, 401);
    const json body = json::parse(blocked->body);
    EXPECT_EQ(body["code"], "ACCOUNT_LOCKED");
    EXPECT_NE(body["message"].get<std::string>().find("Contact an Administrator"),
              std::string::npos);
    EXPECT_EQ(blocked->body.find("accessToken"), std::string::npos);
}

TEST_F(LockoutTest, AdministratorUnlocksLockedCustomerAndStaff) {
    TestServer server(db());
    lock(server, "customer1");
    lock(server, "officer1");
    const auto admin = bearer(server, "admin");

    auto listed = server.client().Get("/users/locked", admin);
    ASSERT_TRUE(listed);
    ASSERT_EQ(listed->status, 200) << listed->body;
    const json users = json::parse(listed->body)["users"];
    ASSERT_EQ(users.size(), 2U);
    EXPECT_EQ(listed->body.find("password"), std::string::npos);

    for (const char* id : {"4", "2"}) {  // customer1, officer1
        auto unlocked = server.client().Patch(std::string("/users/") + id + "/unlock", admin,
                                              "", "application/json");
        ASSERT_TRUE(unlocked);
        ASSERT_EQ(unlocked->status, 200) << unlocked->body;
        EXPECT_EQ(json::parse(unlocked->body)["status"], "active");
    }

    EXPECT_EQ(status_of("customer1"), "active");
    EXPECT_EQ(failed_attempts_of("officer1"), 0);
    EXPECT_EQ(audit_count("USER_UNLOCKED"), 2);
    auto customer = login(server, "customer1", kSeedPassword);
    ASSERT_TRUE(customer);
    EXPECT_EQ(customer->status, 200);
    auto officer = login(server, "officer1", kSeedPassword);
    ASSERT_TRUE(officer);
    EXPECT_EQ(officer->status, 200);
}

TEST_F(LockoutTest, OnlyAdministratorsCanListOrUnlock) {
    TestServer server(db());
    lock(server, "customer1");

    auto anonymous = server.client().Patch("/users/4/unlock", "", "application/json");
    ASSERT_TRUE(anonymous);
    EXPECT_EQ(anonymous->status, 401);

    const auto officer = bearer(server, "officer1");
    auto forbidden = server.client().Patch("/users/4/unlock", officer, "", "application/json");
    ASSERT_TRUE(forbidden);
    EXPECT_EQ(forbidden->status, 403);
    auto list_forbidden = server.client().Get("/users/locked", officer);
    ASSERT_TRUE(list_forbidden);
    EXPECT_EQ(list_forbidden->status, 403);
    EXPECT_EQ(status_of("customer1"), "locked");
}

TEST_F(LockoutTest, UnlockRejectsUnknownAndNotLockedUsers) {
    TestServer server(db());
    const auto admin = bearer(server, "admin");

    auto unknown = server.client().Patch("/users/999/unlock", admin, "", "application/json");
    ASSERT_TRUE(unknown);
    EXPECT_EQ(unknown->status, 404);
    auto malformed = server.client().Patch("/users/abc/unlock", admin, "", "application/json");
    ASSERT_TRUE(malformed);
    EXPECT_EQ(malformed->status, 404);

    auto not_locked = server.client().Patch("/users/2/unlock", admin, "", "application/json");
    ASSERT_TRUE(not_locked);
    EXPECT_EQ(not_locked->status, 409);
    EXPECT_EQ(json::parse(not_locked->body)["code"], "USER_NOT_LOCKED");
}

}  // namespace
