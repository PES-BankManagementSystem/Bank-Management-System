-- Bank Management System Database Schema
-- Money is stored as INTEGER minor units (paise)
-- interest_rate is stored as annual percentage in basis points (e.g., 4.00% = 400 basis points)

PRAGMA foreign_keys = ON;

-- Users table (all roles: administrator, bank_officer, customer)
CREATE TABLE IF NOT EXISTS users (
    user_id INTEGER PRIMARY KEY,
    username TEXT NOT NULL UNIQUE,
    role TEXT NOT NULL CHECK (role IN ('administrator', 'bank_officer', 'customer')),
    status TEXT NOT NULL CHECK (status IN ('active', 'inactive', 'locked')),
    failed_attempts INTEGER NOT NULL CHECK (failed_attempts >= 0),
    password_hash TEXT NOT NULL, -- Argon2id string from libsodium crypto_pwhash_str
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
) STRICT;

-- Customers table (optional link to users for login capability)
CREATE TABLE IF NOT EXISTS customers (
    customer_id INTEGER PRIMARY KEY,
    user_id INTEGER UNIQUE, -- Optional FK to users so a customer can log in
    full_name TEXT NOT NULL,
    email TEXT NOT NULL UNIQUE,
    phone TEXT NOT NULL UNIQUE,
    address TEXT NOT NULL,
    date_of_birth TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE SET NULL
) STRICT;

-- Account types table
CREATE TABLE IF NOT EXISTS account_types (
    type_id INTEGER PRIMARY KEY,
    type_name TEXT NOT NULL UNIQUE,
    min_balance INTEGER NOT NULL CHECK (min_balance >= 0), -- in paise
    interest_rate INTEGER NOT NULL, -- annual percentage in basis points (e.g., 400 = 4.00%)
    daily_withdrawal_limit INTEGER NOT NULL CHECK (daily_withdrawal_limit >= 0), -- in paise
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
) STRICT;

-- Accounts table
CREATE TABLE IF NOT EXISTS accounts (
    account_id INTEGER PRIMARY KEY,
    account_number TEXT NOT NULL UNIQUE,
    customer_id INTEGER NOT NULL,
    type_id INTEGER NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('active', 'frozen', 'closed')),
    balance INTEGER NOT NULL CHECK (balance >= 0), -- in paise
    opened_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (customer_id) REFERENCES customers(customer_id) ON DELETE RESTRICT,
    FOREIGN KEY (type_id) REFERENCES account_types(type_id) ON DELETE RESTRICT
) STRICT;

-- Transactions table
CREATE TABLE IF NOT EXISTS transactions (
    transaction_id INTEGER PRIMARY KEY,
    account_id INTEGER NOT NULL,
    operator_id INTEGER NOT NULL, -- FK to users (administrator who triggered interest runs)
    type TEXT NOT NULL CHECK (type IN ('deposit', 'withdrawal', 'transfer_debit', 'transfer_credit', 'interest')),
    amount INTEGER NOT NULL CHECK (amount > 0), -- in paise
    resulting_balance INTEGER NOT NULL CHECK (resulting_balance >= 0), -- in paise
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    transfer_ref TEXT, -- NULL for non-transfer transactions
    interest_run_id INTEGER, -- NULL for non-interest transactions
    FOREIGN KEY (account_id) REFERENCES accounts(account_id) ON DELETE RESTRICT,
    FOREIGN KEY (operator_id) REFERENCES users(user_id) ON DELETE RESTRICT
) STRICT;

-- Unique index for transfer_ref: at most one per type (gives exactly two linked entries: debit/credit)
CREATE UNIQUE INDEX IF NOT EXISTS idx_transactions_transfer_ref_type ON transactions(transfer_ref, type) WHERE transfer_ref IS NOT NULL;

-- Unique index to prevent interest being credited twice for same account and period
CREATE UNIQUE INDEX IF NOT EXISTS idx_transactions_account_interest_run ON transactions(account_id, interest_run_id) WHERE interest_run_id IS NOT NULL;

-- Index for statements and daily-withdrawal-limit queries
CREATE INDEX IF NOT EXISTS idx_transactions_account_created ON transactions(account_id, created_at);

-- Audit log table (append-only)
CREATE TABLE IF NOT EXISTS audit_log (
    log_id INTEGER PRIMARY KEY,
    user_id INTEGER NOT NULL,
    action TEXT NOT NULL,
    entity TEXT NOT NULL,
    entity_id INTEGER NOT NULL,
    details TEXT,
    timestamp TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (user_id) REFERENCES users(user_id) ON DELETE RESTRICT
) STRICT;

-- Interest runs table
CREATE TABLE IF NOT EXISTS interest_runs (
    run_id INTEGER PRIMARY KEY,
    period TEXT NOT NULL UNIQUE, -- Period identifier (e.g., '2026-01')
    run_by INTEGER NOT NULL, -- FK to users (administrator who triggered it)
    run_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    accounts_credited INTEGER NOT NULL CHECK (accounts_credited >= 0),
    total_interest INTEGER NOT NULL CHECK (total_interest >= 0), -- in paise
    FOREIGN KEY (run_by) REFERENCES users(user_id) ON DELETE RESTRICT
) STRICT;

-- Triggers to enforce append-only nature of audit_log and transactions
CREATE TRIGGER IF NOT EXISTS prevent_audit_log_update
BEFORE UPDATE ON audit_log
BEGIN
    SELECT RAISE(ABORT, 'audit_log is append-only - UPDATE not allowed');
END;

CREATE TRIGGER IF NOT EXISTS prevent_audit_log_delete
BEFORE DELETE ON audit_log
BEGIN
    SELECT RAISE(ABORT, 'audit_log is append-only - DELETE not allowed');
END;

CREATE TRIGGER IF NOT EXISTS prevent_transactions_update
BEFORE UPDATE ON transactions
BEGIN
    SELECT RAISE(ABORT, 'transactions are append-only - UPDATE not allowed');
END;

CREATE TRIGGER IF NOT EXISTS prevent_transactions_delete
BEFORE DELETE ON transactions
BEGIN
    SELECT RAISE(ABORT, 'transactions are append-only - DELETE not allowed');
END;

-- Triggers for users, customers and accounts
CREATE TRIGGER IF NOT EXISTS prevent_users_delete
BEFORE DELETE ON users
BEGIN
    SELECT RAISE(ABORT, 'users cannot be deleted - use status change instead');
END;

CREATE TRIGGER IF NOT EXISTS prevent_customers_delete
BEFORE DELETE ON customers
BEGIN
    SELECT RAISE(ABORT, 'customers cannot be deleted');
END;

CREATE TRIGGER IF NOT EXISTS prevent_accounts_delete
BEFORE DELETE ON accounts
BEGIN
    SELECT RAISE(ABORT, 'accounts cannot be deleted - use status change instead');
END;

-- Set user_version for migration tracking
PRAGMA user_version = 1;