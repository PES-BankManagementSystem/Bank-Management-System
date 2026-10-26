// S-08 (#38): session handling and logout (FR-2, SEC-1).
// Random server-side tokens with an expiry, validated on every protected
// request; logout invalidates the token; expired or unknown tokens get 401.

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
#include "support/seeded_database.hpp"

namespace {

using nlohmann::json;
constexpr char kSeedPassword[] = "Password123!";

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        // A protected route like the ones other modules add: it uses the user
        // id and role that authenticate() exposes.
        router_.get("/whoami", [&database](const bms::api::Request& request) {
            const auto principal = bms::auth::authenticate(request, database);
            return bms::api::Response::ok(
                {{"userId", principal.user_id}, {"role", principal.role}});
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind session test server.");
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

class SessionTest : public bms::test::SeededDatabaseTest {
protected:
    std::string token_for(TestServer& server, const std::string& username) {
        auto response = server.client().Post(
            "/auth/login", json({{"username", username}, {"password", kSeedPassword}}).dump(),
            "application/json");
        EXPECT_TRUE(response);
        EXPECT_EQ(response->status, 200) << response->body;
        return json::parse(response->body)["accessToken"];
    }

    static httplib::Headers bearer(const std::string& token) {
        return {{"Authorization", "Bearer " + token}};
    }
};

TEST_F(SessionTest, ValidTokenExposesTheCurrentUserIdAndRole) {
    TestServer server(db());
    auto officer = server.client().Get("/whoami", bearer(token_for(server, "officer1")));
    ASSERT_TRUE(officer);
    ASSERT_EQ(officer->status, 200) << officer->body;
    EXPECT_EQ(json::parse(officer->body)["userId"], 2);
    EXPECT_EQ(json::parse(officer->body)["role"], "bank_officer");

    auto customer = server.client().Get("/whoami", bearer(token_for(server, "customer1")));
    ASSERT_TRUE(customer);
    EXPECT_EQ(json::parse(customer->body)["role"], "customer");
}

TEST_F(SessionTest, MissingMalformedAndUnknownTokensGet401) {
    TestServer server(db());
    const httplib::Headers cases[] = {
        {},
        {{"Authorization", "Bearer"}},
        {{"Authorization", "Basic YWRtaW46UGFzc3dvcmQxMjMh"}},
        {{"Authorization", "Bearer " + std::string(64, 'a')}},
    };
    for (const auto& headers : cases) {
        auto response = server.client().Get("/whoami", headers);
        ASSERT_TRUE(response);
        EXPECT_EQ(response->status, 401);
        EXPECT_EQ(json::parse(response->body)["code"], "UNAUTHENTICATED");
    }
}

TEST_F(SessionTest, LogoutInvalidatesTheTokenOnly) {
    TestServer server(db());
    const std::string first = token_for(server, "officer1");
    const std::string second = token_for(server, "officer1");

    auto logout = server.client().Post("/auth/logout", bearer(first));
    ASSERT_TRUE(logout);
    EXPECT_EQ(logout->status, 204);

    auto after = server.client().Get("/whoami", bearer(first));
    ASSERT_TRUE(after);
    EXPECT_EQ(after->status, 401);
    auto other_session = server.client().Get("/whoami", bearer(second));
    ASSERT_TRUE(other_session);
    EXPECT_EQ(other_session->status, 200);

    auto no_token = server.client().Post("/auth/logout");
    ASSERT_TRUE(no_token);
    EXPECT_EQ(no_token->status, 401);
}

TEST_F(SessionTest, ExpiredTokensGet401) {
    TestServer server(db());
    const std::string token = token_for(server, "admin");
    auto before = server.client().Get("/whoami", bearer(token));
    ASSERT_TRUE(before);
    ASSERT_EQ(before->status, 200);

    bms::auth::expire_all_sessions_for_testing();

    auto after = server.client().Get("/whoami", bearer(token));
    ASSERT_TRUE(after);
    EXPECT_EQ(after->status, 401);
    EXPECT_EQ(json::parse(after->body)["message"], "The session is invalid or has expired.");
}

TEST_F(SessionTest, SessionOfAUserWhoIsNoLongerActiveGets401) {
    TestServer server(db());
    const std::string token = token_for(server, "customer1");
    auto deactivate = db().prepare("UPDATE users SET status = 'inactive' WHERE username = 'customer1'");
    deactivate->step();

    auto response = server.client().Get("/whoami", bearer(token));
    ASSERT_TRUE(response);
    EXPECT_EQ(response->status, 401);
}

TEST_F(SessionTest, AuthenticateCanBeCalledDirectlyByModuleHandlers) {
    TestServer server(db());
    const std::string token = token_for(server, "admin");

    httplib::Request raw;
    raw.headers.emplace("Authorization", "Bearer " + token);
    const bms::api::Request request(raw);
    const bms::auth::Principal principal = bms::auth::authenticate(request, db());
    EXPECT_EQ(principal.user_id, 1);
    EXPECT_EQ(principal.role, "administrator");
    EXPECT_FALSE(principal.must_change_password);

    httplib::Request anonymous;
    EXPECT_THROW((void)bms::auth::authenticate(bms::api::Request(anonymous), db()), bms::Error);
}

}  // namespace
