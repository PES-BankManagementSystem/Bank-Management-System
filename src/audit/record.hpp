#pragma once

#include <cstdint>
#include <string>

namespace bms {
class Database;
}

namespace bms::audit {

// Appends one entry to the audit trail (NFR-5, SEC-4). It must be called inside
// the transaction that makes the change it describes, so the entry and the
// change commit or roll back together; it throws std::logic_error otherwise.
// The audit_log table has triggers that refuse UPDATE and DELETE, and this
// module offers no way to edit or remove an entry.
//
// operator_id: the user who performed the action
// action:      stable upper-case code, e.g. "DEPOSIT"
// entity:      the table the change affected, e.g. "accounts"
// entity_id:   the primary key of the affected row
// details:     free text with the amount and any other context
void record(Database& database, std::int64_t operator_id, const std::string& action,
            const std::string& entity, std::int64_t entity_id, const std::string& details);

}  // namespace bms::audit
