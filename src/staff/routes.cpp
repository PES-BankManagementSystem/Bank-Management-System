#include "staff/routes.hpp"

#include <cctype>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include <sodium.h>

#include "api/router.hpp"
#include "audit/record.hpp"
#include "auth/password.hpp"
#include "auth/rbac.hpp"
#include "auth/routes.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::staff {
namespace {

constexpr char kPasswordAlphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%*-_";
constexpr std::size_t kTemporaryPasswordLength = 20;

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

auth::Principal require_administrator(const api::Request& request, Database& database) {
    return auth::rbac::require(request, database, auth::rbac::Operation::ManageStaff);
}

std::string generate_temporary_password() {
    if (sodium_init() < 0) {
        throw std::runtime_error("Unable to initialize secure random number generation.");
    }
    std::string password;
    password.reserve(kTemporaryPasswordLength);
    for (std::size_t i = 0; i < kTemporaryPasswordLength; ++i) {
        password.push_back(kPasswordAlphabet[randombytes_uniform(sizeof(kPasswordAlphabet) - 1)]);
    }
    return password;
}

// Officer usernames: 3-32 characters, lower-case letters, digits, '.', '_' or
// '-', starting with a letter. Customer usernames are CUS-... codes, so the
// two never collide.
std::string validated_username(const api::Json& value) {
    if (!value.is_string()) {
        throw validation_error("username", "Username must be a string.");
    }
    const std::string username = value.get<std::string>();
    bool valid = username.size() >= 3 && username.size() <= 32 &&
                 std::islower(static_cast<unsigned char>(username[0])) != 0;
    for (const char c : username) {
        const auto u = static_cast<unsigned char>(c);
        if (!(std::islower(u) || std::isdigit(u) || c == '.' || c == '_' || c == '-')) {
            valid = false;
        }
    }
    if (!valid) {
        throw validation_error("username",
                               "Use 3-32 lower-case letters, digits, '.', '_' or '-', "
                               "starting with a letter.");
    }
    return username;
}

void reject_duplicate_username(Database& database, const std::string& username,
                               std::int64_t except_user_id = 0) {
    auto query = database.prepare("SELECT 1 FROM users WHERE username = ? AND user_id <> ?");
    query->bind(1, username);
    query->bind(2, except_user_id);
    if (query->step()) {
        throw conflict("DUPLICATE_USERNAME", "This username is already in use.", "username");
    }
}

std::int64_t parse_id(const std::string& value) {
    if (value.empty() || value.size() > 18 ||
        value.find_first_not_of("0123456789") != std::string::npos) {
        throw not_found("Bank Officer not found.");
    }
    return std::stoll(value);
}

std::optional<api::Json> find_officer(Database& database, std::int64_t user_id) {
    auto query = database.prepare(
        "SELECT user_id, username, status, created_at, updated_at FROM users "
        "WHERE user_id = ? AND role = 'bank_officer'");
    query->bind(1, user_id);
    if (!query->step()) return std::nullopt;
    return api::Json{
        {"userId", query->column_int(0)},
        {"username", column_string(*query, 1)},
        {"role", "bank_officer"},
        {"status", column_string(*query, 2)},
        {"createdAt", column_string(*query, 3)},
        {"updatedAt", column_string(*query, 4)},
    };
}

}  // namespace

void register_routes(api::Router& router, Database& database,
                     bool allow_initial_credential_delivery) {
    router.post("/staff", [&database, allow_initial_credential_delivery](
                              const api::Request& request) {
        const auth::Principal admin = require_administrator(request, database);
        if (!allow_initial_credential_delivery) {
            throw forbidden(
                "Creating staff with one-time credential delivery is disabled on "
                "network-accessible HTTP listeners. Enable HTTPS before using this route.");
        }
        const auto& body = request.json();
        for (auto it = body.begin(); it != body.end(); ++it) {
            if (it.key() != "username") throw validation_error(it.key(), "Unknown field.");
        }
        if (!body.contains("username")) {
            throw validation_error("username", "Username is required.");
        }
        const std::string username = validated_username(body["username"]);
        const std::string temporary_password = generate_temporary_password();
        const std::string password_hash = auth::hash_password(temporary_password);

        Database::Transaction transaction(database, true);
        reject_duplicate_username(database, username);
        auto create = database.prepare(
            "INSERT INTO users (username, role, status, failed_attempts, password_hash, "
            "must_change_password) VALUES (?, 'bank_officer', 'active', 0, ?, 1)");
        create->bind(1, username);
        create->bind(2, password_hash);
        create->step();
        auto last_id = database.prepare("SELECT last_insert_rowid()");
        last_id->step();
        const std::int64_t officer_id = last_id->column_int(0);
        audit::record(database, admin.user_id, "STAFF_CREATED", "users", officer_id,
                      "Created Bank Officer " + username);
        api::Json officer = *find_officer(database, officer_id);
        transaction.commit();

        officer["login"] = {{"username", username}, {"temporaryPassword", temporary_password}};
        api::Response response = api::Response::created(std::move(officer));
        response.headers.emplace("Location", "/staff/" + std::to_string(officer_id));
        return response;
    });

    router.get("/staff", [&database](const api::Request& request) {
        require_administrator(request, database);
        auto query = database.prepare(
            "SELECT user_id, username, status, created_at, updated_at FROM users "
            "WHERE role = 'bank_officer' ORDER BY username");
        api::Json officers = api::Json::array();
        while (query->step()) {
            officers.push_back({
                {"userId", query->column_int(0)},
                {"username", column_string(*query, 1)},
                {"role", "bank_officer"},
                {"status", column_string(*query, 2)},
                {"createdAt", column_string(*query, 3)},
                {"updatedAt", column_string(*query, 4)},
            });
        }
        return api::Response::ok({{"officers", std::move(officers)}});
    });

    router.patch("/staff/:id", [&database](const api::Request& request) {
        const auth::Principal admin = require_administrator(request, database);
        const std::int64_t officer_id = parse_id(request.param("id"));
        const auto& body = request.json();
        for (auto it = body.begin(); it != body.end(); ++it) {
            if (it.key() != "username" && it.key() != "status") {
                throw validation_error(it.key(), "Unknown field.");
            }
        }
        if (!body.contains("username") && !body.contains("status")) {
            throw validation_error("status", "Provide a new username or status.");
        }
        std::optional<std::string> username;
        if (body.contains("username")) username = validated_username(body["username"]);
        std::optional<std::string> status;
        if (body.contains("status")) {
            if (!body["status"].is_string() ||
                (body["status"] != "active" && body["status"] != "inactive")) {
                throw validation_error("status", "Status must be \"active\" or \"inactive\".");
            }
            status = body["status"].get<std::string>();
        }

        Database::Transaction transaction(database, true);
        const auto current = find_officer(database, officer_id);
        if (!current) throw not_found("Bank Officer not found.");

        std::string details;
        if (username && *username != (*current)["username"].get<std::string>()) {
            reject_duplicate_username(database, *username, officer_id);
            auto rename = database.prepare(
                "UPDATE users SET username = ?, updated_at = CURRENT_TIMESTAMP "
                "WHERE user_id = ?");
            rename->bind(1, *username);
            rename->bind(2, officer_id);
            rename->step();
            details += "username " + (*current)["username"].get<std::string>() + " -> " +
                       *username + "; ";
        }
        if (status && *status != (*current)["status"].get<std::string>()) {
            // Reactivating also clears any failed-attempt count.
            auto change = database.prepare(
                "UPDATE users SET status = ?, failed_attempts = CASE WHEN ? = 'active' "
                "THEN 0 ELSE failed_attempts END, updated_at = CURRENT_TIMESTAMP "
                "WHERE user_id = ?");
            change->bind(1, *status);
            change->bind(2, *status);
            change->bind(3, officer_id);
            change->step();
            details += "status " + (*current)["status"].get<std::string>() + " -> " + *status;
        }
        if (!details.empty()) {
            audit::record(database, admin.user_id,
                          status && *status == "inactive" ? "STAFF_DEACTIVATED" : "STAFF_UPDATED",
                          "users", officer_id, details);
        }
        api::Json officer = *find_officer(database, officer_id);
        transaction.commit();
        return api::Response::ok(std::move(officer));
    });

    router.get("/dashboard", [&database](const api::Request& request) {
        const auth::Principal principal = auth::authenticate(request, database);
        api::Json dashboard = {
            {"role", principal.role},
            {"userId", principal.user_id},
        };

        if (principal.role == "administrator") {
            auto customers = database.prepare(
                "SELECT COUNT(*) FROM customers WHERE status = 'active'");
            customers->step();
            dashboard["customerCount"] = customers->column_int(0);

            auto accounts = database.prepare(
                "SELECT COUNT(*) FROM accounts WHERE status = 'active'");
            accounts->step();
            dashboard["accountCount"] = accounts->column_int(0);

            auto staff = database.prepare(
                "SELECT COUNT(*) FROM users "
                "WHERE role = 'bank_officer' AND status = 'active'");
            staff->step();
            dashboard["staffCount"] = staff->column_int(0);

            auto transactions = database.prepare(
                "SELECT COUNT(*), COALESCE(SUM(amount), 0) FROM transactions "
                "WHERE date(created_at) = date('now')");
            transactions->step();
            dashboard["todayTransactionCount"] = transactions->column_int(0);
            dashboard["todayTransactionTotalPaise"] = transactions->column_int(1);
        } else if (principal.role == "bank_officer") {
            auto transactions = database.prepare(
                "SELECT COUNT(*), COALESCE(SUM(amount), 0) FROM transactions "
                "WHERE operator_id = ? AND date(created_at) = date('now')");
            transactions->bind(1, principal.user_id);
            transactions->step();
            dashboard["todayTransactionCount"] = transactions->column_int(0);
            dashboard["todayTransactionTotalPaise"] = transactions->column_int(1);

            auto recent = database.prepare(
                "SELECT transaction_id, account_id, type, amount, created_at "
                "FROM transactions WHERE operator_id = ? "
                "ORDER BY created_at DESC, transaction_id DESC LIMIT 10");
            recent->bind(1, principal.user_id);
            dashboard["recentTransactions"] = api::Json::array();
            while (recent->step()) {
                dashboard["recentTransactions"].push_back({
                    {"transactionId", recent->column_int(0)},
                    {"accountId", recent->column_int(1)},
                    {"type", column_string(*recent, 2)},
                    {"amountPaise", recent->column_int(3)},
                    {"createdAt", column_string(*recent, 4)},
                });
            }
        } else if (principal.role == "customer") {
            auto accounts = database.prepare(
                "SELECT a.account_id, a.account_number, at.type_name, a.balance, a.status "
                "FROM accounts a "
                "JOIN customers c ON c.customer_id = a.customer_id "
                "JOIN account_types at ON at.type_id = a.type_id "
                "WHERE c.user_id = ? ORDER BY a.account_id");
            accounts->bind(1, principal.user_id);
            dashboard["accounts"] = api::Json::array();
            while (accounts->step()) {
                dashboard["accounts"].push_back({
                    {"accountId", accounts->column_int(0)},
                    {"accountNumber", column_string(*accounts, 1)},
                    {"accountType", column_string(*accounts, 2)},
                    {"balancePaise", accounts->column_int(3)},
                    {"status", column_string(*accounts, 4)},
                });
            }
        } else {
            throw forbidden("This role does not have a dashboard.");
        }

        return api::Response::ok(std::move(dashboard));
    });
}

}  // namespace bms::staff
