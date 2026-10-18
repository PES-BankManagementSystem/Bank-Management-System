-- Bank Management System schema (SQLite).
-- Run once to create all tables.  The server applies this automatically on
-- first start if the database file does not exist yet.

-- Users: Administrators, Bank Officers and Customers.
-- Passwords are stored as Argon2id hashes (FR-1, NFR-4, SEC-2).
CREATE TABLE IF NOT EXISTS users (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    role            TEXT NOT NULL CHECK (role IN ('administrator', 'bank_officer', 'customer')),
    password_hash   TEXT NOT NULL,
    failed_attempts INTEGER NOT NULL DEFAULT 0,
    locked          INTEGER NOT NULL DEFAULT 0,     -- 1 = locked after 3 failed logins (FR-2)
    active          INTEGER NOT NULL DEFAULT 1,     -- 0 = deactivated (FR-13)
    created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    updated_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

-- Sessions: random tokens with an expiry, validated on every protected request
-- (SEC-1).  Invalidated on logout.
CREATE TABLE IF NOT EXISTS sessions (
    token       TEXT PRIMARY KEY,
    user_id     TEXT NOT NULL REFERENCES users(id),
    role        TEXT NOT NULL,
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    expires_at  TEXT NOT NULL
);

-- Accounts: linked to a customer.
CREATE TABLE IF NOT EXISTS accounts (
    id              TEXT PRIMARY KEY,
    customer_id     TEXT NOT NULL REFERENCES users(id),
    account_type    TEXT NOT NULL CHECK (account_type IN ('savings', 'current')),
    balance         REAL NOT NULL DEFAULT 0.0,
    min_balance     REAL NOT NULL DEFAULT 1000.0,
    active          INTEGER NOT NULL DEFAULT 1,
    created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    updated_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

-- Transactions: deposits, withdrawals and transfers.
CREATE TABLE IF NOT EXISTS transactions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id      TEXT NOT NULL REFERENCES accounts(id),
    type            TEXT NOT NULL CHECK (type IN ('deposit', 'withdrawal', 'transfer_in', 'transfer_out')),
    amount          REAL NOT NULL,
    balance_after   REAL NOT NULL,
    description     TEXT,
    performed_by    TEXT NOT NULL REFERENCES users(id),
    created_at      TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

-- Audit trail (append-only).
CREATE TABLE IF NOT EXISTS audit_log (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     TEXT REFERENCES users(id),
    action      TEXT NOT NULL,
    target_type TEXT,
    target_id   TEXT,
    details     TEXT,           -- JSON
    created_at  TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);

-- Index for session lookup by token.
CREATE INDEX IF NOT EXISTS idx_sessions_token ON sessions(token);
CREATE INDEX IF NOT EXISTS idx_sessions_user  ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_accounts_customer ON accounts(customer_id);
CREATE INDEX IF NOT EXISTS idx_transactions_account ON transactions(account_id);
CREATE INDEX IF NOT EXISTS idx_audit_user ON audit_log(user_id);
