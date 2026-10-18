#include "transaction/routes.hpp"

#include "common/db.hpp"
#include "api/router.hpp"
#include "common/error.hpp"
#include "transaction/deposit.hpp"
#include "transaction/withdraw.hpp"

namespace bms::transaction {

void register_routes(api::Router& router, Database& database, std::int64_t operator_id) {
    router.post("/transactions", [&database, operator_id](const api::Request& request) {
        const auto& body = request.json();
        if (!body.contains("type") || !body["type"].is_string()) {
            throw validation_error("type", "Transaction type is required.");
        }
        if (body["type"] != "deposit" && body["type"] != "withdraw") {
            throw validation_error("type", "Only deposit and withdrawal transactions are supported.");
        }
        if (!body.contains("account_number") || !body["account_number"].is_string()) {
            throw validation_error("account_number", "Account number is required.");
        }
        if (!body.contains("amount") || !body["amount"].is_string()) {
            throw validation_error("amount", "Amount must be a decimal string.");
        }

        const std::string account_number = body["account_number"].get<std::string>();
        const std::string amount = body["amount"].get<std::string>();
        const bool is_withdrawal = body["type"] == "withdraw";
        const DepositResult result = is_withdrawal
            ? withdraw(database, account_number, amount, operator_id)
            : deposit(database, account_number, amount, operator_id);
        return api::Response::created({
            {"balance", format_money(result.resulting_balance_paise)},
            {"transaction", {{"transaction_id", result.transaction_id},
                             {"account_id", result.account_id},
                             {"account_number", body["account_number"]},
                             {"type", is_withdrawal ? "withdrawal" : "deposit"},
                             {"amount", format_money(result.amount_paise)},
                             {"resulting_balance", format_money(result.resulting_balance_paise)},
                             {"operator_id", result.operator_id},
                             {"created_at", result.created_at}}},
        });
    });
}

}  // namespace bms::transaction
