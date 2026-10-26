// S-27 (#58): Administrators browse the audit trail (NFR-5, TC-16).
// GET /audit is Administrator-only, filterable by date and account, and
// read-only: no route edits or deletes an entry.

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <gtest/gtest.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/router.hpp"
#include "audit/routes.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "support/seeded_database.hpp"
#include "transaction/deposit.hpp"

namespace {

using nlohmann::json;
constexpr char kSeedPassword[] = "Password123!";

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        bms::auth::register_routes(router_, database, true);
        // Same wiring as main.cpp.
        bms::audit::register_routes(router_, database, [&database](const bms::api::Request& request) {
            if (bms::auth::authenticate(request, database).role != "administrator") {
                throw bms::forbidden();
            }
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind audit test server.");
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

class AuditViewerTest : public bms::test::SeededDatabaseTest {
protected:
    httplib::Headers bearer(TestServer& server, const std::string& username) {
        auto response = server.client().Post(
            "/auth/login", json({{"username", username}, {"password", kSeedPassword}}).dump(),
            "application/json");
        EXPECT_TRUE(response);
        EXPECT_EQ(response->status, 200) << response->body;
        return {{"Authorization",
                 "Bearer " + json::parse(response->body)["accessToken"].get<std::string>()}};
    }

    json get(TestServer& server, const std::string& path, int expected_status = 200) {
        auto response = server.client().Get(path, bearer(server, "admin"));
        EXPECT_TRUE(response);
        EXPECT_EQ(response->status, expected_status) << response->body;
        return json::parse(response->body);
    }

    std::int64_t audit_count() {
        auto query = db().prepare("SELECT COUNT(*) FROM audit_log");
        query->step();
        return query->column_int(0);
    }
};

TEST_F(AuditViewerTest, AdministratorSeesEntriesNewestFirstWithOperatorAndAccount) {
    TestServer server(db());
    const auto body = get(server, "/audit");
    const auto& entries = body["entries"];
    ASSERT_GE(entries.size(), 8u);
    EXPECT_GT(entries[0]["logId"].get<int>(), entries[1]["logId"].get<int>());

    const auto it = std::find_if(entries.begin(), entries.end(),
                                 [](const json& e) { return e["logId"] == 6; });
    ASSERT_NE(it, entries.end());
    EXPECT_EQ((*it)["operator"], "officer1");
    EXPECT_EQ((*it)["operatorId"], 2);
    EXPECT_EQ((*it)["action"], "CREATE_ACCOUNT");
    EXPECT_EQ((*it)["accountNumber"], "ACC001");
    EXPECT_EQ((*it)["timestamp"], "2026-01-15 09:00:00");
}

TEST_F(AuditViewerTest, NonAdministratorsAndAnonymousCallersAreRefused) {
    TestServer server(db());
    EXPECT_EQ(server.client().Get("/audit")->status, 401);
    EXPECT_EQ(server.client().Get("/audit", bearer(server, "officer1"))->status, 403);
    EXPECT_EQ(server.client().Get("/audit", bearer(server, "customer1"))->status, 403);
}

TEST_F(AuditViewerTest, FiltersByAccountNumber) {
    TestServer server(db());
    bms::transaction::deposit(db(), "ACC001", "10.00", 2);

    const auto body = get(server, "/audit?account=ACC001");
    ASSERT_EQ(body["entries"].size(), 2u);
    for (const auto& entry : body["entries"]) {
        EXPECT_EQ(entry["entity"], "accounts");
        EXPECT_EQ(entry["accountNumber"], "ACC001");
    }
    EXPECT_EQ(body["entries"][0]["action"], "DEPOSIT");

    EXPECT_EQ(get(server, "/audit?account=NOPE", 404)["code"], "NOT_FOUND");
}

TEST_F(AuditViewerTest, FiltersByInclusiveDateRange) {
    TestServer server(db());
    EXPECT_EQ(get(server, "/audit?from=2026-02-20&to=2026-03-10")["entries"].size(), 2u);
    EXPECT_EQ(get(server, "/audit?from=2026-03-10")["entries"].size(), 1u);
    EXPECT_EQ(get(server, "/audit?to=2026-01-14")["entries"].size(), 5u);
    EXPECT_EQ(get(server, "/audit?from=2026-01-15&to=2026-01-15&account=ACC001")["entries"].size(), 1u);
    EXPECT_EQ(get(server, "/audit?from=2030-01-01")["entries"].size(), 0u);
}

TEST_F(AuditViewerTest, PagesWithLimitAndOffset) {
    TestServer server(db());
    const auto page = get(server, "/audit?limit=3&offset=2");
    ASSERT_EQ(page["entries"].size(), 3u);
    EXPECT_EQ(page["entries"][0]["logId"], 6);
    EXPECT_EQ(page["limit"], 3);
    EXPECT_EQ(page["offset"], 2);
}

TEST_F(AuditViewerTest, RejectsBadFilters) {
    TestServer server(db());
    for (const char* query : {"from=2026-13-01", "from=01-02-2026", "to=2026-02-30", "from=2026-02-02&to=2026-01-01",
                              "limit=0", "limit=501", "limit=abc", "offset=-1"}) {
        const auto body = get(server, std::string("/audit?") + query, 400);
        EXPECT_EQ(body["code"], "VALIDATION_ERROR") << query;
    }
}

TEST_F(AuditViewerTest, NoRouteEditsOrDeletesAnEntry) {
    TestServer server(db());
    const auto headers = bearer(server, "admin");
    const auto before = audit_count();
    const std::string body = json({{"action", "EDITED"}}).dump();

    for (const char* path : {"/audit", "/audit/1"}) {
        for (const auto& result : {server.client().Post(path, headers, body, "application/json"),
                                   server.client().Put(path, headers, body, "application/json"),
                                   server.client().Patch(path, headers, body, "application/json"),
                                   server.client().Delete(path, headers, body, "application/json")}) {
            ASSERT_TRUE(result);
            EXPECT_EQ(result->status, 405) << path;
            EXPECT_EQ(result->get_header_value("Allow"), "GET");
        }
    }
    EXPECT_EQ(audit_count(), before);
}

}  // namespace
