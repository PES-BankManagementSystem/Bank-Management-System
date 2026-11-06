#include <cstdint>
#include <limits>
#include <string>

#include "account/account_opening.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "support/check.hpp"
#include "support/seeded_db.hpp"
#include "transaction/deposit.hpp"
#include "transaction/transfer.hpp"

namespace {

template <typename Operation>
std::string error_code(Operation&& operation) {
    try {
        operation();
    } catch (const bms::Error& error) {
        return error.code();
    }
    return {};
}

std::int64_t scalar(bms::Database& database, const std::string& sql) {
    auto query = database.prepare(sql);
    if (!query->step()) return -1;
    return query->column_int(0);
}

}  // namespace

int main() {
    using bms::test::run;

    run("closed databases reject operations with DbError", [] {
        bms::Database database(":memory:");
        database.close();

        CHECK(!database.is_open());
        CHECK_THROWS_AS(database.exec("SELECT 1"), bms::DbError);
        CHECK_THROWS_AS(database.prepare("SELECT 1"), bms::DbError);
        CHECK_THROWS_AS(database.begin(), bms::DbError);
        CHECK_THROWS_AS(database.commit(), bms::DbError);
        CHECK_THROWS_AS(database.rollback(), bms::DbError);
    });

    run("invalid account-number generator fails before creating rows", [] {
        bms::test::SeededDatabase seeded;
        auto& database = seeded.database();
        const auto accounts_before = scalar(database, "SELECT COUNT(*) FROM accounts");
        const auto transactions_before = scalar(database, "SELECT COUNT(*) FROM transactions");

        bool threw = false;
        try {
            (void)bms::account::open_account(
                database, {"CUS-00000001", 1, 100000}, 2,
                [] { return std::string("bad-account-number"); });
        } catch (const std::runtime_error&) {
            threw = true;
        }

        CHECK(threw);
        CHECK_EQ(scalar(database, "SELECT COUNT(*) FROM accounts"), accounts_before);
        CHECK_EQ(scalar(database, "SELECT COUNT(*) FROM transactions"), transactions_before);
    });

    run("customer permission refusal leaves deposit state unchanged", [] {
        bms::test::SeededDatabase seeded;
        auto& database = seeded.database();
        const auto balance_before = scalar(
            database, "SELECT balance FROM accounts WHERE account_number = 'ACC001'");
        const auto transactions_before = scalar(database, "SELECT COUNT(*) FROM transactions");

        const auto code = error_code([&] {
            (void)bms::transaction::deposit(database, "ACC001", "1.00", 4);
        });

        CHECK_EQ(code, std::string("FORBIDDEN"));
        CHECK_EQ(scalar(database,
                        "SELECT balance FROM accounts WHERE account_number = 'ACC001'"),
                 balance_before);
        CHECK_EQ(scalar(database, "SELECT COUNT(*) FROM transactions"), transactions_before);
    });

    run("deposit balance overflow is rejected without a transaction", [] {
        bms::test::SeededDatabase seeded;
        auto& database = seeded.database();
        database.exec("UPDATE accounts SET balance = 9223372036854775807 "
                      "WHERE account_number = 'ACC001'");
        const auto transactions_before = scalar(database, "SELECT COUNT(*) FROM transactions");

        const auto code = error_code([&] {
            (void)bms::transaction::deposit(database, "ACC001", "0.01", 2);
        });

        CHECK_EQ(code, std::string("BALANCE_LIMIT_EXCEEDED"));
        CHECK_EQ(scalar(database,
                        "SELECT balance FROM accounts WHERE account_number = 'ACC001'"),
                 std::numeric_limits<std::int64_t>::max());
        CHECK_EQ(scalar(database, "SELECT COUNT(*) FROM transactions"), transactions_before);
    });

    run("transfer destination overflow rolls back before any ledger write", [] {
        bms::test::SeededDatabase seeded;
        auto& database = seeded.database();
        database.exec("UPDATE accounts SET status = 'active', balance = 9223372036854775807 "
                      "WHERE account_number = 'ACC002'");
        const auto source_before = scalar(
            database, "SELECT balance FROM accounts WHERE account_number = 'ACC001'");
        const auto transactions_before = scalar(database, "SELECT COUNT(*) FROM transactions");

        const auto code = error_code([&] {
            (void)bms::transaction::transfer(database, "ACC001", "ACC002", "0.01", 2);
        });

        CHECK_EQ(code, std::string("BALANCE_LIMIT_EXCEEDED"));
        CHECK_EQ(scalar(database,
                        "SELECT balance FROM accounts WHERE account_number = 'ACC001'"),
                 source_before);
        CHECK_EQ(scalar(database,
                        "SELECT balance FROM accounts WHERE account_number = 'ACC002'"),
                 std::numeric_limits<std::int64_t>::max());
        CHECK_EQ(scalar(database, "SELECT COUNT(*) FROM transactions"), transactions_before);
    });

    return bms::test::finish();
}
