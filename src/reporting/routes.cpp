#include "reporting/routes.hpp"

#include <utility>

#include <nlohmann/json.hpp>

#include "api/router.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "reporting/interest.hpp"
#include "transaction/deposit.hpp"

namespace bms::reporting {

void register_routes(api::Router& router, Database& database) {
    router.post("/interest/run", [&database](const api::Request& request) {
        const std::int64_t administrator_id =
            auth::rbac::require(request, database, auth::rbac::Operation::RunInterest).user_id;
        const auto& body = request.json();
        if (!body.contains("period") || !body["period"].is_string()) {
            throw validation_error("period", "Period must use YYYY-MM format.");
        }
        const InterestRunResult result = run_interest(
            database, body["period"].get<std::string>(), administrator_id);
        nlohmann::json accounts = nlohmann::json::array();
        for (const InterestAccountResult& account : result.accounts) {
            accounts.push_back({
                {"account_number", account.account_number},
                {"balance_basis", transaction::format_money(account.balance_basis_paise)},
                {"interest", transaction::format_money(account.interest_paise)},
                {"resulting_balance", transaction::format_money(account.resulting_balance_paise)},
            });
        }
        return api::Response::created({
            {"period", result.period},
            {"accounts_credited", result.accounts_credited},
            {"total_interest", transaction::format_money(result.total_interest_paise)},
            {"accounts", std::move(accounts)},
        });
    });
}

}  // namespace bms::reporting
