#include <httplib.h>

#include <gtest/gtest.h>

#include "account/routes.hpp"
#include "api/errors.hpp"
#include "api/router.hpp"
#include "audit/routes.hpp"
#include "auth/password.hpp"
#include "auth/routes.hpp"
#include "common/error.hpp"
#include "customer/routes.hpp"
#include "reporting/routes.hpp"
#include "staff/routes.hpp"
#include "support/seeded_database.hpp"
#include "transaction/routes.hpp"

namespace {

class ModuleTest : public bms::test::SeededDatabaseTest {};

TEST_F(ModuleTest, CommonLoadsSeededAdministrator) {
    auto user = db().prepare("SELECT role, status FROM users WHERE username = ?");
    user->bind(1, std::string("admin"));
    ASSERT_TRUE(user->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(user->column_text(0)), "administrator");
    EXPECT_STREQ(reinterpret_cast<const char*>(user->column_text(1)), "active");
}

TEST_F(ModuleTest, SeededDemoPasswordMatchesItsDocumentedCredential) {
    auto user = db().prepare("SELECT password_hash FROM users WHERE username = 'admin'");
    ASSERT_TRUE(user->step());
    const auto* value = user->column_text(0);
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(bms::auth::verify_password("Password123!",
                                           reinterpret_cast<const char*>(value)));
}

TEST_F(ModuleTest, ApiMapsValidationErrorsToBadRequest) {
    EXPECT_EQ(bms::api::http_status(bms::ErrorKind::Validation), 400);
}

// These modules currently provide route-registration stubs only. This smoke
// check verifies each module's public registration function is linked and
// callable; behavioral coverage belongs with the route implementations.
TEST_F(ModuleTest, AccountRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::account::register_routes(router));
}

TEST_F(ModuleTest, AuditRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::audit::register_routes(router));
}

TEST_F(ModuleTest, AuthRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::auth::register_routes(router, db(), true));
}

TEST_F(ModuleTest, CustomerRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::customer::register_routes(router, db(), true));
}

TEST_F(ModuleTest, ReportingRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::reporting::register_routes(router));
}

TEST_F(ModuleTest, StaffRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::staff::register_routes(router));
}

TEST_F(ModuleTest, TransactionRouteRegistrationIsCallable) {
    httplib::Server server;
    bms::api::Router router(server);
    EXPECT_NO_THROW(bms::transaction::register_routes(router, db(), 2));
}

}  // namespace
