# Test Summary Report — S-29 (#60)

## Purpose and scope

This report records execution of the primary Bank Management System Test Plan v1.0, Section 14 (TC-01–TC-17), and verification relevant to its Sections 6–8 and 14. The companion `Test_Cases_Bank_Management_System.docx` and `Test_Plan_Test_Cases_BankManagementSystem.pdf` contain an overlapping 11-case set with conflicting numbering and traceability. The primary `Test_Plan_BankManagementSystem.pdf`, which explicitly enumerates 17 cases, is used as the execution baseline. The discrepancy is recorded in [DEVIATIONS.md](DEVIATIONS.md), row 15.

## Tested environment and revision

- Source: refreshed `origin/main`, commit `14291f31d9c32f8f4b719c1f56194578afe2e995`.
- Local host: macOS Darwin 24.5.0, arm64; Apple Clang 17.0.0; CMake 4.4.4; Node.js 25.1.0.
- Database: fresh SQLite files created from `db/schema.sql` and `db/seed.sql`; performance checks added 2,000 accounts (2,005 total).
- Automated build: `cmake --build build-arm64 --parallel 2` — succeeded.
- Automated suite: `ctest --test-dir build-arm64 --output-on-failure` — **22/22 CTest targets passed**, total 75.55 seconds. The first sandboxed attempt could not bind local loopback sockets; the exact command was rerun with local socket access and passed. The old `build/` directory was not removed.
- Security hygiene: `./scripts/check-secrets.sh` — `No committed secrets found.` `git diff --check` passed. The repository does not configure a separate clang-format, clang-tidy, cppcheck, or shellcheck job.
- Live API: temporary seeded databases and servers bound to loopback/ephemeral ports. No test database or generated performance data was kept in the repository.

The full test output includes successful test targets for login/session/lockout, customer registration, account types, deposits, withdrawals, transfers, interest, statements/CSV, staff, audit, RBAC, TLS and log-secret handling. The per-case observations and commands are in [Test_Execution_Log.md](Test_Execution_Log.md).

## Test Plan results

| Total TCs | Passed | Failed | Blocked | Not executed | Pass percentage |
|---:|---:|---:|---:|---:|---:|
| 17 | 13 | 0 | 4 | 0 | 76.5% of all planned cases (100% of cases executable end-to-end) |

The four blocked cases are TC-01 (browser dashboard walkthrough), TC-05 (account opening), TC-06 (account state changes plus complete viewing flow), and TC-11 (balance/history APIs). The endpoints for opening, status, balance, transaction history and `/dashboard` returned HTTP 404 during local probes; the homepage labels its role landing as a placeholder. Those capabilities are planned in backlog stories S-15, S-16, S-20, S-21 and S-25. Related partial checks are recorded, but none of these cases is counted as Pass.

| Feature / cases | Results observed |
|---|---|
| Authentication and lockout — TC-01/02 | Lockout, correct-password refusal while locked, administrator unlock and session logout passed. The UI/dashboard portion of TC-01 is blocked. |
| Customer records — TC-03/04 | Valid registration, unique identifier generation, missing mandatory field errors and successful valid registration passed. |
| Account management — TC-05/06 | Blocked because `POST /accounts` and `PATCH /accounts/{acc}/status` are absent. Tests did confirm seeded frozen/closed accounts reject the applicable money-changing operations. |
| Deposits and withdrawals — TC-07/08 | Live deposit returned HTTP 201 and INR 7,000 from an INR 5,000 test balance. Live INR 800 withdrawal from INR 1,500 returned HTTP 422 `BELOW_MINIMUM_BALANCE`; balance remained INR 1,500. |
| Transfers — TC-09/10 | Live INR 1,000 transfer returned balances INR 4,000/INR 3,000 and a shared reference; same-account transfer returned HTTP 400 with balances unchanged. Automated fault-injection rollback checks passed. |
| Balance/history — TC-11 | Blocked because the balance and transaction-history routes returned HTTP 404. Statement functionality is present and tested separately for FR-12. |
| Interest — TC-12 | Interest run and duplicate-period refusal passed; the duplicate API call returned HTTP 409. |
| Staff — TC-13 | Officer deactivation prevented login and preserved historical operator attribution. |
| NFR — TC-14–17 | TC-14 performance, TC-15 credential/RBAC/TLS checks, TC-16 audit append-only checks and TC-17 invalid-input safety passed. |

## Non-functional verification

- **NFR-1 / TC-14, performance:** with 2,005 accounts, live authenticated requests took deposit **1.54 ms**, withdrawal **1.01 ms**, and transfer **1.24 ms** (request start to response; each returned HTTP 201). Each measured request was below 2 seconds. These are single local samples under light load and do not establish a latency distribution or peak-load result.
- **NFR-2, page load:** a single localhost `GET /` returned HTTP 200, 21,977 bytes, in **39.10 ms** with the 2,005-account database. This is a placeholder home page, not the dashboard/account/history pages named in NFR-2; the 3-second requirement for those pages remains unverified.
- **NFR-3, atomicity:** transfer tests force failures after partial transaction work and verify rollback; deposit, withdrawal and interest audit/database failure tests also passed.
- **NFR-4 / TC-15, security:** automated checks confirmed salted Argon2id password hashes, no passwords/tokens in server logs, backend RBAC enforcement, and TLS server behavior. A live login over the HTTPS listener returned HTTP 200 and the Bank Officer role. No packet capture was taken; the login probe used TLS.
- **NFR-5 / TC-16, audit:** automated checks confirmed audit fields, transactional rollback and database/API refusal of audit edits/deletes.
- **NFR-6, usability:** no representative-user walkthrough was conducted. This requires at least three users per role and is not inferred from automated tests.
- **NFR-7 / TC-17, reliability:** invalid amounts and unknown accounts returned structured errors; the automated tests verified unchanged balances/records on rejection.
- **NFR-8, maintainability:** source/CMake inspection confirmed separate functional modules and static-library targets. This is a structural observation, not a maintainability study.
- **NFR-9, portability:** the local run was macOS arm64. The repository CI workflow is configured to build and run CTest on Ubuntu and Windows, but its PR results are not available in this report yet. Chrome/Firefox compatibility was not tested; no browser walkthrough was conducted.

## Requirements coverage

| Requirement | Test Plan mapping / additional evidence | Coverage result |
|---|---|---|
| FR-1 | TC-01/02; login and RBAC automated tests | Login/API role checks passed; TC-01 browser dashboard portion blocked. |
| FR-2 | TC-02 | Pass. |
| FR-3 | TC-03/04 | Pass. |
| FR-4 | TC-05 | Blocked; account-opening endpoint absent. |
| FR-5 | TC-06 | Blocked; state-transition endpoint absent; seeded frozen-account rejection subchecks pass. |
| FR-6 | TC-07 | Pass. |
| FR-7 | TC-08 | Pass. |
| FR-8 | TC-09/10 | Pass. |
| FR-9 / FR-10 | TC-11 | Blocked; balance/history endpoints absent. |
| FR-11 | TC-12 | Pass. |
| FR-12 | No dedicated TC in primary Test Plan traceability table; `statement_test` and `statement_csv_test` | Supplementary automated statement/CSV tests passed; formal Test Plan coverage is missing. |
| FR-13 | TC-13 | Pass for deactivation and historical attribution; broader staff create/update checks also pass. |
| FR-14 | No TC in primary Test Plan traceability table; live `GET /dashboard` | No dashboard endpoint; HTTP 404. Not covered by an executable TC. |
| NFR-1 | TC-14 | Pass for measured local samples; no peak-load distribution. |
| NFR-2 | No TC in primary Test Plan traceability table; one `GET /` sample | Representative dashboard/account/history performance is unverified. |
| NFR-3 | TC-09 and rollback tests | Pass for tested transaction failure paths. |
| NFR-4 | TC-15 and auth/RBAC/TLS tests | Pass for stored hashes, logs, server authorization and TLS login probe. |
| NFR-5 | TC-16 | Pass. |
| NFR-6 | No TC in primary Test Plan traceability table | Not executed; representative users were unavailable. |
| NFR-7 | TC-17 | Pass for tested invalid input cases. |
| NFR-8 | No TC in primary Test Plan traceability table | Partial structural inspection only. |
| NFR-9 | No TC in primary Test Plan traceability table; local build and configured CI matrix | Linux/Windows CI and Chrome/Firefox behavior are not yet evidenced here. |

## Defects

No TC was marked Fail, so no defect issue was created. The four Blocked cases have observed reasons and are already represented by the backlog stories listed above. No severity labels were created because there were no failing cases to file as defects.

## Known limitations and risks

- The account-opening, account-status, balance, transaction-history and dashboard endpoints are absent on the tested revision; this prevents completion of four planned system cases and full FR coverage.
- The separate test-case documents conflict with the primary Test Plan's IDs/mappings. Backlog S-35 calls for consolidation; this report follows the primary PDF and records the discrepancy without silently remapping cases.
- Usability and browser compatibility need human walkthroughs. NFR-2 needs representative pages and a larger/repeated timing sample; TC-14 used only a single sample per operation.
- Local macOS tests do not demonstrate Linux/Windows portability. CI should supply that evidence after this report's PR is opened.

## Conclusion

The automated suite passes and all 13 executable Test Plan cases passed. Four cases remain blocked by unavailable UI/API behavior. Several SRS requirements (notably FR-4, FR-5, FR-9/FR-10 and FR-14) therefore lack complete system-level evidence, and usability/browser verification remains outstanding. **The available evidence is not sufficient to conclude that the product meets the full SRS.** The report records the gaps rather than treating missing features or unrun walkthroughs as passes.

## Performance verification (NFR-1, NFR-2, TC-14)

### Requirements and method

- **NFR-1 / TC-14:** measure authenticated deposit, withdrawal and transfer
  requests through the HTTP response, with 2,000–3,000 accounts; each must
  complete in under 2 seconds including persistence.
- **NFR-2:** dashboard, account-list and transaction-history views must each
  load in under 3 seconds. Measurements are full server-side HTTP response
  times; this run does not include browser rendering.
- Each available operation uses 30 samples after 3 warmups by default. The
  report includes min, mean, median, nearest-rank p95/p99, max, threshold and
  verdict. One transaction response balance is checked against a read-only
  SQLite query after the response returns.

The seed uses the real schema and seed SQL, adds 2,500 generated accounts and
customers (2,505 total), and creates deterministic ledger histories. Five
generated accounts have 300 transactions each; the other generated accounts
have 12. Generated customers do not need login credentials because the seeded
Bank Officer authenticates and performs the measurements.

### Results

Environment: macOS Darwin 24.5.0 arm64 (Apple T8103), memory amount unavailable
to the local process; Apple Clang 17.0.0, CMake 4.4.4, Python 3.13.1, bundled
SQLite 3.53.0 in the app (Python seed/readback SQLite 3.45.3), app version
0.1.0, source commit `f626c3a9d3fba92564ebb629b9a7f19771a825d2`. The database
contained 2,505 accounts (2,500 generated), each with a customer and history;
five generated accounts had 300 transactions and the other generated accounts
had 12. Measurements ran on loopback over HTTP with one serial client. The
initial configure needed the pinned FetchContent downloads; the sandbox blocked
network access, so configure was rerun with network permission. The first
CTest attempt was also blocked from binding loopback; the full suite passed
when rerun with local socket access.

| Operation | Samples | Mean | Median | p95 | p99 | Max | Threshold | Verdict |
|---|---:|---:|---:|---:|---:|---:|---:|---|
| Deposit | 30 | 1.009 ms | 0.980 ms | 1.209 ms | 1.237 ms | 1.237 ms | 2,000 ms | Pass |
| Withdrawal | 30 | 1.026 ms | 1.001 ms | 1.197 ms | 1.390 ms | 1.390 ms | 2,000 ms | Pass |
| Transfer | 30 | 1.257 ms | 1.211 ms | 1.563 ms | 1.728 ms | 1.728 ms | 2,000 ms | Pass |
| Dashboard (`GET /dashboard`) | 30 diagnostics, HTTP 404 | — | — | — | — | — | 3,000 ms | Blocked (route absent) |
| Account list (`GET /accounts`) | 30 diagnostics, HTTP 404 | — | — | — | — | — | 3,000 ms | Blocked (route absent) |
| Transaction history (`GET /accounts/{acc}/transactions`) | 30 diagnostics, HTTP 404 | — | — | — | — | — | 3,000 ms | Blocked (route absent) |

All 90 timed transaction requests returned HTTP 201. The script compared a
returned transaction balance with its persisted SQLite account balance after
the response. Additional available-resource measurements: the static shell
(`GET /`, 30 samples) mean 0.595 ms, median 0.431 ms, p95 1.224 ms, max
4.339 ms; the statement API (30 samples) mean 5.282 ms, median 5.265 ms, p95
5.397 ms, max 5.519 ms. The statement API and shell are not substitutes for
the required dashboard/account-list/history views. The full CTest suite passed
23/23 in 75.34 seconds, including the new performance-helper tests.

Raw successful and blocked-probe measurements are recorded in
[`docs/perf/results.json`](perf/results.json). No operation exceeded a
threshold, so no defect issue was opened. The three blocked views are existing
feature gaps, not measured slow operations.

### Environment, limitations and reproduction

Measurements are local, serial HTTP API response times; they are not
browser-render timings or concurrent-load benchmarks. The app has a statement
endpoint and static frontend shell, but the three NFR-2 views are not
implemented in this revision.

```sh
python3 scripts/perf/seed.py --db "$(python3 -c 'import tempfile; print(tempfile.gettempdir() + "/bms-perf-s30.sqlite")')"
cmake -S . -B build
cmake --build build --parallel
./build/bin/bms_server --db "$(python3 -c 'import tempfile; print(tempfile.gettempdir() + "/bms-perf-s30.sqlite")')" --port 8080
# In another terminal:
python3 scripts/perf/measure.py --base-url http://127.0.0.1:8080 \
  --db "$(python3 -c 'import tempfile; print(tempfile.gettempdir() + "/bms-perf-s30.sqlite")')" --samples 30 --warmup 3
ctest --test-dir build --output-on-failure
```

To repeat with the same path, add `--reset` to `seed.py`. The guard allows only
paths in the OS temporary directory named `bms-perf-*.sqlite`; it refuses
ordinary project and production-looking paths. On Windows, replace `python3`
with `py -3`, use `build/bin/Debug/bms_server.exe`, and set the DB path with
PowerShell: `$db = Join-Path ([IO.Path]::GetTempPath()) 'bms-perf-s30.sqlite'`.
