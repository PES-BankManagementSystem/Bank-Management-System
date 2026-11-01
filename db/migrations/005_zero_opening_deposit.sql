-- Permit a zero opening deposit when the account type has a zero minimum.
PRAGMA foreign_keys = OFF;
BEGIN IMMEDIATE;

CREATE TABLE transactions_v5 (
    transaction_id INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL,
    operator_id INTEGER NOT NULL,
    type TEXT NOT NULL CHECK (type IN ('deposit', 'withdrawal', 'transfer_debit', 'transfer_credit', 'interest')),
    amount INTEGER NOT NULL CHECK (
        amount > 0 OR (amount = 0 AND type = 'deposit')
    ),
    resulting_balance INTEGER NOT NULL CHECK (resulting_balance >= 0),
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    transfer_ref TEXT,
    interest_run_id INTEGER,
    FOREIGN KEY (account_id) REFERENCES accounts(account_id) ON DELETE RESTRICT,
    FOREIGN KEY (operator_id) REFERENCES users(user_id) ON DELETE RESTRICT
) STRICT;

INSERT INTO transactions_v5
    (transaction_id, account_id, operator_id, type, amount, resulting_balance,
     created_at, transfer_ref, interest_run_id)
SELECT transaction_id, account_id, operator_id, type, amount, resulting_balance,
       created_at, transfer_ref, interest_run_id
FROM transactions;

DROP TABLE transactions;
ALTER TABLE transactions_v5 RENAME TO transactions;

CREATE UNIQUE INDEX idx_transactions_transfer_ref_type
    ON transactions(transfer_ref, type) WHERE transfer_ref IS NOT NULL;
CREATE UNIQUE INDEX idx_transactions_account_interest_run
    ON transactions(account_id, interest_run_id) WHERE interest_run_id IS NOT NULL;
CREATE INDEX idx_transactions_account_created
    ON transactions(account_id, created_at);

CREATE TRIGGER prevent_transactions_update
BEFORE UPDATE ON transactions
BEGIN
    SELECT RAISE(ABORT, 'transactions are append-only - UPDATE not allowed');
END;

CREATE TRIGGER prevent_transactions_delete
BEFORE DELETE ON transactions
BEGIN
    SELECT RAISE(ABORT, 'transactions are append-only - DELETE not allowed');
END;

PRAGMA user_version = 5;
COMMIT;
PRAGMA foreign_keys = ON;
