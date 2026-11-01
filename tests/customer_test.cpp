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
#include "common/calendar.hpp"
#include "customer/registration.hpp"
#include "customer/records.hpp"
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
            "WHERE username IN ('admin', 'officer1', 'customer1', 'customer2')");
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

    std::tm today = bms::calendar::to_local(std::time(nullptr));
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

TEST_F(CustomerTest, FR3_SearchMatchesCustomerIdNamePhoneAndLiteralWildcards) {
    const auto exact = bms::customer::search_customers(db(), "cus-00000001", 20, 0);
    ASSERT_EQ(exact.total, 1);
    EXPECT_EQ(exact.items[0].customer_id, "CUS-00000001");

    const auto prefix = bms::customer::search_customers(db(), "CUS-0000000", 20, 0);
    EXPECT_EQ(prefix.total, 3);

    const auto name = bms::customer::search_customers(db(), "dOe", 20, 0);
    ASSERT_EQ(name.total, 1);
    EXPECT_EQ(name.items[0].name, "John Doe");

    const auto phone = bms::customer::search_customers(db(), "555-0101", 20, 0);
    ASSERT_EQ(phone.total, 1);
    EXPECT_EQ(phone.items[0].customer_id, "CUS-00000001");
    const auto phone_with_spaces =
        bms::customer::search_customers(db(), "555 0101", 20, 0);
    ASSERT_EQ(phone_with_spaces.total, 1);
    EXPECT_EQ(phone_with_spaces.items[0].customer_id, "CUS-00000001");

    const auto wildcard = bms::customer::search_customers(db(), "%_", 20, 0);
    EXPECT_EQ(wildcard.total, 0);
    EXPECT_TRUE(wildcard.items.empty());

    const auto empty = bms::customer::search_customers(db(), "", 20, 0);
    ASSERT_EQ(empty.total, 3);
    const auto first_page = bms::customer::search_customers(db(), "", 1, 1);
    ASSERT_EQ(first_page.total, 3);
    ASSERT_EQ(first_page.items.size(), 1U);
    EXPECT_EQ(first_page.items[0].customer_id, "CUS-00000002");
}

TEST_F(CustomerTest, FR3_DetailsReturnAccountsAndUnknownCustomerIsNotFound) {
    const auto customer = bms::customer::get_customer(db(), "CUS-00000001");
    EXPECT_EQ(customer.customer_id, "CUS-00000001");
    ASSERT_EQ(customer.accounts.size(), 2U);
    EXPECT_EQ(customer.accounts[0].account_number, "ACC001");
    EXPECT_EQ(customer.accounts[0].type, "Savings");
    EXPECT_EQ(customer.accounts[0].balance_paise, 5000000);

    try {
        (void)bms::customer::get_customer(db(), "not-a-customer-id");
        FAIL() << "Expected an unknown customer to return not found";
    } catch (const bms::Error& error) {
        EXPECT_EQ(error.kind(), bms::ErrorKind::NotFound);
        EXPECT_NE(std::string(error.what()).find("not-a-customer-id"), std::string::npos);
    }

    auto created_input = valid_input();
    created_input.phone = "+14155552679";
    created_input.email = "no.accounts@example.com";
    created_input.identification_reference = "ID-NO-ACCOUNTS";
    const auto created = bms::customer::register_customer(db(), created_input, 2);
    const auto no_accounts = bms::customer::get_customer(db(), created.customer_code);
    EXPECT_TRUE(no_accounts.accounts.empty());
}

TEST_F(CustomerTest, FR3_UpdateAllowsOwnUniqueValuesAndAuditsFieldNamesOnly) {
    const auto registered = bms::customer::register_customer(db(), valid_input(), 2);
    const auto unchanged =
        bms::customer::update_customer(db(), registered.customer_code, valid_input(), 2);
    EXPECT_EQ(unchanged.name, valid_input().name);
    EXPECT_EQ(unchanged.phone, "+919876543210");

    auto revised = valid_input();
    revised.name = "Rahul Sharma Updated";
    revised.phone = "+14155552672";
    revised.email = "updated.rahul@example.com";
    revised.identification_reference = "ID-RAHUL-002";
    const auto updated = bms::customer::update_customer(
        db(), registered.customer_code, revised, 2);
    EXPECT_EQ(updated.name, revised.name);
    EXPECT_EQ(updated.phone, revised.phone);
    EXPECT_EQ(updated.email, revised.email);
    EXPECT_EQ(updated.identification_reference, revised.identification_reference);

    auto audit = db().prepare(
        "SELECT details FROM audit_log WHERE action = 'CUSTOMER_UPDATED' "
        "AND entity_id = ? ORDER BY log_id DESC LIMIT 1");
    audit->bind(1, registered.customer_id);
    ASSERT_TRUE(audit->step());
    const auto* details = audit->column_text(0);
    ASSERT_NE(details, nullptr);
    const std::string audit_details = reinterpret_cast<const char*>(details);
    EXPECT_NE(audit_details.find("fields=name,phone,email,identificationReference"),
              std::string::npos);
    EXPECT_EQ(audit_details.find(revised.name), std::string::npos);
    EXPECT_EQ(audit_details.find(revised.phone), std::string::npos);
    EXPECT_EQ(audit_details.find(revised.email), std::string::npos);
    EXPECT_EQ(audit_details.find(revised.identification_reference), std::string::npos);
}

TEST_F(CustomerTest, FR3_UpdateRejectsDuplicateFieldsAndRollsBackWhenAuditFails) {
    const auto first = bms::customer::register_customer(db(), valid_input(), 2);
    auto second_input = valid_input();
    second_input.name = "Another Customer";
    second_input.phone = "+14155552672";
    second_input.email = "another@example.com";
    second_input.identification_reference = "ID-ANOTHER-001";
    const auto second = bms::customer::register_customer(db(), second_input, 2);

    auto duplicate = second_input;
    duplicate.phone = first.phone;
    const std::vector<std::pair<std::string, std::string>> duplicate_values = {
        {"phone", first.phone},
        {"email", first.email},
        {"identificationReference", "ID-RAHUL-001"},
    };
    for (const auto& duplicate_value : duplicate_values) {
        duplicate = second_input;
        if (duplicate_value.first == "phone") duplicate.phone = duplicate_value.second;
        else if (duplicate_value.first == "email") duplicate.email = duplicate_value.second;
        else duplicate.identification_reference = duplicate_value.second;
        try {
            (void)bms::customer::update_customer(db(), second.customer_code, duplicate, 2);
            FAIL() << "Expected duplicate " << duplicate_value.first << " conflict";
        } catch (const bms::Error& error) {
            EXPECT_EQ(error.kind(), bms::ErrorKind::Conflict);
            EXPECT_EQ(error.field(),
                      std::optional<std::string>(duplicate_value.first));
        }
    }

    db().exec(
        "CREATE TRIGGER fail_customer_update_audit BEFORE INSERT ON audit_log "
        "WHEN NEW.action = 'CUSTOMER_UPDATED' BEGIN "
        "SELECT RAISE(ABORT, 'forced audit failure'); END");
    const std::int64_t audit_count =
        scalar_int(db(), "SELECT COUNT(*) FROM audit_log WHERE action = 'CUSTOMER_UPDATED'");
    auto revised = second_input;
    revised.name = "Must Roll Back";
    EXPECT_THROW(
        (void)bms::customer::update_customer(db(), second.customer_code, revised, 2),
        bms::DbError);
    EXPECT_EQ(bms::customer::get_customer(db(), second.customer_code).name,
              second_input.name);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM audit_log WHERE action = 'CUSTOMER_UPDATED'"),
              audit_count);
}

TEST_F(CustomerTest, FR3_SearchAndDetailsRoutesEnforceRolesAndOwnership) {
    TestServer server(db(), true);
    auto unauthenticated = server.client().Get("/customers");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);

    auto customer_login = server.client().Post(
        "/auth/login", R"({"username":"customer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(customer_login);
    ASSERT_EQ(customer_login->status, 200);
    const std::string customer_token = json::parse(customer_login->body)["accessToken"];
    httplib::Headers customer_headers{{"Authorization", "Bearer " + customer_token}};
    auto customer_search = server.client().Get("/customers", customer_headers);
    ASSERT_TRUE(customer_search);
    EXPECT_EQ(customer_search->status, 403);

    auto own = server.client().Get("/customers/me", customer_headers);
    ASSERT_TRUE(own);
    ASSERT_EQ(own->status, 200) << own->body;
    EXPECT_EQ(json::parse(own->body)["customerId"], "CUS-00000001");
    EXPECT_EQ(json::parse(own->body)["accounts"].size(), 2U);

    auto own_by_id = server.client().Get("/customers/CUS-00000001", customer_headers);
    ASSERT_TRUE(own_by_id);
    EXPECT_EQ(own_by_id->status, 200);
    auto other_by_id = server.client().Get("/customers/CUS-00000002", customer_headers);
    ASSERT_TRUE(other_by_id);
    EXPECT_EQ(other_by_id->status, 404);
    EXPECT_NE(json::parse(other_by_id->body)["message"].get<std::string>().find(
                  "CUS-00000002"), std::string::npos);

    auto officer_login = server.client().Post(
        "/auth/login", R"({"username":"officer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(officer_login);
    ASSERT_EQ(officer_login->status, 200);
    const std::string officer_token = json::parse(officer_login->body)["accessToken"];
    httplib::Headers officer_headers{{"Authorization", "Bearer " + officer_token}};
    auto list = server.client().Get("/customers?query=DOE&limit=1&offset=0", officer_headers);
    ASSERT_TRUE(list);
    ASSERT_EQ(list->status, 200) << list->body;
    const json list_body = json::parse(list->body);
    EXPECT_EQ(list_body["total"], 1);
    EXPECT_EQ(list_body["items"][0]["name"], "John Doe");
    EXPECT_FALSE(list_body["items"][0].contains("identificationReference"));

    auto invalid_limit = server.client().Get("/customers?limit=0", officer_headers);
    ASSERT_TRUE(invalid_limit);
    EXPECT_EQ(invalid_limit->status, 400);
    EXPECT_EQ(json::parse(invalid_limit->body)["field"], "limit");
    auto invalid_offset = server.client().Get("/customers?offset=-1", officer_headers);
    ASSERT_TRUE(invalid_offset);
    EXPECT_EQ(invalid_offset->status, 400);
    EXPECT_EQ(json::parse(invalid_offset->body)["field"], "offset");

    auto missing = server.client().Get("/customers/CUS-00000099", officer_headers);
    ASSERT_TRUE(missing);
    EXPECT_EQ(missing->status, 404);
    EXPECT_NE(json::parse(missing->body)["message"].get<std::string>().find(
                  "CUS-00000099"), std::string::npos);

    auto admin_login = server.client().Post(
        "/auth/login", R"({"username":"admin","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(admin_login);
    ASSERT_EQ(admin_login->status, 200);
    const std::string admin_token = json::parse(admin_login->body)["accessToken"];
    httplib::Headers admin_headers{{"Authorization", "Bearer " + admin_token}};
    auto admin_details = server.client().Get("/customers/CUS-00000002", admin_headers);
    ASSERT_TRUE(admin_details);
    EXPECT_EQ(admin_details->status, 200);
}

TEST_F(CustomerTest, FR3_UpdateRouteValidatesAndRestrictsCustomerRole) {
    TestServer server(db(), true);
    const std::string valid_payload =
        R"({"name":"Updated John","dateOfBirth":"1990-05-15","address":"Updated address","phone":"9876543210","identificationReference":"ID-JOHN-UPDATED","email":"updated.john@example.com"})";
    auto unauthenticated = server.client().Put(
        "/customers/CUS-00000001", valid_payload, "application/json");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);

    auto customer_login = server.client().Post(
        "/auth/login", R"({"username":"customer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(customer_login);
    const std::string customer_token = json::parse(customer_login->body)["accessToken"];
    httplib::Headers customer_headers{{"Authorization", "Bearer " + customer_token}};
    auto forbidden_update = server.client().Put(
        "/customers/CUS-00000001", customer_headers, valid_payload, "application/json");
    ASSERT_TRUE(forbidden_update);
    EXPECT_EQ(forbidden_update->status, 403);

    auto officer_login = server.client().Post(
        "/auth/login", R"({"username":"officer1","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(officer_login);
    const std::string officer_token = json::parse(officer_login->body)["accessToken"];
    httplib::Headers officer_headers{{"Authorization", "Bearer " + officer_token}};
    auto login_before = db().prepare(
        "SELECT username, role, status, password_hash, must_change_password "
        "FROM users WHERE user_id = 4");
    ASSERT_TRUE(login_before->step());
    const std::vector<std::string> login_values_before = {
        reinterpret_cast<const char*>(login_before->column_text(0)),
        reinterpret_cast<const char*>(login_before->column_text(1)),
        reinterpret_cast<const char*>(login_before->column_text(2)),
        reinterpret_cast<const char*>(login_before->column_text(3)),
        std::to_string(login_before->column_int(4)),
    };
    auto invalid = server.client().Put(
        "/customers/CUS-00000001", officer_headers, "{}", "application/json");
    ASSERT_TRUE(invalid);
    ASSERT_EQ(invalid->status, 400);
    EXPECT_GE(json::parse(invalid->body)["fields"].size(), 6U);
    const std::string several_invalid_fields =
        R"({"name":7,"dateOfBirth":"2026-02-30","address":" ","phone":"12x","identificationReference":"bad id","email":"invalid"})";
    auto several_invalid = server.client().Put(
        "/customers/CUS-00000001", officer_headers, several_invalid_fields,
        "application/json");
    ASSERT_TRUE(several_invalid);
    ASSERT_EQ(several_invalid->status, 400);
    EXPECT_GE(json::parse(several_invalid->body)["fields"].size(), 6U);

    const std::string immutable_payload =
        R"({"customerId":"CUS-00000002","name":"Updated John","dateOfBirth":"1990-05-15","address":"Updated address","phone":"9876543210","identificationReference":"ID-JOHN-UPDATED","email":"updated.john@example.com"})";
    auto immutable = server.client().Put(
        "/customers/CUS-00000001", officer_headers, immutable_payload,
        "application/json");
    ASSERT_TRUE(immutable);
    EXPECT_EQ(immutable->status, 400);
    EXPECT_EQ(json::parse(immutable->body)["fields"][0]["field"], "customerId");

    auto updated = server.client().Put(
        "/customers/CUS-00000001", officer_headers, valid_payload, "application/json");
    ASSERT_TRUE(updated);
    ASSERT_EQ(updated->status, 200) << updated->body;
    EXPECT_EQ(json::parse(updated->body)["name"], "Updated John");
    EXPECT_EQ(json::parse(updated->body)["accounts"].size(), 2U);
    EXPECT_FALSE(json::parse(updated->body)["updatedAt"].get<std::string>().empty());
    auto login_after = db().prepare(
        "SELECT username, role, status, password_hash, must_change_password "
        "FROM users WHERE user_id = 4");
    ASSERT_TRUE(login_after->step());
    const std::vector<std::string> login_values_after = {
        reinterpret_cast<const char*>(login_after->column_text(0)),
        reinterpret_cast<const char*>(login_after->column_text(1)),
        reinterpret_cast<const char*>(login_after->column_text(2)),
        reinterpret_cast<const char*>(login_after->column_text(3)),
        std::to_string(login_after->column_int(4)),
    };
    EXPECT_EQ(login_values_after, login_values_before);

    const std::string duplicate_phone =
        R"({"name":"Jane Smith Updated","dateOfBirth":"1985-08-22","address":"Updated address","phone":"9876543210","identificationReference":"ID-JANE-UPDATED","email":"jane.updated@example.com"})";
    auto duplicate = server.client().Put(
        "/customers/CUS-00000002", officer_headers, duplicate_phone, "application/json");
    ASSERT_TRUE(duplicate);
    ASSERT_EQ(duplicate->status, 409);
    EXPECT_EQ(json::parse(duplicate->body)["field"], "phone");

    auto missing = server.client().Put(
        "/customers/CUS-00000099", officer_headers, valid_payload, "application/json");
    ASSERT_TRUE(missing);
    EXPECT_EQ(missing->status, 404);

    auto admin_login = server.client().Post(
        "/auth/login", R"({"username":"admin","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(admin_login);
    ASSERT_EQ(admin_login->status, 200);
    const std::string admin_token = json::parse(admin_login->body)["accessToken"];
    httplib::Headers admin_headers{{"Authorization", "Bearer " + admin_token}};
    auto admin_update = server.client().Put(
        "/customers/CUS-00000001", admin_headers, valid_payload, "application/json");
    ASSERT_TRUE(admin_update);
    EXPECT_EQ(admin_update->status, 200) << admin_update->body;
}

TEST_F(CustomerTest, FR3_AdminDeactivationIsAtomicIdempotentAndPreservesHistory) {
    TestServer server(db(), true);
    auto unauthenticated = server.client().Put(
        "/customers/CUS-00000001", R"({"status":"inactive"})", "application/json");
    ASSERT_TRUE(unauthenticated);
    EXPECT_EQ(unauthenticated->status, 401);

    auto login = [&server](const char* username) {
        return server.client().Post(
            "/auth/login",
            json({{"username", username}, {"password", kPassword}}).dump(),
            "application/json");
    };
    auto admin_login = login("admin");
    ASSERT_TRUE(admin_login);
    ASSERT_EQ(admin_login->status, 200) << admin_login->body;
    httplib::Headers admin_headers{{
        "Authorization", "Bearer " + json::parse(admin_login->body)["accessToken"].get<std::string>()}};
    auto officer_login = login("officer1");
    ASSERT_TRUE(officer_login);
    ASSERT_EQ(officer_login->status, 200);
    httplib::Headers officer_headers{{
        "Authorization", "Bearer " + json::parse(officer_login->body)["accessToken"].get<std::string>()}};
    auto customer_login = login("customer1");
    ASSERT_TRUE(customer_login);
    ASSERT_EQ(customer_login->status, 200);
    httplib::Headers customer_headers{{
        "Authorization", "Bearer " + json::parse(customer_login->body)["accessToken"].get<std::string>()}};

    auto forbidden_officer = server.client().Put(
        "/customers/CUS-00000001", officer_headers, R"({"status":"inactive"})",
        "application/json");
    ASSERT_TRUE(forbidden_officer);
    EXPECT_EQ(forbidden_officer->status, 403);
    auto forbidden_customer = server.client().Put(
        "/customers/CUS-00000001", customer_headers, R"({"status":"inactive"})",
        "application/json");
    ASSERT_TRUE(forbidden_customer);
    EXPECT_EQ(forbidden_customer->status, 403);

    auto invalid_status = server.client().Put(
        "/customers/CUS-00000001", admin_headers, R"({"status":"active"})",
        "application/json");
    ASSERT_TRUE(invalid_status);
    ASSERT_EQ(invalid_status->status, 400);
    EXPECT_EQ(json::parse(invalid_status->body)["field"], "status");
    auto unknown = server.client().Put(
        "/customers/CUS-00000099", admin_headers, R"({"status":"inactive"})",
        "application/json");
    ASSERT_TRUE(unknown);
    EXPECT_EQ(unknown->status, 404);
    EXPECT_NE(json::parse(unknown->body)["message"].get<std::string>().find(
                  "CUS-00000099"), std::string::npos);

    const std::int64_t accounts_before = scalar_int(
        db(), "SELECT COUNT(*) FROM accounts WHERE customer_id = 1");
    const std::int64_t transactions_before = scalar_int(
        db(), "SELECT COUNT(*) FROM transactions "
              "WHERE account_id IN (SELECT account_id FROM accounts WHERE customer_id = 1)");
    auto deactivated = server.client().Put(
        "/customers/CUS-00000001", admin_headers, R"({"status":"inactive"})",
        "application/json");
    ASSERT_TRUE(deactivated);
    ASSERT_EQ(deactivated->status, 200) << deactivated->body;
    EXPECT_EQ(json::parse(deactivated->body)["status"], "inactive");
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM customers WHERE customer_id = 1"), 1);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM users WHERE user_id = 4"), 1);
    auto user_status = db().prepare("SELECT status FROM users WHERE user_id = 4");
    ASSERT_TRUE(user_status->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(user_status->column_text(0)), "inactive");
    auto audit = db().prepare(
        "SELECT user_id, details, timestamp FROM audit_log "
        "WHERE action = 'CUSTOMER_DEACTIVATED' AND entity_id = 1");
    ASSERT_TRUE(audit->step());
    EXPECT_EQ(audit->column_int(0), 1);
    const std::string audit_details =
        reinterpret_cast<const char*>(audit->column_text(1));
    EXPECT_NE(audit_details.find("customerId=CUS-00000001"), std::string::npos);
    EXPECT_NE(audit_details.find("oldStatus=active;newStatus=inactive"), std::string::npos);
    EXPECT_FALSE(audit->column_null(2));
    EXPECT_EQ(audit_details.find("John Doe"), std::string::npos);
    EXPECT_EQ(audit_details.find("john.doe@example.com"), std::string::npos);
    EXPECT_EQ(audit_details.find("555-0101"), std::string::npos);

    auto old_token_profile = server.client().Get("/customers/me", customer_headers);
    ASSERT_TRUE(old_token_profile);
    EXPECT_EQ(old_token_profile->status, 401);
    auto old_token_password_change = server.client().Post(
        "/auth/change-password", customer_headers,
        R"({"newPassword":"NewCustomerPassword123!"})", "application/json");
    ASSERT_TRUE(old_token_password_change);
    EXPECT_EQ(old_token_password_change->status, 401);
    auto inactive_login = login("customer1");
    ASSERT_TRUE(inactive_login);
    EXPECT_EQ(inactive_login->status, 401);
    EXPECT_EQ(json::parse(inactive_login->body)["message"], "Invalid username or password.");
    auto active_login = login("customer2");
    ASSERT_TRUE(active_login);
    EXPECT_EQ(active_login->status, 200) << active_login->body;

    auto search = server.client().Get("/customers?query=CUS-00000001", admin_headers);
    ASSERT_TRUE(search);
    ASSERT_EQ(search->status, 200) << search->body;
    ASSERT_EQ(json::parse(search->body)["items"].size(), 1U);
    EXPECT_EQ(json::parse(search->body)["items"][0]["status"], "inactive");
    auto details = server.client().Get("/customers/CUS-00000001", admin_headers);
    ASSERT_TRUE(details);
    ASSERT_EQ(details->status, 200);
    EXPECT_EQ(json::parse(details->body)["status"], "inactive");
    EXPECT_EQ(json::parse(details->body)["accounts"].size(), 2U);
    EXPECT_EQ(scalar_int(
                  db(), "SELECT COUNT(*) FROM accounts WHERE customer_id = 1"),
              accounts_before);
    EXPECT_EQ(scalar_int(
                  db(), "SELECT COUNT(*) FROM transactions "
                        "WHERE account_id IN (SELECT account_id FROM accounts WHERE customer_id = 1)"),
              transactions_before);

    const std::int64_t audit_count =
        scalar_int(db(), "SELECT COUNT(*) FROM audit_log WHERE action = 'CUSTOMER_DEACTIVATED'");
    auto repeated = server.client().Put(
        "/customers/CUS-00000001", admin_headers, R"({"status":"inactive"})",
        "application/json");
    ASSERT_TRUE(repeated);
    EXPECT_EQ(repeated->status, 200);
    EXPECT_EQ(json::parse(repeated->body)["status"], "inactive");
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM audit_log WHERE action = 'CUSTOMER_DEACTIVATED'"),
              audit_count);
}

TEST_F(CustomerTest, SRS55_DeactivationAuditFailureRollsBackCustomerAndLogin) {
    db().exec(
        "CREATE TRIGGER fail_customer_deactivation_audit BEFORE INSERT ON audit_log "
        "WHEN NEW.action = 'CUSTOMER_DEACTIVATED' BEGIN "
        "SELECT RAISE(ABORT, 'forced audit failure'); END");
    TestServer server(db(), true);
    auto admin_login = server.client().Post(
        "/auth/login", R"({"username":"admin","password":"OfficerPassword123!"})",
        "application/json");
    ASSERT_TRUE(admin_login);
    ASSERT_EQ(admin_login->status, 200);
    httplib::Headers admin_headers{{
        "Authorization", "Bearer " + json::parse(admin_login->body)["accessToken"].get<std::string>()}};
    auto result = server.client().Put(
        "/customers/CUS-00000001", admin_headers, R"({"status":"inactive"})",
        "application/json");
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, 500);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM customers "
                               "WHERE customer_id = 1 AND status = 'active'"),
              1);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM users "
                               "WHERE user_id = 4 AND status = 'active'"),
              1);
    EXPECT_EQ(scalar_int(db(), "SELECT COUNT(*) FROM audit_log "
                               "WHERE action = 'CUSTOMER_DEACTIVATED'"),
              0);
}

TEST_F(CustomerTest, FR3_AccountOpeningGuardAllowsOnlyActiveCustomers) {
    EXPECT_NO_THROW(bms::customer::require_active_customer(db(), "CUS-00000001"));
    EXPECT_NO_THROW(bms::customer::require_active_customer(db(), "CUS-00000002"));
    (void)bms::customer::deactivate_customer(db(), "CUS-00000001", 1);
    try {
        bms::customer::require_active_customer(db(), "CUS-00000001");
        FAIL() << "Expected an inactive customer to be rejected";
    } catch (const bms::Error& error) {
        EXPECT_EQ(error.kind(), bms::ErrorKind::Conflict);
        EXPECT_NE(std::string(error.what()).find("inactive"), std::string::npos);
    }
    try {
        bms::customer::require_active_customer(db(), "CUS-00000099");
        FAIL() << "Expected an unknown customer to be rejected";
    } catch (const bms::Error& error) {
        EXPECT_EQ(error.kind(), bms::ErrorKind::NotFound);
    }
}

TEST_F(CustomerTest, FR3_CustomerDataRoutesAreDisabledOnNetworkHttpListeners) {
    TestServer server(db(), false);
    auto list = server.client().Get("/customers");
    ASSERT_TRUE(list);
    EXPECT_EQ(list->status, 403);
    auto details = server.client().Get("/customers/CUS-00000001");
    ASSERT_TRUE(details);
    EXPECT_EQ(details->status, 403);
    auto status_change = server.client().Put(
        "/customers/CUS-00000001", R"({"status":"inactive"})", "application/json");
    ASSERT_TRUE(status_change);
    EXPECT_EQ(status_change->status, 403);
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

TEST_F(CustomerTest, SRS55_VersionTwoMigrationAddsIndependentCustomerStatus) {
    bms::Database legacy(":memory:");
    legacy.exec(
        "CREATE TABLE users (user_id INTEGER PRIMARY KEY, status TEXT NOT NULL)");
    legacy.exec(
        "CREATE TABLE customers (customer_id INTEGER PRIMARY KEY, customer_code TEXT NOT NULL, "
        "user_id INTEGER, identification_reference TEXT NOT NULL)");
    legacy.exec("INSERT INTO users VALUES (1, 'active'), (2, 'inactive'), (3, 'locked')");
    legacy.exec(
        "INSERT INTO customers VALUES (1, 'CUS-00000001', 1, 'ID-1'), "
        "(2, 'CUS-00000002', 2, 'ID-2'), (3, 'CUS-00000003', 3, 'ID-3')");
    legacy.exec("PRAGMA user_version = 2");

    std::ifstream migration(std::filesystem::path(BMS_DB_DIR) / "migrations" /
                            "003_customer_status.sql");
    ASSERT_TRUE(migration.good());
    std::ostringstream script;
    script << migration.rdbuf();
    legacy.exec_script(script.str());

    auto rows = legacy.prepare(
        "SELECT customer_code, status FROM customers ORDER BY customer_id");
    ASSERT_TRUE(rows->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(rows->column_text(0)), "CUS-00000001");
    EXPECT_STREQ(reinterpret_cast<const char*>(rows->column_text(1)), "active");
    ASSERT_TRUE(rows->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(rows->column_text(0)), "CUS-00000002");
    EXPECT_STREQ(reinterpret_cast<const char*>(rows->column_text(1)), "inactive");
    ASSERT_TRUE(rows->step());
    EXPECT_STREQ(reinterpret_cast<const char*>(rows->column_text(1)), "active");
    auto version = legacy.prepare("PRAGMA user_version");
    ASSERT_TRUE(version->step());
    EXPECT_EQ(version->column_int(0), 3);
}

}  // namespace
