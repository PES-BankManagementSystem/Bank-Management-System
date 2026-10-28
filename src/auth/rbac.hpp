#pragma once

#include <cstdint>
#include <string>

#include "auth/routes.hpp"

namespace bms::api {
class Request;
}

namespace bms {
class Database;
}

// Role-based access control (FR-1, NFR-4, SEC-1, SRS 5.5). One permission
// table decides which roles may perform each operation. The check is made in
// the core as well as at the API layer, so a caller that reaches a core
// function without going through HTTP is held to the same rules.
namespace bms::auth::rbac {

enum class Operation {
    // Customers and accounts (SRS 5.5): Bank Officer or Administrator.
    RegisterCustomer,
    UpdateCustomer,
    DeactivateCustomer,
    OpenAccount,
    CloseAccount,
    FreezeAccount,
    // Anything that changes a balance: Bank Officer or Administrator, never a Customer.
    Deposit,
    Withdraw,
    Transfer,
    // Read access. A Customer is limited to their own records (see require_*_access).
    ViewCustomer,
    ViewAccount,
    // Administrator only.
    RunInterest,
    ManageStaff,
    ManageAccountTypes,
    UnlockUser,
    ViewAudit,
};

// Stable upper-case name used in audit entries, e.g. "DEPOSIT".
const char* name(Operation operation);

// The permission table: may a user with this role perform the operation?
bool is_allowed(Operation operation, const std::string& role);

// Core check. Looks up the user's current role and throws a 403 bms::Error if
// it may not perform the operation. A refusal is recorded in the audit trail
// (action ACCESS_DENIED). Do not call it inside an open transaction.
void require_role(Database& database, std::int64_t user_id, Operation operation);

// API check: authenticates the bearer token (401 if missing or expired), then
// require_permission(). Returns the signed-in user.
Principal require(const api::Request& request, Database& database, Operation operation);

// The second half of require(), for a handler that has to authenticate before
// it can tell which operation was asked for.
void require_permission(const api::Request& request, Database& database,
                        const Principal& principal, Operation operation);

// Ownership: a Customer's user is linked to one customer record, which holds
// the accounts. Staff roles are not limited by ownership.
bool owns_customer_record(Database& database, std::int64_t user_id, std::int64_t customer_id);
bool owns_account(Database& database, std::int64_t user_id, const std::string& account_number);

// A Customer may only reach their own customer record or account; anyone else
// is refused with 403 and the attempt is recorded. Bank Officers and
// Administrators pass (their role is checked by require()).
void require_customer_access(Database& database, const Principal& principal, std::int64_t customer_id);
void require_account_access(Database& database, const Principal& principal,
                            const std::string& account_number);

}  // namespace bms::auth::rbac
