#include "staff/routes.hpp"

#include "api/router.hpp"
#include "auth/auth_service.hpp"
#include "auth/middleware.hpp"
#include "common/database.hpp"
#include "common/error.hpp"
#include "common/log.hpp"

namespace bms::staff {

namespace {

using api::Handler;
using api::Request;
using api::Response;
using Json = nlohmann::json;

// POST /staff  (S-24, Administrator only) – create a new Bank Officer.
Response handle_create_officer(const Request& req) {
    auth::AuthUser admin = auth::require_admin(req);
    const Json& body = req.json();

    if (!body.contains("id") || !body["id"].is_string() || body["id"].get<std::string>().empty()) {
        throw validation_error("id", "Officer ID is required.");
    }
    if (!body.contains("name") || !body["name"].is_string() || body["name"].get<std::string>().empty()) {
        throw validation_error("name", "Officer name is required.");
    }
    if (!body.contains("password") || !body["password"].is_string() || body["password"].get<std::string>().size() < 6) {
        throw validation_error("password", "Password must be at least 6 characters.");
    }

    const std::string id = body["id"].get<std::string>();
    const std::string name = body["name"].get<std::string>();
    const std::string password = body["password"].get<std::string>();
    const std::string hash = auth::hash_password(password);

    auto& db = Database::instance();

    // Check for duplicate ID.
    auto check = db.prepare("SELECT id FROM users WHERE id = ?1");
    check.bind(1, id);
    if (check.step()) {
        throw conflict("DUPLICATE_USER", "A user with ID '" + id + "' already exists.");
    }

    auto stmt = db.prepare(
        "INSERT INTO users (id, name, role, password_hash) VALUES (?1, ?2, 'bank_officer', ?3)");
    stmt.bind(1, id).bind(2, name).bind(3, hash);
    stmt.execute();

    log::info("Bank Officer created by " + admin.id + ": " + id);

    return Response::created({{"id", id}, {"name", name}, {"role", "bank_officer"}});
}

// GET /staff  (S-24, Administrator only) – list all Bank Officers.
Response handle_list_officers(const Request& req) {
    auth::require_admin(req);

    auto& db = Database::instance();
    auto stmt = db.prepare(
        "SELECT id, name, active, locked, created_at, updated_at "
        "FROM users WHERE role = 'bank_officer' ORDER BY name");

    Json officers = Json::array();
    while (stmt.step()) {
        officers.push_back({
            {"id", stmt.column_text(0)},
            {"name", stmt.column_text(1)},
            {"active", stmt.column_int(2) == 1},
            {"locked", stmt.column_int(3) == 1},
            {"created_at", stmt.column_text(4)},
            {"updated_at", stmt.column_text(5)}
        });
    }

    return Response::ok({{"officers", officers}});
}

// PATCH /staff/:id  (S-24, Administrator only) – update or deactivate a Bank Officer.
Response handle_update_officer(const Request& req) {
    auth::AuthUser admin = auth::require_admin(req);
    const std::string officer_id = req.param("id");
    const Json& body = req.json();

    auto& db = Database::instance();

    // Check the officer exists and is a bank_officer.
    auto check = db.prepare("SELECT id, role FROM users WHERE id = ?1");
    check.bind(1, officer_id);
    if (!check.step()) {
        throw not_found("Officer '" + officer_id + "' not found.");
    }
    if (check.column_text(1) != "bank_officer") {
        throw forbidden("Only Bank Officer accounts can be managed here.");
    }

    // Build the update dynamically based on which fields are provided.
    if (body.contains("name") && body["name"].is_string()) {
        auto stmt = db.prepare(
            "UPDATE users SET name = ?1, updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') "
            "WHERE id = ?2");
        stmt.bind(1, body["name"].get<std::string>()).bind(2, officer_id);
        stmt.execute();
    }

    if (body.contains("active") && body["active"].is_boolean()) {
        int active = body["active"].get<bool>() ? 1 : 0;
        auto stmt = db.prepare(
            "UPDATE users SET active = ?1, updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') "
            "WHERE id = ?2");
        stmt.bind(1, active).bind(2, officer_id);
        stmt.execute();

        if (!active) {
            // Invalidate all sessions for the deactivated officer.
            auto del = db.prepare("DELETE FROM sessions WHERE user_id = ?1");
            del.bind(1, officer_id);
            del.execute();
            log::info("Officer deactivated and sessions cleared by " + admin.id + ": " + officer_id);
        }
    }

    if (body.contains("password") && body["password"].is_string()) {
        const std::string pw = body["password"].get<std::string>();
        if (pw.size() < 6) {
            throw validation_error("password", "Password must be at least 6 characters.");
        }
        auto stmt = db.prepare(
            "UPDATE users SET password_hash = ?1, "
            "updated_at = strftime('%Y-%m-%dT%H:%M:%SZ', 'now') WHERE id = ?2");
        stmt.bind(1, auth::hash_password(pw)).bind(2, officer_id);
        stmt.execute();
    }

    return Response::ok({{"message", "Officer updated."}, {"id", officer_id}});
}

// GET /dashboard  (S-25) – returns dashboard data specific to the caller's role.
Response handle_dashboard(const Request& req) {
    auth::AuthUser user = auth::require_auth(req);
    auto& db = Database::instance();

    Json data = {{"role", user.role}, {"user_id", user.id}, {"user_name", user.name}};

    if (user.role == "administrator") {
        // Admin dashboard: counts of customers, accounts, staff, and today's totals.
        {
            auto stmt = db.prepare("SELECT COUNT(*) FROM users WHERE role = 'customer' AND active = 1");
            stmt.step();
            data["customer_count"] = stmt.column_int(0);
        }
        {
            auto stmt = db.prepare("SELECT COUNT(*) FROM accounts WHERE active = 1");
            stmt.step();
            data["account_count"] = stmt.column_int(0);
        }
        {
            auto stmt = db.prepare("SELECT COUNT(*) FROM users WHERE role = 'bank_officer' AND active = 1");
            stmt.step();
            data["staff_count"] = stmt.column_int(0);
        }
        {
            auto stmt = db.prepare(
                "SELECT COALESCE(SUM(amount), 0), COUNT(*) FROM transactions "
                "WHERE date(created_at) = date('now')");
            stmt.step();
            data["today_transaction_total"] = stmt.column_double(0);
            data["today_transaction_count"] = stmt.column_int(1);
        }
    } else if (user.role == "bank_officer") {
        // Officer dashboard: transactions they processed today.
        {
            auto stmt = db.prepare(
                "SELECT COUNT(*), COALESCE(SUM(amount), 0) FROM transactions "
                "WHERE performed_by = ?1 AND date(created_at) = date('now')");
            stmt.bind(1, user.id);
            stmt.step();
            data["today_transaction_count"] = stmt.column_int(0);
            data["today_transaction_total"] = stmt.column_double(1);
        }
        // Recent transactions for quick reference.
        {
            auto stmt = db.prepare(
                "SELECT id, account_id, type, amount, created_at FROM transactions "
                "WHERE performed_by = ?1 ORDER BY created_at DESC LIMIT 10");
            stmt.bind(1, user.id);
            Json txns = Json::array();
            while (stmt.step()) {
                txns.push_back({
                    {"id", stmt.column_int(0)},
                    {"account_id", stmt.column_text(1)},
                    {"type", stmt.column_text(2)},
                    {"amount", stmt.column_double(3)},
                    {"created_at", stmt.column_text(4)}
                });
            }
            data["recent_transactions"] = txns;
        }
    } else if (user.role == "customer") {
        // Customer dashboard: their accounts and balances.
        {
            auto stmt = db.prepare(
                "SELECT id, account_type, balance, active FROM accounts "
                "WHERE customer_id = ?1 ORDER BY id");
            stmt.bind(1, user.id);
            Json accounts = Json::array();
            while (stmt.step()) {
                accounts.push_back({
                    {"id", stmt.column_text(0)},
                    {"account_type", stmt.column_text(1)},
                    {"balance", stmt.column_double(2)},
                    {"active", stmt.column_int(3) == 1}
                });
            }
            data["accounts"] = accounts;
        }
    }

    return Response::ok(data);
}

// GET /staff/all-users  (Administrator only) – list all users for admin unlock UI.
Response handle_list_all_users(const Request& req) {
    auth::require_admin(req);

    auto& db = Database::instance();
    auto stmt = db.prepare(
        "SELECT id, name, role, active, locked, failed_attempts "
        "FROM users ORDER BY role, name");

    Json users = Json::array();
    while (stmt.step()) {
        users.push_back({
            {"id", stmt.column_text(0)},
            {"name", stmt.column_text(1)},
            {"role", stmt.column_text(2)},
            {"active", stmt.column_int(3) == 1},
            {"locked", stmt.column_int(4) == 1},
            {"failed_attempts", stmt.column_int(5)}
        });
    }

    return Response::ok({{"users", users}});
}

}  // namespace

void register_routes(api::Router& router) {
    // S-24: Staff management (Administrator only).
    router.post("/staff", handle_create_officer);
    router.get("/staff", handle_list_officers);
    router.patch("/staff/:id", handle_update_officer);

    // S-25: Role-specific dashboard.
    router.get("/dashboard", handle_dashboard);

    // Admin utility: list all users for the unlock UI.
    router.get("/staff/all-users", handle_list_all_users);
}

}  // namespace bms::staff
