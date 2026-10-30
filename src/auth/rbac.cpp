#include "auth/rbac.hpp"

#include <string>

#include "api/router.hpp"
#include "audit/record.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::auth::rbac {
namespace {

constexpr const char* kAdministrator = "administrator";
constexpr const char* kBankOfficer = "bank_officer";
constexpr const char* kCustomer = "customer";

// Records the refusal in its own transaction, then reports 403. A failure to
// write the entry never turns a refusal into an error of a different kind.
[[noreturn]] void deny(Database& database, std::int64_t user_id, const std::string& details) {
    try {
        Database::Transaction transaction(database, true);
        audit::record(database, user_id, "ACCESS_DENIED", "users", user_id, details);
        transaction.commit();
    } catch (...) {
    }
    throw forbidden();
}

std::string role_of(Database& database, std::int64_t user_id) {
    auto query = database.prepare("SELECT role FROM users WHERE user_id = ?");
    query->bind(1, user_id);
    if (!query->step()) return {};
    const auto* role = query->column_text(0);
    return role == nullptr ? std::string{} : reinterpret_cast<const char*>(role);
}

}  // namespace

const char* name(Operation operation) {
    switch (operation) {
        case Operation::RegisterCustomer: return "REGISTER_CUSTOMER";
        case Operation::UpdateCustomer: return "UPDATE_CUSTOMER";
        case Operation::DeactivateCustomer: return "DEACTIVATE_CUSTOMER";
        case Operation::OpenAccount: return "OPEN_ACCOUNT";
        case Operation::CloseAccount: return "CLOSE_ACCOUNT";
        case Operation::FreezeAccount: return "FREEZE_ACCOUNT";
        case Operation::Deposit: return "DEPOSIT";
        case Operation::Withdraw: return "WITHDRAW";
        case Operation::Transfer: return "TRANSFER";
        case Operation::ViewCustomer: return "VIEW_CUSTOMER";
        case Operation::ViewAccount: return "VIEW_ACCOUNT";
        case Operation::RunInterest: return "RUN_INTEREST";
        case Operation::ManageStaff: return "MANAGE_STAFF";
        case Operation::ViewAccountTypes: return "VIEW_ACCOUNT_TYPES";
        case Operation::ManageAccountTypes: return "MANAGE_ACCOUNT_TYPES";
        case Operation::UnlockUser: return "UNLOCK_USER";
        case Operation::ViewAudit: return "VIEW_AUDIT";
    }
    return "UNKNOWN";
}

bool is_allowed(Operation operation, const std::string& role) {
    switch (operation) {
        case Operation::RegisterCustomer:
        case Operation::UpdateCustomer:
        case Operation::OpenAccount:
        case Operation::CloseAccount:
        case Operation::FreezeAccount:
        case Operation::Deposit:
        case Operation::Withdraw:
        case Operation::Transfer:
            return role == kAdministrator || role == kBankOfficer;
        case Operation::DeactivateCustomer:
            return role == kAdministrator;
        case Operation::ViewCustomer:
        case Operation::ViewAccount:
            return role == kAdministrator || role == kBankOfficer || role == kCustomer;
        case Operation::RunInterest:
        case Operation::ManageStaff:
        case Operation::UnlockUser:
        case Operation::ViewAudit:
            return role == kAdministrator;
        case Operation::ViewAccountTypes:
            return role == kAdministrator || role == kBankOfficer;
        case Operation::ManageAccountTypes:
            return role == kAdministrator;
    }
    return false;
}

void require_role(Database& database, std::int64_t user_id, Operation operation) {
    const std::string role = role_of(database, user_id);
    if (!is_allowed(operation, role)) {
        // An unknown user id has no audit row to attach to (foreign key).
        if (role.empty()) throw forbidden();
        deny(database, user_id,
             std::string("Role ") + role + " refused " + name(operation) + ".");
    }
}

void require_permission(const api::Request& request, Database& database,
                        const Principal& principal, Operation operation) {
    if (!is_allowed(operation, principal.role)) {
        deny(database, principal.user_id,
             std::string("Role ") + principal.role + " refused " + name(operation) + " (" +
                 request.method() + " " + request.path() + ").");
    }
}

Principal require(const api::Request& request, Database& database, Operation operation) {
    const Principal principal = authenticate(request, database);
    require_permission(request, database, principal, operation);
    return principal;
}

bool owns_customer_record(Database& database, std::int64_t user_id, std::int64_t customer_id) {
    auto query = database.prepare(
        "SELECT 1 FROM customers WHERE customer_id = ? AND user_id = ?");
    query->bind(1, customer_id);
    query->bind(2, user_id);
    return query->step();
}

bool owns_account(Database& database, std::int64_t user_id, const std::string& account_number) {
    auto query = database.prepare(
        "SELECT 1 FROM accounts a JOIN customers c ON c.customer_id = a.customer_id "
        "WHERE a.account_number = ? AND c.user_id = ?");
    query->bind(1, account_number);
    query->bind(2, user_id);
    return query->step();
}

void require_customer_access(Database& database, const Principal& principal,
                             std::int64_t customer_id) {
    if (principal.role != kCustomer) return;
    if (!owns_customer_record(database, principal.user_id, customer_id)) {
        deny(database, principal.user_id,
             "Customer tried to reach customer record " + std::to_string(customer_id) + ".");
    }
}

void require_account_access(Database& database, const Principal& principal,
                            const std::string& account_number) {
    if (principal.role != kCustomer) return;
    if (!owns_account(database, principal.user_id, account_number)) {
        deny(database, principal.user_id, "Customer tried to reach account " + account_number + ".");
    }
}

}  // namespace bms::auth::rbac
