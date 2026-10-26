// S-28 (#59): log review. Drives the flows that handle secrets (sign-in with a
// wrong and a right password, one-time initial passwords, password change,
// bearer tokens) through a running server and checks that none of them ever
// reaches the log (NFR-4, SEC-2).

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "auth/routes.hpp"
#include "common/config.hpp"
#include "common/log.hpp"
#include "customer/routes.hpp"
#include "staff/routes.hpp"
#include "support/seeded_database.hpp"

namespace {

using nlohmann::json;

class LogCapture {
public:
    LogCapture() {
        previous_ = bms::log::set_sink([this](bms::log::Level, std::string_view line) {
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.emplace_back(line);
        });
    }
    ~LogCapture() { bms::log::set_sink(previous_); }

    std::string text() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string all;
        for (const auto& line : lines_) all += line + "\n";
        return all;
    }

private:
    std::mutex mutex_;
    std::vector<std::string> lines_;
    bms::log::Sink previous_;
};

class LogSecretsTest : public bms::test::SeededDatabaseTest {};

TEST_F(LogSecretsTest, PasswordsAndTokensNeverReachTheLog) {
    LogCapture capture;
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    bms::api::Server server(config);
    bms::auth::register_routes(server.router(), db(), true);
    bms::customer::register_routes(server.router(), db(), true);
    bms::staff::register_routes(server.router(), db(), true);
    const int port = server.bind();
    std::thread thread([&server] { server.run(); });
    struct Stopper {
        bms::api::Server& server;
        std::thread& thread;
        ~Stopper() {
            server.stop();
            if (thread.joinable()) thread.join();
        }
    } stopper{server, thread};
    server.wait_until_ready();
    httplib::Client client("127.0.0.1", port);

    auto login = [&](const std::string& user, const std::string& password) {
        return client.Post("/auth/login", json({{"username", user}, {"password", password}}).dump(),
                           "application/json");
    };
    const std::string wrong_password = "WrongPass!987-secret";
    const std::string seed_password = "Password123!";

    std::vector<std::string> secrets = {wrong_password, seed_password};

    ASSERT_EQ(login("officer1", wrong_password)->status, 401);
    const auto admin_login = login("admin", seed_password);
    ASSERT_EQ(admin_login->status, 200);
    const std::string admin_token = json::parse(admin_login->body)["accessToken"];
    secrets.push_back(admin_token);
    const httplib::Headers admin{{"Authorization", "Bearer " + admin_token}};

    // One-time initial passwords for a new officer and a new customer.
    const auto staff = client.Post("/staff", admin, R"({"username":"new.officer"})", "application/json");
    ASSERT_EQ(staff->status, 201) << staff->body;
    const std::string officer_password =
        json::parse(staff->body)["login"]["temporaryPassword"];
    secrets.push_back(officer_password);

    const auto customer = client.Post(
        "/customers", admin,
        R"({"name":"Rahul Sharma","dateOfBirth":"1998-04-12","address":"10 Main Street",)"
        R"("phone":"+91 98765-43210","identificationReference":"ID-RAHUL-001","email":"rahul@example.com"})",
        "application/json");
    ASSERT_EQ(customer->status, 201) << customer->body;
    secrets.push_back(json::parse(customer->body)["login"]["temporaryPassword"]);

    // The new officer signs in with the temporary password and chooses a new one.
    const auto officer_login = login("new.officer", officer_password);
    ASSERT_EQ(officer_login->status, 200) << officer_login->body;
    const std::string officer_token = json::parse(officer_login->body)["accessToken"];
    secrets.push_back(officer_token);
    const std::string new_password = "My-New-Passphrase-42";
    secrets.push_back(new_password);
    const auto change = client.Post("/auth/change-password",
                                    {{"Authorization", "Bearer " + officer_token}},
                                    json({{"newPassword", new_password}}).dump(), "application/json");
    ASSERT_EQ(change->status, 200) << change->body;

    server.stop();
    thread.join();

    const std::string log = capture.text();
    ASSERT_FALSE(log.empty());
    EXPECT_NE(log.find("POST /auth/login"), std::string::npos);  // the access log is working
    for (const auto& secret : secrets) {
        ASSERT_FALSE(secret.empty());
        EXPECT_EQ(log.find(secret), std::string::npos) << "a secret reached the log";
    }
}

}  // namespace
