#!/usr/bin/env python3
"""Create a deterministic, disposable SQLite performance dataset."""

from __future__ import annotations

import argparse
import sqlite3
import sys
from pathlib import Path

from perf_common import generated_history, safe_database_path


def read_sql(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def seed_database(database_path: Path, account_count: int = 2500) -> int:
    if not 2000 <= account_count <= 3000:
        raise ValueError("account count must be between 2,000 and 3,000")
    database_path = safe_database_path(database_path)
    if database_path.exists():
        raise FileExistsError(f"{database_path} already exists; pass --reset to recreate it")
    database_path.parent.mkdir(parents=True, exist_ok=True)
    root = Path(__file__).resolve().parents[2]
    connection = sqlite3.connect(database_path)
    try:
        connection.execute("PRAGMA foreign_keys = ON")
        connection.executescript(read_sql(root / "db" / "schema.sql"))
        connection.executescript(read_sql(root / "db" / "seed.sql"))
        next_customer = connection.execute("SELECT MAX(customer_id) + 1 FROM customers").fetchone()[0]
        next_account = connection.execute("SELECT MAX(account_id) + 1 FROM accounts").fetchone()[0]

        for index in range(account_count):
            customer_id = next_customer + index
            account_id = next_account + index
            account_number = f"PF{index + 1:06d}"
            connection.execute(
                "INSERT INTO customers (customer_id, customer_code, full_name, email, phone, "
                "identification_reference, address, date_of_birth) VALUES (?, ?, ?, ?, ?, ?, ?, ?)",
                (customer_id, f"PERF-{index + 1:06d}", f"Performance Customer {index + 1}",
                 f"perf{index + 1}@example.test", f"+91-900{index + 1:07d}",
                 f"PERF-ID-{index + 1:06d}", "Local performance test address", "1990-01-01"),
            )
            connection.execute(
                "INSERT INTO accounts (account_id, account_number, customer_id, type_id, "
                "status, balance, opened_at) VALUES (?, ?, ?, 1, 'active', 0, '2023-01-01')",
                (account_id, account_number, customer_id),
            )
            history = generated_history(index)
            balance = 0
            for offset, (kind, amount) in enumerate(history):
                if kind == "withdrawal":
                    amount = min(amount, max(1, balance - 100_000))
                    balance -= amount
                else:
                    balance += amount
                month = (offset % 36) + 1
                year = 2023 + (month - 1) // 12
                month = ((month - 1) % 12) + 1
                day = ((offset * 7 + index) % 28) + 1
                connection.execute(
                    "INSERT INTO transactions (account_id, operator_id, type, amount, "
                    "resulting_balance, created_at) VALUES (?, 2, ?, ?, ?, ?)",
                    (account_id, kind, amount, balance,
                     f"{year:04d}-{month:02d}-{day:02d} {9 + offset % 10:02d}:15:00"),
                )
            connection.execute("UPDATE accounts SET balance = ? WHERE account_id = ?",
                               (balance, account_id))
        connection.commit()
        return account_count + 5
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--db", required=True, help="new path under the OS temp dir, e.g. /tmp/bms-perf-run.sqlite")
    parser.add_argument("--accounts", type=int, default=2500, help="generated accounts (2,000-3,000)")
    parser.add_argument("--reset", action="store_true", help="remove this guarded disposable DB before recreating it")
    args = parser.parse_args()
    try:
        database_path = safe_database_path(args.db)
        if args.reset and database_path.exists():
            for suffix in ("", "-journal", "-wal", "-shm"):
                Path(str(database_path) + suffix).unlink(missing_ok=True)
        total = seed_database(database_path, args.accounts)
    except (OSError, ValueError, sqlite3.Error) as error:
        print(f"seed failed: {error}", file=sys.stderr)
        return 1
    print(f"Seeded {total} total accounts ({args.accounts} generated): {database_path}")
    print("Five accounts have 300 historical transactions each; other generated accounts have 12.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
