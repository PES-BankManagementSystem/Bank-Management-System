# Bank Management System — Entity Relationship (ER) Diagram

- **Design Document:** ERD-2026-v1.1
- **Component:** Core Database & Relational Schema
- **Story:** [S-34] Fix SRS gaps (#65)
- **Target RDBMS:** SQLite 3 (Foreign Key enforcement enabled)

---

## 1. Visual Entity Relationship Diagram

```mermaid
erDiagram
    USERS ||--o{ SESSIONS : "has active"
    USERS ||--o| CUSTOMERS : "profiles"
    CUSTOMERS ||--o{ ACCOUNTS : "owns"
    ACCOUNTS ||--o{ TRANSACTIONS : "records"
    USERS ||--o{ TRANSACTIONS : "processes (officer)"
    USERS ||--o{ AUDIT_LOG : "triggers"

    USERS {
        TEXT id PK "Unique User identifier (e.g. admin, officer1, cust1)"
        TEXT name "Full display name"
        TEXT role "administrator | bank_officer | customer"
        TEXT password_hash "Salted Argon2id / cryptographic hash"
        INTEGER active "1 = active, 0 = deactivated"
        INTEGER locked "1 = locked after 3 failed attempts, 0 = normal"
        INTEGER failed_attempts "Consecutive failed login attempts"
        TEXT created_at "ISO-8601 UTC timestamp"
        TEXT updated_at "ISO-8601 UTC timestamp"
    }

    SESSIONS {
        TEXT token PK "64-character crypto-random token"
        TEXT user_id FK "References USERS(id)"
        TEXT role "User role at session creation"
        TEXT expires_at "Expiry timestamp (30 min lifetime)"
        TEXT created_at "ISO-8601 UTC timestamp"
    }

    CUSTOMERS {
        TEXT id PK "Unique Customer ID"
        TEXT user_id FK "References USERS(id)"
        TEXT phone "Contact telephone number"
        TEXT address "Postal residential address"
        TEXT created_at "ISO-8601 UTC timestamp"
    }

    ACCOUNTS {
        TEXT id PK "Account number (e.g. ACC-100234)"
        TEXT customer_id FK "References CUSTOMERS(id)"
        TEXT account_type "savings | current | fixed_deposit"
        REAL balance "Current available ledger balance"
        INTEGER active "1 = active, 0 = frozen / closed"
        TEXT created_at "ISO-8601 UTC timestamp"
    }

    TRANSACTIONS {
        INTEGER id PK "Auto-increment transaction ID"
        TEXT account_id FK "References ACCOUNTS(id)"
        TEXT type "deposit | withdrawal | transfer"
        REAL amount "Transaction amount"
        REAL balance_after "Account balance after transaction"
        TEXT performed_by FK "References USERS(id)"
        TEXT description "Transaction narrative or reference"
        TEXT created_at "ISO-8601 UTC timestamp"
    }

    AUDIT_LOG {
        INTEGER id PK "Auto-increment audit entry"
        TEXT event_type "LOGIN | LOGOUT | LOCKOUT | UNLOCK | TRANSFER | DEPOSIT | WITHDRAWAL"
        TEXT actor_id FK "References USERS(id)"
        TEXT details "JSON event payload"
        TEXT ip_address "Client IP address"
        TEXT created_at "ISO-8601 UTC timestamp"
    }
```

---

## 2. Table Specifications & Relationships

### `users`
- Central authentication and identity table.
- Stores credentials hashed with Argon2id; tracks consecutive failed login attempts and lockout state (FR-1, FR-2, SEC-2, S-07, S-09).
- Deactivation flags prevent new session creation while preserving foreign-key references on past transactions (S-24).

### `sessions`
- Server-side token store with strict 30-minute expiry (FR-2, SEC-1, S-08).
- Foreign key cascading ensures sessions are cleared immediately upon user deletion or deactivation.

### `customers` & `accounts`
- One-to-many relationship between customers and bank accounts.
- Enforces relational referential integrity (`PRAGMA foreign_keys = ON`).

### `transactions` & `audit_log`
- Append-only transactional ledger with serialized writes via `BEGIN IMMEDIATE` (NFR-3).
- Every balance-modifying event records both the affected account and the initiating user ID.
