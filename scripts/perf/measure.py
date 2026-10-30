#!/usr/bin/env python3
"""Measure authenticated local API response times against a seeded DB."""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
import time
import urllib.error
import urllib.request
from decimal import Decimal
from pathlib import Path
from urllib.parse import urljoin

from perf_common import safe_database_path, summarize


def request(base_url: str, path: str, token: str | None = None,
            payload: dict | None = None) -> tuple[int, bytes, float]:
    headers = {"Accept": "application/json"}
    data = None
    if payload is not None:
        data = json.dumps(payload).encode("utf-8")
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"
    req = urllib.request.Request(urljoin(base_url.rstrip("/") + "/", path.lstrip("/")),
                                 data=data, headers=headers, method="POST" if data else "GET")
    started = time.perf_counter()
    try:
        with urllib.request.urlopen(req, timeout=15) as response:
            status, body = response.status, response.read()
    except urllib.error.HTTPError as error:
        status, body = error.code, error.read()
    elapsed = (time.perf_counter() - started) * 1000
    return status, body, elapsed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-url", default="http://127.0.0.1:8080")
    parser.add_argument("--db", required=True, help="the same guarded temp DB used by seed.py")
    parser.add_argument("--samples", type=int, default=30)
    parser.add_argument("--warmup", type=int, default=3)
    parser.add_argument("--output", default="docs/perf/results.json")
    args = parser.parse_args()
    try:
        database_path = safe_database_path(args.db)
        if not database_path.is_file():
            raise FileNotFoundError(database_path)
        if args.samples < 1 or args.warmup < 0:
            raise ValueError("samples must be positive and warmup non-negative")
        status, body, _ = request(args.base_url, "/auth/login",
                                  payload={"username": "officer1", "password": "Password123!"})
        if status != 200:
            raise RuntimeError(f"login failed with HTTP {status}: {body.decode(errors='replace')}")
        token = json.loads(body)["accessToken"]

        data: dict = {"base_url": args.base_url, "dataset_accounts": None,
                      "samples_per_operation": args.samples, "warmup": args.warmup,
                      "measurements": {}, "blocked": {}}
        with sqlite3.connect(f"file:{database_path}?mode=ro", uri=True) as db:
            data["dataset_accounts"] = db.execute("SELECT COUNT(*) FROM accounts").fetchone()[0]

        def timed(label: str, path: str, payload_factory, expected_status: int = 201) -> dict:
            values: list[float] = []
            checked_balance: tuple[str, int] | None = None
            for sample in range(args.warmup + args.samples):
                payload = payload_factory(sample)
                status_code, response_body, elapsed = request(args.base_url, path, token, payload)
                if status_code != expected_status:
                    raise RuntimeError(f"{label} returned HTTP {status_code}: "
                                       f"{response_body.decode(errors='replace')}")
                response = json.loads(response_body)
                if label == "deposit":
                    account, balance = payload["account_number"], response["balance"]
                elif label == "withdrawal":
                    account, balance = payload["account_number"], response["balance"]
                elif label == "transfer":
                    account, balance = payload["from_account_number"], response["from_balance"]
                else:
                    account, balance = "", ""
                if sample == args.warmup:
                    checked_balance = (account, int(Decimal(balance) * 100))
                if sample >= args.warmup:
                    values.append(elapsed)
            if checked_balance is not None:
                with sqlite3.connect(f"file:{database_path}?mode=ro", uri=True) as db:
                    stored = db.execute("SELECT balance FROM accounts WHERE account_number = ?",
                                         (checked_balance[0],)).fetchone()
                if stored is None or stored[0] != checked_balance[1]:
                    raise RuntimeError(f"{label} response balance was not persisted in SQLite")
            return {"raw_ms": values, **summarize(values, 2000.0)}

        # Disjoint high-balance accounts avoid one operation affecting another's samples.
        data["measurements"]["deposit"] = timed(
            "deposit", "/transactions", lambda i: {"type": "deposit", "account_number": f"PF{i + 1:06d}", "amount": "1.23"})
        data["measurements"]["withdrawal"] = timed(
            "withdrawal", "/transactions", lambda i: {"type": "withdraw", "account_number": f"PF{i + 100:06d}", "amount": "1.00"})
        data["measurements"]["transfer"] = timed(
            "transfer", "/transfers", lambda i: {"from_account_number": f"PF{i + 200:06d}",
                                                  "to_account_number": f"PF{i + 500:06d}", "amount": "1.23"})

        # These paths represent the requested pages/data views. On current main they
        # return 404; keep diagnostic timings, but classify them as Blocked, not Pass.
        page_paths = {
            "dashboard": "/dashboard",
            "account_list": "/accounts",
            "transaction_history": "/accounts/PF000001/transactions?from=2023-01-01&to=2026-12-31",
        }
        for name, path in page_paths.items():
            statuses, timings = [], []
            for _ in range(args.warmup + args.samples):
                code, _, elapsed = request(args.base_url, path, token)
                statuses.append(code)
                if len(timings) < args.samples and len(statuses) > args.warmup:
                    timings.append(elapsed)
            data["blocked"][name] = {"path": path, "http_statuses": sorted(set(statuses)),
                                     "raw_ms": timings, "verdict": "Blocked"}
        # Measure existing rendered shell and statement/history API separately.
        for name, path in {
            "frontend_shell": "/",
            "statement_api": "/accounts/PF000001/statement?from=2023-01-01&to=2026-12-31",
        }.items():
            statuses, timings = [], []
            for index in range(args.warmup + args.samples):
                code, _, elapsed = request(args.base_url, path, token)
                statuses.append(code)
                if index >= args.warmup:
                    timings.append(elapsed)
            result = {"path": path, "http_statuses": sorted(set(statuses)),
                      "raw_ms": timings, **summarize(timings, 3000.0)}
            if any(code != 200 for code in statuses):
                result["verdict"] = "Blocked"
            data["measurements"][name] = result

        output = Path(args.output)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
        print(f"Accounts in database: {data['dataset_accounts']}; samples: {args.samples} (after {args.warmup} warmups)")
        for name, result in data["measurements"].items():
            print(f"{name}: n={result['samples']} mean={result['mean_ms']:.3f} ms "
                  f"median={result['median_ms']:.3f} ms p95={result['p95_ms']:.3f} ms "
                  f"p99={result['p99_ms']:.3f} ms max={result['max_ms']:.3f} ms "
                  f"limit={result['threshold_ms']:.0f} ms {result['verdict']}")
        for name, result in data["blocked"].items():
            print(f"{name}: Blocked; HTTP {result['http_statuses']} for {result['path']}")
        print(f"Raw measurements: {output}")
    except (OSError, ValueError, RuntimeError, KeyError, sqlite3.Error) as error:
        print(f"measurement failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
