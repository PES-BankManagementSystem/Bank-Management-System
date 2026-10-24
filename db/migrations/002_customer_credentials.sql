-- Upgrade databases created from schema version 1.
BEGIN IMMEDIATE;

ALTER TABLE users
    ADD COLUMN must_change_password INTEGER NOT NULL DEFAULT 0
    CHECK (must_change_password IN (0, 1));

ALTER TABLE customers ADD COLUMN customer_code TEXT NOT NULL DEFAULT '';
ALTER TABLE customers ADD COLUMN identification_reference TEXT NOT NULL DEFAULT '';

UPDATE customers
SET customer_code = 'CUS-' || printf('%08d', customer_id),
    identification_reference = 'LEGACY-ID-' || printf('%08d', customer_id);

CREATE UNIQUE INDEX idx_customers_customer_code ON customers(customer_code);
CREATE UNIQUE INDEX idx_customers_identification_reference
    ON customers(identification_reference);

PRAGMA user_version = 2;
COMMIT;
