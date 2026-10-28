#include "audit/record.hpp"

#include <stdexcept>

#include "common/db.hpp"

namespace bms::audit {

void record(Database& database, std::int64_t operator_id, const std::string& action,
            const std::string& entity, std::int64_t entity_id, const std::string& details) {
    if (!database.in_transaction()) {
        throw std::logic_error("audit::record must be called inside the transaction it describes.");
    }
    auto insert = database.prepare(
        "INSERT INTO audit_log (user_id, action, entity, entity_id, details) "
        "VALUES (?, ?, ?, ?, ?)");
    insert->bind(1, operator_id);
    insert->bind(2, action);
    insert->bind(3, entity);
    insert->bind(4, entity_id);
    insert->bind(5, details);
    insert->step();
}

}  // namespace bms::audit
