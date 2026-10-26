#include "transaction/routes.hpp"

#include "api/router.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "transaction/deposit.hpp"
#include "transaction/withdraw.hpp"
#include "transaction/transfer.hpp"

namespace bms::transaction {

void register_routes(api::Router& router, Database& database) {
    router.post("/transactions", [&database](const api::Request& request) {
        const auth::Principal principal = auth::authenticate(request, database);
        // A caller who may do neither is refused before the body is read.
        if (!auth::rbac::is_allowed(auth::rbac::Operation::Deposit, principal.role) &&
            !auth::rbac::is_allowed(auth::rbac::Operation::Withdraw, principal.role)) {
            auth::rbac::require_permission(request, database, principal,
                                           auth::rbac::Operation::Deposit);
        }
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
        auth::rbac::require_permission(request, database, principal,
                                       is_withdrawal ? auth::rbac::Operation::Withdraw
                                                     : auth::rbac::Operation::Deposit);
        const std::int64_t operator_id = principal.user_id;
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

    router.post("/transfers", [&database](const api::Request& request) {
        const std::int64_t operator_id =
            auth::rbac::require(request, database, auth::rbac::Operation::Transfer).user_id;
        const auto& body = request.json();
        if (!body.contains("from_account_number") || !body["from_account_number"].is_string()) {
            throw validation_error("from_account_number", "Source account number is required.");
        }
        if (!body.contains("to_account_number") || !body["to_account_number"].is_string()) {
            throw validation_error("to_account_number", "Destination account number is required.");
        }
        if (!body.contains("amount") || !body["amount"].is_string()) {
            throw validation_error("amount", "Amount must be a decimal string.");
        }

        const std::string from_account_number = body["from_account_number"].get<std::string>();
        const std::string to_account_number = body["to_account_number"].get<std::string>();
        const TransferResult result = transfer(database, from_account_number, to_account_number,
                                               body["amount"].get<std::string>(), operator_id);
        const std::string formatted_amount = format_money(result.amount_paise);
        return api::Response::created({
            {"transfer_reference", result.transfer_reference},
            {"from_balance", format_money(result.source_balance_paise)},
            {"to_balance", format_money(result.destination_balance_paise)},
            {"transactions", {
                {"debit", {{"transaction_id", result.source_transaction_id},
                            {"account_id", result.source_account_id},
                            {"account_number", from_account_number},
                            {"type", "transfer_debit"},
                            {"amount", formatted_amount},
                            {"resulting_balance", format_money(result.source_balance_paise)},
                            {"operator_id", result.operator_id},
                            {"transfer_ref", result.transfer_reference},
                            {"created_at", result.created_at}}},
                {"credit", {{"transaction_id", result.destination_transaction_id},
                             {"account_id", result.destination_account_id},
                             {"account_number", to_account_number},
                             {"type", "transfer_credit"},
                             {"amount", formatted_amount},
                             {"resulting_balance", format_money(result.destination_balance_paise)},
                             {"operator_id", result.operator_id},
                             {"transfer_ref", result.transfer_reference},
                             {"created_at", result.created_at}}},
            }},
        });
    });
}

}  // namespace bms::transaction
