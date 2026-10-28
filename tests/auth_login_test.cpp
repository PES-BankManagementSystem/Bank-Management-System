// S-07 (#37): log in with user ID and password (FR-1, NFR-4, SEC-2, TC-01).
// Covers valid login for every role, wrong password, unknown user and
// inactive user, and checks that passwords never leak.

#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/router.hpp"
#include "auth/password.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/log.hpp"
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

class LoginTest : public bms::test::SeededDatabaseTest {
protected:
    httplib::Result login(TestServer& server, const std::string& username,
                          const std::string& password) {
        return server.client().Post(
            "/auth/login", json({{"username", username}, {"password", password}}).dump(),
            "application/json");
    }
};

TEST_F(LoginTest, ValidLoginReturnsSessionTokenAndRoleForEveryRole) {
    TestServer server(db());
    const std::vector<std::pair<std::string, std::string>> users = {
        {"admin", "administrator"}, {"officer1", "bank_officer"}, {"customer1", "customer"}};
    std::set<std::string> tokens;
    for (const auto& [username, role] : users) {
        auto response = login(server, username, kSeedPassword);
        ASSERT_TRUE(response);
        ASSERT_EQ(response->status, 200) << response->body;
        const json body = json::parse(response->body);
        EXPECT_EQ(body["role"], role);
        EXPECT_EQ(body["tokenType"], "Bearer");
        const std::string token = body["accessToken"];
        EXPECT_EQ(token.size(), 64U);  // 32 random bytes, hex encoded
        EXPECT_EQ(token.find_first_not_of("0123456789abcdef"), std::string::npos);
        tokens.insert(token);
        EXPECT_EQ(response->body.find(kSeedPassword), std::string::npos);
        EXPECT_EQ(response->body.find("password_hash"), std::string::npos);
    }
    EXPECT_EQ(tokens.size(), users.size());

    // A second login for the same user gets a different token.
    auto again = login(server, "admin", kSeedPassword);
    ASSERT_TRUE(again);
    EXPECT_EQ(tokens.count(json::parse(again->body)["accessToken"]), 0U);
}

TEST_F(LoginTest, WrongPasswordUnknownUserAndInactiveUserGetTheSameGenericError) {
    auto deactivate = db().prepare("UPDATE users SET status = 'inactive' WHERE username = 'officer2'");
    deactivate->step();
    TestServer server(db());

    auto wrong_password = login(server, "officer1", "not-the-password");
    auto unknown_user = login(server, "nobody", kSeedPassword);
    auto inactive_user = login(server, "officer2", kSeedPassword);
    for (const auto* response : {&wrong_password, &unknown_user, &inactive_user}) {
        ASSERT_TRUE(*response);
        EXPECT_EQ((*response)->status, 401);
    }
    // Identical bodies, so the reason (unknown user vs wrong password vs
    // inactive) is not revealed.
    EXPECT_EQ(wrong_password->body, unknown_user->body);
    EXPECT_EQ(wrong_password->body, inactive_user->body);
    const json body = json::parse(wrong_password->body);
    EXPECT_EQ(body["code"], "UNAUTHENTICATED");
    EXPECT_EQ(body["message"], "Invalid username or password.");
    EXPECT_FALSE(body.contains("accessToken"));
}

TEST_F(LoginTest, MissingOrMalformedCredentialsAreRejectedGenerically) {
    TestServer server(db());
    for (const char* payload :
         {R"({})", R"({"username":"admin"})", R"({"password":"x"})",
          R"({"username":"","password":""})", R"({"username":1,"password":2})"}) {
        auto response = server.client().Post("/auth/login", payload, "application/json");
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 401) << payload;
    }
    auto not_json = server.client().Post("/auth/login", "username=admin", "text/plain");
    ASSERT_TRUE(not_json);
    EXPECT_EQ(not_json->status, 400);
}

TEST_F(LoginTest, StoredPasswordsAreSaltedArgon2idHashes) {
    auto query = db().prepare("SELECT password_hash FROM users");
    std::set<std::string> hashes;
    while (query->step()) {
        const std::string hash = reinterpret_cast<const char*>(query->column_text(0));
        EXPECT_EQ(hash.rfind("$argon2id$", 0), 0U);
        EXPECT_EQ(hash.find(kSeedPassword), std::string::npos);
        hashes.insert(hash);
    }
    EXPECT_GT(hashes.size(), 1U);  // same password, different salts

    const std::string first = bms::auth::hash_password("Same-Password-1");
    const std::string second = bms::auth::hash_password("Same-Password-1");
    EXPECT_NE(first, second);
    EXPECT_TRUE(bms::auth::verify_password("Same-Password-1", first));
    EXPECT_FALSE(bms::auth::verify_password("same-password-1", first));
}

TEST_F(LoginTest, PasswordsNeverAppearInServerLogs) {
    std::mutex mutex;
    std::vector<std::string> lines;
    auto previous = bms::log::set_sink([&](bms::log::Level, std::string_view line) {
        std::lock_guard<std::mutex> lock(mutex);
        lines.emplace_back(line);
    });
    {
        TestServer server(db());
        login(server, "admin", kSeedPassword);
        login(server, "admin", "Secret-Wrong-Password");
        login(server, "nobody", "Another-Secret");
    }
    bms::log::set_sink(previous);
    for (const std::string& line : lines) {
        EXPECT_EQ(line.find(kSeedPassword), std::string::npos) << line;
        EXPECT_EQ(line.find("Secret-Wrong-Password"), std::string::npos) << line;
        EXPECT_EQ(line.find("Another-Secret"), std::string::npos) << line;
    }
}

}  // namespace
