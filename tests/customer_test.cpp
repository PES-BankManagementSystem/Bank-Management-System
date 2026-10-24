#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <gtest/gtest.h>

#include "api/router.hpp"
#include "auth/password.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "customer/registration.hpp"
#include "customer/routes.hpp"
#include "customer/validation.hpp"
#include "support/seeded_database.hpp"

namespace {

using nlohmann::json;
constexpr char kPassword[] = "OfficerPassword123!";

bms::customer::CustomerInput valid_input() {
    return {"Rahul Sharma", "1998-04-12", "10 Main Street", "+91 98765-43210",
            "ID-RAHUL-001", "rahul@example.com"};
}

std::string format_date(std::tm date) {
    char buffer[11]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &date);
    return buffer;
}

std::tm shift_day(std::tm date, int days) {
    date.tm_mday += days;
    std::mktime(&date);
    return date;
}

std::int64_t scalar_int(bms::Database& database, const char* sql) {
    auto query = database.prepare(sql);
    EXPECT_TRUE(query->step());
    return query->column_int(0);
}

class TestServer {
public:
    TestServer(bms::Database& database, bool allow_delivery)
        : router_(server_) {
        bms::auth::register_routes(router_, database, allow_delivery);
        bms::customer::register_routes(router_, database, allow_delivery);
        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) throw std::runtime_error("Could not bind customer API test server.");
        thread_ = std::thread([this] { server_.listen_after_bind(); });
        client_ = std::make_unique<httplib::Client>("127.0.0.1", port_);
        client_->set_connection_timeout(2);
        client_->set_read_timeout(5);
        for (int attempt = 0; attempt < 50; ++attempt) {
            auto response = client_->Get("/health");
            if (response) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("Customer API test server did not start.");
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

class CustomerTest : public bms::test::SeededDatabaseTest {
protected:
    void SetUp() override {
        SeededDatabaseTest::SetUp();
        const std::string hash = bms::auth::hash_password(kPassword);
        auto update = db().prepare(
            "UPDATE users SET password_hash = ?, failed_attempts = 0, status = 'active' "
            "WHERE username IN ('admin', 'officer1', 'customer1')");
        update->bind(1, hash);
        update->step();
    }
};

TEST_F(CustomerTest, ValidationNormalizesPhoneAndReportsMultipleFields) {
    auto input = valid_input();
    const auto normalized = bms::customer::validate_customer_input(input);
    EXPECT_EQ(normalized.phone, "+919876543210");

    input.name = " ";
    input.date_of_birth = "2026-02-30";
    input.phone = "12x";
    input.identification_reference = "bad id";
    try {
        (void)bms::customer::validate_customer_input(input);
        FAIL() << "Expected validation failure";
    } catch (const bms::Error& error) {
        ASSERT_EQ(error.fields().size(), 4U);
        EXPECT_EQ(error.fields()[0].first, "name");
        EXPECT_EQ(error.fields()[1].first, "dateOfBirth");
        EXPECT_EQ(error.fields()[2].first, "phone");
        EXPECT_EQ(error.fields()[3].first, "identificationReference");
    }
}

TEST_F(CustomerTest, ValidationEnforcesLengthPhoneAndDateBoundaries) {
    auto input = valid_input();
    input.name = std::string(100, 'A');
    input.address = std::string(255, 'x');
    input.phone = "+123456789012345";
    EXPECT_NO_THROW((void)bms::customer::validate_customer_input(input));

    input = valid_input();
    input.name = std::string(101, 'A');
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);
    input = valid_input();
    input.address = std::string(256, 'x');
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);

    input = valid_input();
    input.address.clear();
    for (int i = 0; i < 255; ++i) input.address += "\xE7\x95\x8C";
    EXPECT_NO_THROW((void)bms::customer::validate_customer_input(input));
    input.address += "\xE7\x95\x8C";
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);

    input = valid_input();
    input.phone = "1234567890";
    EXPECT_NO_THROW((void)bms::customer::validate_customer_input(input));
    input.phone = "123456789";
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);
    input = valid_input();
    input.phone = "1234567890123456";
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);

    std::time_t now = std::time(nullptr);
    std::tm today{};
#ifdef _WIN32
    localtime_s(&today, &now);
#else
    localtime_r(&now, &today);
#endif
    input = valid_input();
    input.date_of_birth = format_date(shift_day(today, -1));
    EXPECT_NO_THROW((void)bms::customer::validate_customer_input(input));
    input.date_of_birth = format_date(today);
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);
    input.date_of_birth = format_date(shift_day(today, 1));
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);
    input.date_of_birth = "2026-02-30";
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);
    input.date_of_birth = "2000-02-29";
    EXPECT_NO_THROW((void)bms::customer::validate_customer_input(input));
    today.tm_year -= 121;
    input.date_of_birth = format_date(today);
    EXPECT_THROW((void)bms::customer::validate_customer_input(input), bms::Error);
}

TEST_F(CustomerTest, EachRequiredFieldRejectsWhitespaceAndEmptyValues) {
    const std::vector<std::pair<std::string, std::string>> fields = {
        {"name", "name"}, {"dateOfBirth", "date_of_birth"}, {"address", "address"},
        {"phone", "phone"}, {"identificationReference", "identification_reference"},
        {"email", "email"},
    };
    for (const auto& field : fields) {
        auto empty = valid_input();
        if (field.second == "name") empty.name = "  ";
        else if (field.second == "date_of_birth") empty.date_of_birth.clear();
        else if (field.second == "address") empty.address = "  ";
        else if (field.second == "phone") empty.phone.clear();
        else if (field.second == "identification_reference") empty.identification_reference = " ";
        else empty.email.clear();
        try {
            (void)bms::customer::validate_customer_input(empty);
            FAIL() << "Expected " << field.first << " to be rejected";
        } catch (const bms::Error& error) {
            ASSERT_EQ(error.fields().size(), 1U);
            EXPECT_EQ(error.fields()[0].first, field.first);
        }
    }
}

TEST_F(CustomerTest, RegistrationCreatesHashedLoginAndRequiresPasswordChange) {
    const auto result = bms::customer::register_customer(db(), valid_input(), 2);
    EXPECT_EQ(result.username, result.customer_code);
    EXPECT_EQ(result.customer_code.size(), 12U);
    EXPECT_EQ(result.phone, "+919876543210");
    EXPECT_EQ(result.temporary_password.size(), 20U);

    auto user = db().prepare(
        "SELECT password_hash, must_change_password FROM users WHERE username = ?");
    user->bind(1, result.username);
    ASSERT_TRUE(user->step());
    const auto* stored = user->column_text(0);
    ASSERT_NE(stored, nullptr);
    const std::string password_hash = reinterpret_cast<const char*>(stored);
    EXPECT_NE(password_hash, result.temporary_password);
    EXPECT_TRUE(bms::auth::verify_password(result.temporary_password, password_hash));
    EXPECT_EQ(user->column_int(1), 1);

    auto audit = db().prepare(
        "SELECT details FROM audit_log WHERE action = 'CUSTOMER_REGISTERED' "
        "AND entity_id = ?");
    audit->bind(1, result.customer_id);
    ASSERT_TRUE(audit->step());
    const auto* details = audit->column_text(0);
    ASSERT_NE(details, nullptr);
    EXPECT_STREQ(reinterpret_cast<const char*>(details),
                 ("customerId=" + result.customer_code).c_str());
}

TEST_F(CustomerTest, SeparateRegistrationsReceiveDifferentCustomerIdentifiers) {
    auto first_input = valid_input();
    auto second_input = valid_input();
    second_input.name = "Second Customer";
    second_input.email = "second@example.com";
    second_input.phone = "+14155552672";
    second_input.identification_reference = "ID-SECOND-001";
    const auto first = bms::customer::register_customer(db(), first_input, 2);
    const auto second = bms::customer::register_customer(db(), second_input, 2);
    EXPECT_NE(first.customer_code, second.customer_code);
    EXPECT_NE(first.username, second.username);
}

TEST_F(CustomerTest, DuplicateIdentificationReferenceIsAConflict) {
    const auto first = bms::customer::register_customer(db(), valid_input(), 2);
    auto duplicate = valid_input();
    duplicate.name = "Another Customer";
    duplicate.phone = "+14155552671";
    duplicate.email = "another@example.com";
    EXPECT_THROW(
        try {
            (void)bms::customer::register_customer(db(), duplicate, 2);
        } catch (const bms::Error& error) {
            EXPECT_EQ(error.kind(), bms::ErrorKind::Conflict);
            EXPECT_EQ(error.field(), std::optional<std::string>("identificationReference"));
            throw;
        },
        bms::Error);
    EXPECT_FALSE(first.customer_code.empty());
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM customers"), 4);
}

TEST_F(CustomerTest, FailedAuditInsertRollsBackCustomerAndLogin) {
    db().exec(
        "CREATE TRIGGER fail_customer_audit BEFORE INSERT ON audit_log "
        "WHEN NEW.action = 'CUSTOMER_REGISTERED' BEGIN "
        "SELECT RAISE(ABORT, 'forced audit failure'); END");
    const auto users_before = scalar_int(db(), "SELECT COUNT(*) FROM users");
    const auto customers_before = scalar_int(db(), "SELECT COUNT(*) FROM customers");
    EXPECT_THROW((void)bms::customer::register_customer(db(), valid_input(), 2), bms::DbError);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM users"), users_before);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM customers"), customers_before);
}

TEST_F(CustomerTest, RegistrationRouteRequiresSessionAndAllowedRole) {
    TestServer server(db(), true);

    const std::string payload =
        R"({"name":"Rahul Sharma","dateOfBirth":"1998-04-12","address":"10 Main Street","phone":"9876543210","identificationReference":"ID-ROUTE-001","email":"route@example.com"})";
    auto unauthenticated = server.client().Post("/customers", payload, "application/json");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);

    auto login_customer = server.client().Post(
        "/auth/login", R"({"username":"customer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(login_customer);
    ASSERT_EQ(login_customer->status, 200) << login_customer->body;
    const std::string customer_token = json::parse(login_customer->body)["accessToken"];
    httplib::Headers customer_headers{{"Authorization", "Bearer " + customer_token}};
    auto forbidden = server.client().Post("/customers", customer_headers, payload,
                                          "application/json");
    ASSERT_TRUE(forbidden);
    EXPECT_EQ(forbidden->status, 403);

    auto login_officer = server.client().Post(
        "/auth/login", R"({"username":"officer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(login_officer);
    ASSERT_EQ(login_officer->status, 200) << login_officer->body;
    const std::string officer_token = json::parse(login_officer->body)["accessToken"];
    httplib::Headers officer_headers{{"Authorization", "Bearer " + officer_token}};
    auto created = server.client().Post("/customers", officer_headers, payload,
                                        "application/json");
    ASSERT_TRUE(created);
    ASSERT_EQ(created->status, 201) << created->body;
    EXPECT_EQ(created->get_header_value("Location").find("/customers/CUS-"), 0U);
    const json body = json::parse(created->body);
    EXPECT_EQ(body["login"]["username"], body["customerId"]);
    EXPECT_EQ(body["login"]["temporaryPassword"].get<std::string>().size(), 20U);
    EXPECT_EQ(created->body.find("password_hash"), std::string::npos);

    const std::string duplicate_reference =
        R"({"name":"Another Customer","dateOfBirth":"1998-04-12","address":"10 Main Street","phone":"9876543211","identificationReference":"ID-ROUTE-001","email":"another.route@example.com"})";
    auto duplicate = server.client().Post("/customers", officer_headers, duplicate_reference,
                                          "application/json");
    ASSERT_TRUE(duplicate);
    ASSERT_EQ(duplicate->status, 409);
    EXPECT_EQ(json::parse(duplicate->body)["field"], "identificationReference");

    auto login_admin = server.client().Post(
        "/auth/login", R"({"username":"admin","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(login_admin);
    ASSERT_EQ(login_admin->status, 200) << login_admin->body;
    const std::string admin_token = json::parse(login_admin->body)["accessToken"];
    httplib::Headers admin_headers{{"Authorization", "Bearer " + admin_token}};
    auto admin_created = server.client().Post(
        "/customers", admin_headers,
        R"({"name":"Jane Smith","dateOfBirth":"1985-08-22","address":"25 Oak Street","phone":"4155552671","identificationReference":"ID-ADMIN-001","email":"jane.route@example.com"})",
        "application/json");
    ASSERT_TRUE(admin_created);
    EXPECT_EQ(admin_created->status, 201) << admin_created->body;

    auto logout = server.client().Post("/auth/logout", officer_headers);
    ASSERT_TRUE(logout);
    EXPECT_EQ(logout->status, 204);
    auto after_logout = server.client().Post("/customers", officer_headers, payload,
                                             "application/json");
    ASSERT_TRUE(after_logout);
    EXPECT_EQ(after_logout->status, 401);
}

TEST_F(CustomerTest, CredentialDeliveryRouteIsDisabledForNetworkListeners) {
    TestServer server(db(), false);
    auto login = server.client().Post(
        "/auth/login", R"({"username":"officer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(login);
    EXPECT_EQ(login->status, 403);
    auto no_session = server.client().Post(
        "/customers", R"({"name":"Rahul Sharma","dateOfBirth":"1998-04-12","address":"10 Main Street","phone":"9876543210","identificationReference":"ID-NETWORK-001","email":"network@example.com"})",
        "application/json");
    ASSERT_TRUE(no_session);
    EXPECT_EQ(no_session->status, 401);
}

TEST_F(CustomerTest, TemporaryPasswordForcesPasswordChangeAtLogin) {
    const auto result = bms::customer::register_customer(db(), valid_input(), 2);
    TestServer server(db(), true);
    const std::string login_body =
        json({{"username", result.username}, {"password", result.temporary_password}}).dump();
    auto login = server.client().Post("/auth/login", login_body, "application/json");
    ASSERT_TRUE(login);
    ASSERT_EQ(login->status, 200) << login->body;
    const json login_result = json::parse(login->body);
    EXPECT_TRUE(login_result["mustChangePassword"]);
    const std::string token = login_result["accessToken"];
    httplib::Headers headers{{"Authorization", "Bearer " + token}};

    auto blocked = server.client().Post("/customers", headers,
        R"({"name":"Blocked Name","dateOfBirth":"1998-04-12","address":"10 Main Street","phone":"9876543211","identificationReference":"ID-BLOCKED-001","email":"blocked@example.com"})",
        "application/json");
    ASSERT_TRUE(blocked);
    EXPECT_EQ(blocked->status, 403);

    auto changed = server.client().Post(
        "/auth/change-password", headers,
        R"({"newPassword":"MyNewPassword456!"})", "application/json");
    ASSERT_TRUE(changed);
    EXPECT_EQ(changed->status, 200) << changed->body;

    const std::string changed_login_body =
        json({{"username", result.username}, {"password", "MyNewPassword456!"}}).dump();
    auto changed_login = server.client().Post("/auth/login", changed_login_body,
                                               "application/json");
    ASSERT_TRUE(changed_login);
    ASSERT_EQ(changed_login->status, 200) << changed_login->body;
    EXPECT_FALSE(json::parse(changed_login->body)["mustChangePassword"]);
}

TEST_F(CustomerTest, InvalidRequestAggregatesFieldErrors) {
    TestServer server(db(), true);
    auto login = server.client().Post(
        "/auth/login", R"({"username":"officer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(login);
    ASSERT_EQ(login->status, 200);
    const std::string token = json::parse(login->body)["accessToken"];
    httplib::Headers headers{{"Authorization", "Bearer " + token}};
    auto invalid = server.client().Post(
        "/customers", headers,
        R"({"name":7,"dateOfBirth":"2026-02-30","address":" ","phone":"12x","identificationReference":"bad id","email":"invalid"})",
        "application/json");
    ASSERT_TRUE(invalid);
    ASSERT_EQ(invalid->status, 400);
    const json body = json::parse(invalid->body);
    ASSERT_TRUE(body["fields"].is_array());
    EXPECT_GE(body["fields"].size(), 6U);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM customers"), 3);
}

TEST_F(CustomerTest, PasswordHashHelperVerifiesStoredCredential) {
    auto query = db().prepare("SELECT password_hash FROM users WHERE username = 'admin'");
    ASSERT_TRUE(query->step());
    const auto* value = query->column_text(0);
    ASSERT_NE(value, nullptr);
    EXPECT_TRUE(bms::auth::verify_password(kPassword, reinterpret_cast<const char*>(value)));
}

TEST_F(CustomerTest, VersionOneDatabaseMigrationAddsCredentialFields) {
    bms::Database legacy(":memory:");
    legacy.exec(
        "CREATE TABLE users (user_id INTEGER PRIMARY KEY, username TEXT NOT NULL UNIQUE, "
        "role TEXT NOT NULL, status TEXT NOT NULL, failed_attempts INTEGER NOT NULL, "
        "password_hash TEXT NOT NULL)");
    legacy.exec(
        "CREATE TABLE customers (customer_id INTEGER PRIMARY KEY, user_id INTEGER, "
        "full_name TEXT NOT NULL, email TEXT NOT NULL, phone TEXT NOT NULL, "
        "address TEXT NOT NULL, date_of_birth TEXT NOT NULL)");
    legacy.exec(
        "INSERT INTO users VALUES (1, 'officer', 'bank_officer', 'active', 0, 'hash')");
    legacy.exec(
        "INSERT INTO customers VALUES (7, 1, 'Legacy Customer', 'legacy@example.com', "
        "'+14155552671', 'Legacy Street', '1990-01-01')");

    std::ifstream migration(std::filesystem::path(BMS_DB_DIR) / "migrations" /
                            "002_customer_credentials.sql");
    ASSERT_TRUE(migration.good());
    std::ostringstream script;
    script << migration.rdbuf();
    legacy.exec_script(script.str());

    auto row = legacy.prepare(
        "SELECT customer_code, identification_reference FROM customers WHERE customer_id = 7");
    ASSERT_TRUE(row->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(row->column_text(0)), "CUS-00000007");
    EXPECT_STREQ(reinterpret_cast<const char*>(row->column_text(1)), "LEGACY-ID-00000007");
    auto flag = legacy.prepare("SELECT must_change_password FROM users WHERE user_id = 1");
    ASSERT_TRUE(flag->step());
    EXPECT_EQ(flag->column_int(0), 0);
}

}  // namespace
