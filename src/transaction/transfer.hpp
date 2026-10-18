#pragma once

#include <cstdint>
#include <string>

namespace bms {
class Database;
}

namespace bms::transaction {

struct TransferResult {
    std::string transfer_reference;
    std::int64_t source_transaction_id;
    std::int64_t destination_transaction_id;
    std::int64_t source_account_id;
    std::int64_t destination_account_id;
    std::int64_t operator_id;
    std::int64_t amount_paise;
    std::int64_t source_balance_paise;
    std::int64_t destination_balance_paise;
    std::string created_at;
};

// Produces a readable reference from a transaction timestamp and unique ID.
std::string make_transfer_reference(const std::string& created_at,
                                    std::int64_t source_transaction_id);

TransferResult transfer(Database& database, const std::string& from_account_number,
                        const std::string& to_account_number, const std::string& amount,
                        std::int64_t operator_id);

}  // namespace bms::transaction
