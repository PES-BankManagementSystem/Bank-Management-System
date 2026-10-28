// S-10 (#40): role-based access control enforced in the back-end core (FR-1,
// NFR-4, SEC-1, SRS 5.5). Forbidden requests are sent straight to the API
// without the UI and must get 403; refusals are recorded in the audit trail.

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
#include "auth/rbac.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "customer/registration.hpp"
#include "customer/routes.hpp"
#include "reporting/interest.hpp"
#include "reporting/routes.hpp"
#include "staff/routes.hpp"
#include "support/login.hpp"
#include "support/seeded_database.hpp"
#include "transaction/deposit.hpp"
#include "transaction/routes.hpp"
#include "transaction/transfer.hpp"
#include "transaction/withdraw.hpp"

namespace {

using bms::auth::rbac::Operation;
using nlohmann::json;

constexpr std::int64_t kAdmin = 1;
constexpr std::int64_t kOfficer = 2;
constexpr std::int64_t kCustomer1 = 4;  // owns customer 1: ACC001 and ACC004

class TestServer {
public:
    explicit TestServer(bms::Database& database) : router_(server_) {
        // Same wiring as main.cpp.
        bms::auth::register_routes(router_, database, true);
        bms::customer::register_routes(router_, database, true);
        bms::transaction::register_routes(router_, database);
        bms::reporting::register_routes(router_, database);
        bms::staff::register_routes(router_, database, true);
        bms::audit::register_routes(router_, database, [&database](const bms::api::Request& request) {
            bms::auth::rbac::require(request, database, Operation::ViewAudit);
        });
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind RBAC test server.");
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

class RbacTest : public bms::test::SeededDatabaseTest {
protected:
    std::int64_t count(const std::string& sql) {
        auto query = db().prepare(sql);
        query->step();
        return query->column_int(0);
    }

    std::int64_t denials_for(std::int64_t user_id) {
        return count("SELECT COUNT(*) FROM audit_log WHERE action = 'ACCESS_DENIED' AND user_id = " +
                     std::to_string(user_id));
    }

    // True when the call is refused with a 403 bms::Error.
    template <typename Call>
    bool forbidden(Call&& call) {
        try {
            call();
        } catch (const bms::Error& error) {
            return error.kind() == bms::ErrorKind::Forbidden;
        }
        return false;
    }
};

TEST(PermissionTable, MatchesTheBusinessRules) {
    const std::string admin = "administrator", officer = "bank_officer", customer = "customer";

    for (const Operation op : {Operation::RegisterCustomer, Operation::UpdateCustomer,
                               Operation::DeactivateCustomer, Operation::OpenAccount,
                               Operation::CloseAccount, Operation::FreezeAccount,
                               Operation::Deposit, Operation::Withdraw, Operation::Transfer}) {
        EXPECT_TRUE(bms::auth::rbac::is_allowed(op, admin)) << bms::auth::rbac::name(op);
        EXPECT_TRUE(bms::auth::rbac::is_allowed(op, officer)) << bms::auth::rbac::name(op);
        EXPECT_FALSE(bms::auth::rbac::is_allowed(op, customer)) << bms::auth::rbac::name(op);
    }
    for (const Operation op : {Operation::ViewCustomer, Operation::ViewAccount}) {
        EXPECT_TRUE(bms::auth::rbac::is_allowed(op, admin));
        EXPECT_TRUE(bms::auth::rbac::is_allowed(op, officer));
        EXPECT_TRUE(bms::auth::rbac::is_allowed(op, customer));
    }
    for (const Operation op : {Operation::RunInterest, Operation::ManageStaff,
                               Operation::ManageAccountTypes, Operation::UnlockUser,
                               Operation::ViewAudit}) {
        EXPECT_TRUE(bms::auth::rbac::is_allowed(op, admin)) << bms::auth::rbac::name(op);
        EXPECT_FALSE(bms::auth::rbac::is_allowed(op, officer)) << bms::auth::rbac::name(op);
        EXPECT_FALSE(bms::auth::rbac::is_allowed(op, customer)) << bms::auth::rbac::name(op);
    }
    EXPECT_FALSE(bms::auth::rbac::is_allowed(Operation::Deposit, ""));
    EXPECT_FALSE(bms::auth::rbac::is_allowed(Operation::Deposit, "superuser"));
}

TEST_F(RbacTest, CoreRefusesACustomerBeforeAnythingChanges) {
    const auto balance = count("SELECT balance FROM accounts WHERE account_number = 'ACC001'");
    const auto transactions = count("SELECT COUNT(*) FROM transactions");

    EXPECT_TRUE(forbidden([&] { bms::transaction::deposit(db(), "ACC001", "10.00", kCustomer1); }));
    EXPECT_TRUE(forbidden([&] { bms::transaction::withdraw(db(), "ACC001", "10.00", kCustomer1); }));
    EXPECT_TRUE(forbidden([&] { bms::transaction::transfer(db(), "ACC001", "ACC004", "10.00", kCustomer1); }));
    EXPECT_TRUE(forbidden([&] { bms::customer::register_customer(db(), {}, kCustomer1); }));

    EXPECT_EQ(count("SELECT balance FROM accounts WHERE account_number = 'ACC001'"), balance);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions);
    EXPECT_EQ(denials_for(kCustomer1), 4);
}

TEST_F(RbacTest, CoreRefusesAnOfficerTheAdministratorInterestRun) {
    EXPECT_TRUE(forbidden([&] { bms::reporting::run_interest(db(), "2026-04", kOfficer); }));
    EXPECT_EQ(count("SELECT COUNT(*) FROM interest_runs WHERE period = '2026-04'"), 0);
    EXPECT_EQ(denials_for(kOfficer), 1);
}

TEST_F(RbacTest, CoreRefusesAnUnknownUserAndAllowsStaff) {
    EXPECT_TRUE(forbidden([&] { bms::transaction::deposit(db(), "ACC001", "10.00", 9999); }));
    EXPECT_NO_THROW(bms::transaction::deposit(db(), "ACC001", "10.00", kOfficer));
    EXPECT_NO_THROW(bms::transaction::deposit(db(), "ACC001", "10.00", kAdmin));
    EXPECT_NO_THROW(bms::reporting::run_interest(db(), "2026-04", kAdmin));
}

TEST_F(RbacTest, ForbiddenRequestsToTheApiGet403AndAreRecorded) {
    TestServer server(db());
    auto& client = server.client();
    const auto customer = bms::test::sign_in(client, "customer1");
    const auto officer = bms::test::sign_in(client, "officer1");
    const auto balance = count("SELECT balance FROM accounts WHERE account_number = 'ACC001'");
    const std::string deposit = R"({"type":"deposit","account_number":"ACC001","amount":"10.00"})";
    const std::string transfer = R"({"from_account_number":"ACC001","to_account_number":"ACC004","amount":"10.00"})";

    // A Customer may never change a balance, register customers or administer anything.
    EXPECT_EQ(client.Post("/transactions", customer, deposit, "application/json")->status, 403);
    EXPECT_EQ(client.Post("/transactions", customer, "not json", "application/json")->status, 403);
    EXPECT_EQ(client.Post("/transfers", customer, transfer, "application/json")->status, 403);
    EXPECT_EQ(client.Post("/customers", customer, "{}", "application/json")->status, 403);
    EXPECT_EQ(client.Post("/interest/run", customer, R"({"period":"2026-04"})", "application/json")->status, 403);
    EXPECT_EQ(client.Post("/staff", customer, R"({"username":"new.officer"})", "application/json")->status, 403);
    EXPECT_EQ(client.Get("/staff", customer)->status, 403);
    EXPECT_EQ(client.Get("/users/locked", customer)->status, 403);
    EXPECT_EQ(client.Get("/audit", customer)->status, 403);

    // A Bank Officer is limited to customer-facing work.
    EXPECT_EQ(client.Post("/interest/run", officer, R"({"period":"2026-04"})", "application/json")->status, 403);
    EXPECT_EQ(client.Post("/staff", officer, R"({"username":"new.officer"})", "application/json")->status, 403);
    EXPECT_EQ(client.Patch("/staff/3", officer, R"({"status":"inactive"})", "application/json")->status, 403);
    EXPECT_EQ(client.Get("/users/locked", officer)->status, 403);
    EXPECT_EQ(client.Patch("/users/5/unlock", officer, "", "application/json")->status, 403);
    EXPECT_EQ(client.Get("/audit", officer)->status, 403);

    EXPECT_EQ(count("SELECT balance FROM accounts WHERE account_number = 'ACC001'"), balance);
    EXPECT_EQ(count("SELECT COUNT(*) FROM interest_runs WHERE period = '2026-04'"), 0);
    EXPECT_EQ(denials_for(kCustomer1), 9);
    EXPECT_EQ(denials_for(kOfficer), 6);

    auto entry = db().prepare(
        "SELECT details FROM audit_log WHERE action = 'ACCESS_DENIED' AND user_id = ? "
        "ORDER BY log_id LIMIT 1");
    entry->bind(1, kCustomer1);
    ASSERT_TRUE(entry->step());
    const std::string details = reinterpret_cast<const char*>(entry->column_text(0));
    EXPECT_NE(details.find("DEPOSIT"), std::string::npos);
    EXPECT_NE(details.find("POST /transactions"), std::string::npos);
}

TEST_F(RbacTest, AnonymousRequestsGet401) {
    TestServer server(db());
    auto& client = server.client();
    EXPECT_EQ(client.Post("/transactions", R"({"type":"deposit"})", "application/json")->status, 401);
    EXPECT_EQ(client.Post("/transfers", "{}", "application/json")->status, 401);
    EXPECT_EQ(client.Post("/interest/run", "{}", "application/json")->status, 401);
    EXPECT_EQ(client.Post("/customers", "{}", "application/json")->status, 401);
    EXPECT_EQ(client.Get("/audit")->status, 401);
}

TEST_F(RbacTest, StaffRolesReachTheirOwnOperations) {
    TestServer server(db());
    auto& client = server.client();
    const auto officer = bms::test::sign_in(client, "officer1");
    const auto admin = bms::test::sign_in(client, "admin");
    const std::string deposit = R"({"type":"deposit","account_number":"ACC001","amount":"10.00"})";

    EXPECT_EQ(client.Post("/transactions", officer, deposit, "application/json")->status, 201);
    EXPECT_EQ(client.Post("/transactions", admin, deposit, "application/json")->status, 201);
    EXPECT_EQ(client.Post("/interest/run", admin, R"({"period":"2026-04"})", "application/json")->status, 201);
    EXPECT_EQ(client.Get("/audit", admin)->status, 200);
    EXPECT_EQ(client.Get("/staff", admin)->status, 200);
    EXPECT_EQ(client.Get("/users/locked", admin)->status, 200);
    EXPECT_EQ(denials_for(kOfficer) + denials_for(kAdmin), 0);
}

TEST_F(RbacTest, CustomersReachOnlyTheirOwnCustomerRecordAndAccounts) {
    EXPECT_TRUE(bms::auth::rbac::owns_customer_record(db(), kCustomer1, 1));
    EXPECT_FALSE(bms::auth::rbac::owns_customer_record(db(), kCustomer1, 2));
    EXPECT_TRUE(bms::auth::rbac::owns_account(db(), kCustomer1, "ACC001"));
    EXPECT_TRUE(bms::auth::rbac::owns_account(db(), kCustomer1, "ACC004"));
    EXPECT_FALSE(bms::auth::rbac::owns_account(db(), kCustomer1, "ACC002"));
    EXPECT_FALSE(bms::auth::rbac::owns_account(db(), kCustomer1, "NOPE"));

    const bms::auth::Principal customer{kCustomer1, "customer", false};
    EXPECT_NO_THROW(bms::auth::rbac::require_account_access(db(), customer, "ACC001"));
    EXPECT_NO_THROW(bms::auth::rbac::require_customer_access(db(), customer, 1));
    EXPECT_TRUE(forbidden([&] { bms::auth::rbac::require_account_access(db(), customer, "ACC002"); }));
    EXPECT_TRUE(forbidden([&] { bms::auth::rbac::require_account_access(db(), customer, "NOPE"); }));
    EXPECT_TRUE(forbidden([&] { bms::auth::rbac::require_customer_access(db(), customer, 2); }));
    EXPECT_EQ(denials_for(kCustomer1), 3);

    // Staff are not limited by ownership.
    const bms::auth::Principal officer{kOfficer, "bank_officer", false};
    EXPECT_NO_THROW(bms::auth::rbac::require_account_access(db(), officer, "ACC002"));
    EXPECT_NO_THROW(bms::auth::rbac::require_customer_access(db(), officer, 2));
}

}  // namespace
