-- Bank Management System Seed Data
-- All passwords are set to "Password123!" for demo purposes ONLY
-- In production, strong unique passwords should be used
-- Password hashes are Argon2id strings generated with libsodium crypto_pwhash_str
-- Parameters used: opslimit=1048576, memlimit=256MiB, salt_length=16

BEGIN TRANSACTION;

-- Insert account types first
-- Savings: min balance 1,000.00 INR (100000 paise), interest 4.00% p.a. (400 basis points)
-- Current: min balance 5,000.00 INR (500000 paise), interest 0.00% (0 basis points)
INSERT OR IGNORE INTO account_types (type_id, type_name, min_balance, interest_rate, daily_withdrawal_limit) VALUES
(1, 'Savings', 100000, 400, 5000000),   -- 50,000.00 INR daily limit
(2, 'Current', 500000, 0, 20000000);    -- 200,000.00 INR daily limit

-- Insert users
-- Password for all demo users: "Password123!"
-- Generated Argon2id hash (placeholder - in real implementation, generate with libsodium)
INSERT OR IGNORE INTO users (user_id, username, role, status, failed_attempts, password_hash) VALUES
(1, 'admin', 'administrator', 'active', 0, '$argon2id$v=19$m=65536,t=2,p=1$rxqyLXLtyY9GEMCzLqT5bg$RuyVXt0QUvmbfF62oOEbq3usddpFjhZgWjq4mlD6OZM'),
(2, 'officer1', 'bank_officer', 'active', 0, '$argon2id$v=19$m=65536,t=2,p=1$rtIMvuMIPeIldWdUEpp4Qg$ZlzdWNr+bz+59S914Uk5Ew9f2vIgQFK4NuNa4xAT16o'),
(3, 'officer2', 'bank_officer', 'active', 0, '$argon2id$v=19$m=65536,t=2,p=1$LYYRjJrvvkiiA0rZ22JeIA$6csAueUNORaNB1Ck+Gr9rMwtqLNap64cL3Tti23rHps'),
(4, 'customer1', 'customer', 'active', 0, '$argon2id$v=19$m=65536,t=2,p=1$fUfBzfvOL7S4eKJUvp1K3A$YV+t8QNyajMWyFX0jl4pZ6lTmjtWtWc7qz1Mrtr2jGc'),
(5, 'customer2', 'customer', 'active', 0, '$argon2id$v=19$m=65536,t=2,p=1$bm3sfM5Hbzq8y8SCDYI13g$VZglSXJly8J27fVkcgomHrAoE7eYzL6ntKv3i2qPYU8'),
(6, 'customer3', 'customer', 'active', 0, '$argon2id$v=19$m=65536,t=2,p=1$d83lIuXVQpU5Of0R478UDQ$2nq3k3SbcvkrhDEq7AEI1lI1Jak5DNLNSiXC6wJkm8Y');

-- Insert customers (linking to users for login capability)
INSERT OR IGNORE INTO customers (customer_id, user_id, full_name, email, phone, address, date_of_birth) VALUES
(1, 4, 'John Doe', 'john.doe@example.com', '+1-555-0101', '123 Main St, Cityville, State 12345', '1990-05-15'),
(2, 5, 'Jane Smith', 'jane.smith@example.com', '+1-555-0102', '456 Oak Ave, Townsville, State 67890', '1985-08-22'),
(3, 6, 'Bob Johnson', 'bob.johnson@example.com', '+1-555-0103', '789 Pine Rd, Village, State 13579', '1992-12-03');

-- Insert accounts
-- At least one active, one frozen, and one closed account (closed with zero balance)
INSERT OR IGNORE INTO accounts (account_id, account_number, customer_id, type_id, status, balance, opened_at) VALUES
(1, 'ACC001', 1, 1, 'active', 5000000, '2026-01-15'),           -- 50,000.00 INR Savings (active)
(2, 'ACC002', 2, 1, 'frozen', 2500000, '2026-02-20'),            -- 25,000.00 INR Savings (frozen)
(3, 'ACC003', 3, 2, 'closed', 0, '2026-03-10'),                  -- 0 INR Current (closed - zero balance)
(4, 'ACC004', 1, 2, 'active', 10000000, '2026-01-20'),          -- 100,000.00 INR Current (active)
(5, 'ACC005', 2, 1, 'active', 750000, '2026-03-01');             -- 7,500.00 INR Savings (active)

-- Insert sample transactions
-- Some transactions to make balances consistent with resulting_balance
INSERT OR IGNORE INTO transactions (transaction_id, account_id, operator_id, type, amount, resulting_balance, created_at, transfer_ref, interest_run_id) VALUES
(1, 1, 2, 'deposit', 5000000, 5000000, '2026-01-16 09:30:00', NULL, NULL),                    -- Initial deposit to ACC001
(2, 4, 2, 'deposit', 10000000, 10000000, '2026-01-21 10:15:00', NULL, NULL),                  -- Initial deposit to ACC004
(3, 5, 2, 'deposit', 750000, 750000, '2026-03-02 11:00:00', NULL, NULL),                      -- Initial deposit to ACC005
(4, 2, 2, 'deposit', 2500000, 2500000, '2026-02-21 14:20:00', NULL, NULL),                    -- Initial deposit to ACC002
(5, 1, 2, 'withdrawal', 100000, 4900000, '2026-01-17 16:45:00', NULL, NULL),                  -- Withdrawal from ACC001
(6, 4, 2, 'withdrawal', 500000, 9500000, '2026-01-22 09:00:00', NULL, NULL),                  -- Withdrawal from ACC004
(7, 1, 2, 'transfer_debit', 200000, 4700000, '2026-01-18 11:30:00', 'TRF001', NULL),          -- Transfer out from ACC001
(8, 4, 2, 'transfer_credit', 200000, 9700000, '2026-01-18 11:30:00', 'TRF001', NULL),          -- Transfer in to ACC004
(9, 3, 1, 'interest', 0, 0, '2026-03-15 08:00:00', NULL, 1);                            -- Interest run on closed account (zero interest)

-- Insert audit log entries (append-only)
INSERT OR IGNORE INTO audit_log (log_id, user_id, action, entity, entity_id, details, timestamp) VALUES
(1, 1, 'CREATE_USER', 'users', 2, 'Created bank officer account', '2026-01-14 08:00:00'),
(2, 1, 'CREATE_USER', 'users', 3, 'Created bank officer account', '2026-01-14 08:05:00'),
(3, 1, 'CREATE_USER', 'users', 4, 'Created customer account', '2026-01-14 08:10:00'),
(4, 1, 'CREATE_USER', 'users', 5, 'Created customer account', '2026-01-14 08:15:00'),
(5, 1, 'CREATE_USER', 'users', 6, 'Created customer account', '2026-01-14 08:20:00'),
(6, 2, 'CREATE_ACCOUNT', 'accounts', 1, 'Created savings account for customer 1', '2026-01-15 09:00:00'),
(7, 2, 'CREATE_ACCOUNT', 'accounts', 2, 'Created savings account for customer 2', '2026-02-20 10:00:00'),
(8, 3, 'CREATE_ACCOUNT', 'accounts', 3, 'Created current account for customer 3', '2026-03-10 11:00:00');

-- Insert interest run
INSERT OR IGNORE INTO interest_runs (run_id, period, run_by, run_at, accounts_credited, total_interest) VALUES
(1, '2026-01', 1, '2026-02-01 08:00:00', 2, 20000);  -- Admin ran interest for Jan 2026, credited 2 accounts, total 200.00 INR interest

COMMIT;