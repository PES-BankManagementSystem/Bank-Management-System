-- Bound account-type configuration and make names case-insensitively unique.
-- Existing account and transaction rows keep their type IDs and history.
PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

CREATE TABLE account_types_v4 (
    type_id INTEGER PRIMARY KEY,
    type_name TEXT NOT NULL COLLATE NOCASE UNIQUE,
    min_balance INTEGER NOT NULL CHECK (min_balance >= 0 AND min_balance <= 1000000000),
    interest_rate INTEGER NOT NULL CHECK (interest_rate >= 0 AND interest_rate <= 2000),
    daily_withdrawal_limit INTEGER NOT NULL
        CHECK (daily_withdrawal_limit >= 0 AND daily_withdrawal_limit <= 1000000000),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
) STRICT;

INSERT INTO account_types_v4
    (type_id, type_name, min_balance, interest_rate, daily_withdrawal_limit, created_at)
SELECT type_id, type_name, min_balance, interest_rate, daily_withdrawal_limit, created_at
FROM account_types;

DROP TABLE account_types;
ALTER TABLE account_types_v4 RENAME TO account_types;

CREATE TRIGGER prevent_account_types_delete
BEFORE DELETE ON account_types
BEGIN
    SELECT RAISE(ABORT, 'account types cannot be deleted');
END;

PRAGMA user_version = 4;
COMMIT;
PRAGMA foreign_keys = ON;
