-- Seed data for development and testing.
-- Passwords are Argon2id hashes of the given plaintext (see comments).
-- In production, these would be changed on first login.

-- Default administrator: password = "admin123"
-- Default bank officer: password = "officer123"
-- Default customer: password = "customer123"
-- (The hashes below are placeholders; the server's seed command generates real ones.)

INSERT OR IGNORE INTO users (id, name, role, password_hash) VALUES
    ('admin1', 'System Administrator', 'administrator', '$argon2id$v=19$m=65536,t=3,p=1$placeholder$admin_hash'),
    ('officer1', 'Jane Smith', 'bank_officer', '$argon2id$v=19$m=65536,t=3,p=1$placeholder$officer_hash'),
    ('officer2', 'Bob Jones', 'bank_officer', '$argon2id$v=19$m=65536,t=3,p=1$placeholder$bob_hash'),
    ('customer1', 'Alice Brown', 'customer', '$argon2id$v=19$m=65536,t=3,p=1$placeholder$customer_hash'),
    ('customer2', 'Charlie Davis', 'customer', '$argon2id$v=19$m=65536,t=3,p=1$placeholder$charlie_hash');

INSERT OR IGNORE INTO accounts (id, customer_id, account_type, balance) VALUES
    ('ACC001', 'customer1', 'savings', 50000.00),
    ('ACC002', 'customer1', 'current', 25000.00),
    ('ACC003', 'customer2', 'savings', 75000.00);

INSERT OR IGNORE INTO transactions (account_id, type, amount, balance_after, description, performed_by) VALUES
    ('ACC001', 'deposit', 50000.00, 50000.00, 'Initial deposit', 'officer1'),
    ('ACC002', 'deposit', 25000.00, 25000.00, 'Initial deposit', 'officer1'),
    ('ACC003', 'deposit', 75000.00, 75000.00, 'Initial deposit', 'officer1');
