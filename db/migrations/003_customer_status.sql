-- Add an independent lifecycle status for customer records.
BEGIN IMMEDIATE;

ALTER TABLE customers
    ADD COLUMN status TEXT NOT NULL DEFAULT 'active'
    CHECK (status IN ('active', 'inactive'));

UPDATE customers
SET status = 'inactive'
WHERE user_id IN (SELECT user_id FROM users WHERE status = 'inactive');

PRAGMA user_version = 3;
COMMIT;
