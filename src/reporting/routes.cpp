#include "reporting/routes.hpp"

#include <utility>

#include <nlohmann/json.hpp>

#include "api/router.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "reporting/interest.hpp"
#include "reporting/statement.hpp"
#include "transaction/deposit.hpp"

namespace bms::reporting {
namespace {

std::string format_signed_money(std::int64_t amount_paise) {
    if (amount_paise < 0) return "-" + transaction::format_money(-amount_paise);
    return transaction::format_money(amount_paise);
}

}  // namespace

void register_routes(api::Router& router, Database& database) {
    router.get("/accounts/:acc/statement", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::ViewAccount);
        const std::string& account_number = request.param("acc");
        auth::rbac::require_account_access(database, principal, account_number);

        const auto from = request.query("from");
        const auto to = request.query("to");
        if (!from) throw validation_error("from", "From date is required in YYYY-MM-DD format.");
        if (!to) throw validation_error("to", "To date is required in YYYY-MM-DD format.");

        const AccountStatement statement =
            get_account_statement(database, account_number, *from, *to);
        nlohmann::json transactions = nlohmann::json::array();
        for (const StatementEntry& entry : statement.transactions) {
            transactions.push_back({
                {"transaction_id", entry.transaction_id},
                {"type", entry.type},
                {"amount", format_signed_money(entry.amount_paise)},
                {"resulting_balance", transaction::format_money(entry.resulting_balance_paise)},
                {"created_at", entry.created_at},
            });
        }
        return api::Response::ok({
            {"account_number", statement.account_number},
            {"from", statement.from},
            {"to", statement.to},
            {"opening_balance", transaction::format_money(statement.opening_balance_paise)},
            {"transactions", std::move(transactions)},
            {"closing_balance", transaction::format_money(statement.closing_balance_paise)},
        });
    });

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
