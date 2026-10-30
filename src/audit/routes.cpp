#include "audit/routes.hpp"

#include <cstdint>
#include <string>
#include <utility>

#include "api/router.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "common/input_validation.hpp"

namespace bms::audit {
namespace {

constexpr std::int64_t kDefaultLimit = 100;
constexpr std::int64_t kMaxLimit = 500;

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

// Accepts only a real calendar date written as YYYY-MM-DD.
std::string parse_date(Database& database, const api::Request& request, const std::string& name) {
    const auto value = request.query(name);
    if (!value || value->empty()) return {};
    auto check = database.prepare("SELECT date(?)");
    check->bind(1, *value);
    check->step();
    if (value->size() != 10 || column_string(*check, 0) != *value) {
        throw validation_error(name, "Use a date written as YYYY-MM-DD.");
    }
    return *value;
}

std::int64_t parse_count(const api::Request& request, const std::string& name,
                         std::int64_t fallback, std::int64_t minimum, std::int64_t maximum) {
    const auto value = request.query(name);
    if (!value || value->empty()) return fallback;
    if (value->size() > 9 || value->find_first_not_of("0123456789") != std::string::npos) {
        throw validation_error(name, "Must be a whole number.");
    }
    const std::int64_t number = std::stoll(*value);
    if (number < minimum || number > maximum) {
        throw validation_error(name, "Must be between " + std::to_string(minimum) + " and " +
                                         std::to_string(maximum) + ".");
    }
    return number;
}

api::Response read_only(const api::Request&) {
    api::Response response;
    response.status = 405;
    response.body = {{"code", "METHOD_NOT_ALLOWED"},
                     {"message", "The audit trail is read-only; entries cannot be changed or removed."}};
    response.headers.emplace("Allow", "GET");
    return response;
}

}  // namespace

void register_routes(api::Router& router, Database& database,
                     RequireAdministrator require_administrator) {
    router.get("/audit", [&database, require_administrator = std::move(require_administrator)](
                             const api::Request& request) {
        require_administrator(request);

        const std::string from = parse_date(database, request, "from");
        const std::string to = parse_date(database, request, "to");
        if (!from.empty() && !to.empty() && from > to) {
            throw validation_error("from", "The start date must not be after the end date.");
        }
        const std::int64_t limit = parse_count(request, "limit", kDefaultLimit, 1, kMaxLimit);
        const std::int64_t offset = parse_count(request, "offset", 0, 0, 999999999);

        const auto account_number = request.query("account");
        std::int64_t account_id = 0;
        const bool filter_account = account_number && !account_number->empty();
        if (filter_account) {
            validate_account_number(*account_number, "account");
            auto lookup = database.prepare("SELECT account_id FROM accounts WHERE account_number = ?");
            lookup->bind(1, *account_number);
            if (!lookup->step()) throw not_found("Account " + *account_number + " was not found.");
            account_id = lookup->column_int(0);
        }

        auto query = database.prepare(
            "SELECT l.log_id, l.timestamp, l.user_id, u.username, l.action, l.entity, "
            "l.entity_id, a.account_number, l.details "
            "FROM audit_log l JOIN users u ON u.user_id = l.user_id "
            "LEFT JOIN accounts a ON l.entity = 'accounts' AND a.account_id = l.entity_id "
            "WHERE (?1 IS NULL OR date(l.timestamp) >= ?1) "
            "AND (?2 IS NULL OR date(l.timestamp) <= ?2) "
            "AND (?3 IS NULL OR (l.entity = 'accounts' AND l.entity_id = ?3)) "
            "ORDER BY l.log_id DESC LIMIT ?4 OFFSET ?5");
        if (from.empty()) query->bind(1, nullptr); else query->bind(1, from);
        if (to.empty()) query->bind(2, nullptr); else query->bind(2, to);
        if (filter_account) query->bind(3, account_id); else query->bind(3, nullptr);
        query->bind(4, limit);
        query->bind(5, offset);

        api::Json entries = api::Json::array();
        while (query->step()) {
            entries.push_back({
                {"logId", query->column_int(0)},
                {"timestamp", column_string(*query, 1)},
                {"operatorId", query->column_int(2)},
                {"operator", column_string(*query, 3)},
                {"action", column_string(*query, 4)},
                {"entity", column_string(*query, 5)},
                {"entityId", query->column_int(6)},
                {"accountNumber", query->column_null(7) ? api::Json(nullptr)
                                                         : api::Json(column_string(*query, 7))},
                {"details", column_string(*query, 8)},
            });
        }
        return api::Response::ok({{"entries", std::move(entries)}, {"limit", limit}, {"offset", offset}});
    });

    // No route changes or removes an entry (NFR-5, TC-16); say so explicitly
    // instead of answering 404.
    for (const char* pattern : {"/audit", "/audit/:id"}) {
        router.post(pattern, read_only);
        router.put(pattern, read_only);
        router.patch(pattern, read_only);
        router.del(pattern, read_only);
    }
}

}  // namespace bms::audit
