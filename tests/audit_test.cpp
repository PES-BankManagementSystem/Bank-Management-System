// S-26 (#48): every balance change is recorded in the append-only audit trail
// inside the same database transaction (NFR-5, SEC-4, TC-16).

#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "audit/record.hpp"
#include "common/db.hpp"
#include "support/seeded_database.hpp"
#include "transaction/deposit.hpp"
#include "transaction/withdraw.hpp"

namespace {

class AuditTest : public bms::test::SeededDatabaseTest {
protected:
    std::int64_t count(const std::string& sql) {
        auto query = db().prepare(sql);
        query->step();
        return query->column_int(0);
    }

    std::int64_t balance(const std::string& account_number) {
        auto query = db().prepare("SELECT balance FROM accounts WHERE account_number = ?");
        query->bind(1, account_number);
        query->step();
        return query->column_int(0);
    }
};

std::string text(const bms::Statement& row, int index) {
    return reinterpret_cast<const char*>(row.column_text(index));
}

TEST_F(AuditTest, RecordStoresOperatorActionAccountAndAmount) {
    const auto before = count("SELECT COUNT(*) FROM audit_log");
    {
        bms::Database::Transaction transaction(db(), true);
        bms::audit::record(db(), 2, "DEPOSIT", "accounts", 1, "Deposited 500 paise.");
        transaction.commit();
    }
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), before + 1);

    auto row = db().prepare(
        "SELECT user_id, action, entity, entity_id, details, timestamp FROM audit_log "
        "ORDER BY log_id DESC LIMIT 1");
    ASSERT_TRUE(row->step());
    EXPECT_EQ(row->column_int(0), 2);
    EXPECT_EQ(text(*row, 1), "DEPOSIT");
    EXPECT_EQ(text(*row, 2), "accounts");
    EXPECT_EQ(row->column_int(3), 1);
    EXPECT_EQ(text(*row, 4), "Deposited 500 paise.");
    EXPECT_FALSE(text(*row, 5).empty());
}

TEST_F(AuditTest, RecordOutsideATransactionIsRefused) {
    const auto before = count("SELECT COUNT(*) FROM audit_log");
    EXPECT_THROW(bms::audit::record(db(), 2, "DEPOSIT", "accounts", 1, "x"), std::logic_error);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), before);
}

TEST_F(AuditTest, EntryIsDiscardedWhenTheTransactionRollsBack) {
    const auto before = count("SELECT COUNT(*) FROM audit_log");
    {
        bms::Database::Transaction transaction(db(), true);
        bms::audit::record(db(), 2, "DEPOSIT", "accounts", 1, "x");
        transaction.rollback();
    }
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), before);
}

TEST_F(AuditTest, EntriesCannotBeUpdatedOrDeleted) {
    ASSERT_GT(count("SELECT COUNT(*) FROM audit_log"), 0);
    EXPECT_THROW(db().exec("UPDATE audit_log SET action = 'EDITED'"), bms::DbError);
    EXPECT_THROW(db().exec("DELETE FROM audit_log"), bms::DbError);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log WHERE action = 'EDITED'"), 0);
}

TEST_F(AuditTest, DepositAndWithdrawalWriteAnEntryWithTheOperator) {
    const auto before = count("SELECT COUNT(*) FROM audit_log");
    bms::transaction::deposit(db(), "ACC001", "10.00", 2);
    bms::transaction::withdraw(db(), "ACC001", "5.00", 2);
    EXPECT_EQ(count("SELECT COUNT(*) FROM audit_log"), before + 2);

    auto rows = db().prepare(
        "SELECT user_id, action, entity FROM audit_log ORDER BY log_id DESC LIMIT 2");
    ASSERT_TRUE(rows->step());
    EXPECT_EQ(rows->column_int(0), 2);
    EXPECT_EQ(text(*rows, 1), "WITHDRAWAL");
    EXPECT_EQ(text(*rows, 2), "accounts");
    ASSERT_TRUE(rows->step());
    EXPECT_EQ(text(*rows, 1), "DEPOSIT");
}

TEST_F(AuditTest, FailedAuditWriteRollsBackTheBalanceChange) {
    db().exec(
        "CREATE TRIGGER fail_audit BEFORE INSERT ON audit_log "
        "BEGIN SELECT RAISE(ABORT, 'forced audit failure'); END");
    const auto balance_before = balance("ACC001");
    const auto transactions_before = count("SELECT COUNT(*) FROM transactions");

    EXPECT_THROW(bms::transaction::deposit(db(), "ACC001", "10.00", 2), bms::DbError);
    EXPECT_THROW(bms::transaction::withdraw(db(), "ACC001", "5.00", 2), bms::DbError);

    EXPECT_EQ(balance("ACC001"), balance_before);
    EXPECT_EQ(count("SELECT COUNT(*) FROM transactions"), transactions_before);
}

}  // namespace
