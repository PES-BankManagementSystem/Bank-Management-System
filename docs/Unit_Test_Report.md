# Unit Test Report — S-42 (#118)

## Tested revision and environments

- **Commit tested (refreshed `origin/main`):** `31f1cf48659dbe1a559d6e5eeb321b1d30e16281` (S-40 #123 and S-25 #113 are merged).
- **Local test tree:** that commit plus the uncommitted S-42 test-only changes in this branch. The local results below therefore describe the working tree, not a committed S-42 revision.
- **macOS local:** AppleClang 17.0.0, arm64, CMake 4.4.4, Node.js 25.1.0, Python 3.12.2. JUnit: [ctest-junit-macos-local.xml](reports/ctest-junit-macos-local.xml).
- **Ubuntu CI:** [Build & test (ubuntu-latest), run #111](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060/job/127045275368) — success, tested SHA above.
- **Windows CI:** [Build & test (windows-latest), run #111](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060/job/127045275793) — success, tested SHA above.
- **Coverage CI:** [Code and branch coverage, run #111](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060/job/127045274980) — success. The `coverage-report` artifact (ID `13387499446`) is available from the [workflow run page](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/41966709060).

GitHub CLI is installed but `gh auth status` reported an invalid saved token. No login repair was attempted. Public run and artifact metadata were readable; downloading the artifact contents returned HTTP 401.

## Commands

Local build and test commands:

```sh
cmake -S . -B build-arm64
cmake --build build-arm64 --parallel 4
ctest --test-dir build-arm64 --output-failure --output-junit docs/reports/ctest-junit-macos-local.xml
```

CTest wrote the relative JUnit path under `build-arm64/`; the resulting XML was copied unchanged to `docs/reports/ctest-junit-macos-local.xml`.

The CI workflow ran these commands on both operating systems:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

**JUnit limitation:** CI does not pass `--output-junit` or upload a JUnit XML. Therefore per-target Ubuntu/Windows durations and XML are **NOT AVAILABLE: the CI workflow must emit and retain `ctest --output-junit` output to provide them**. The job results and links above are real CI results; the detailed target table below comes from the local JUnit run.

## Local CTest results

The final local run passed **31/31 targets**, with **0 failures, 0 skipped**, in **97.48 seconds**. The table is grouped by the principal module the target checks. Durations are the CTest JUnit testcase times; descriptions are from the test sources and CMake registration. CTest JUnit contains one testcase per target, not per GoogleTest assertion.

| Module | CTest target | Checks exercised | Result | Duration (s) |
|---|---|---|---:|---:|
| Core / API | `server_reports_version` | Server binary reports the configured project version. | Pass | 0.020 |
| Core / API | `server_shows_help` | `--help` prints the usage text. | Pass | 0.012 |
| Common | `config_test` | Configuration defaults, environment and command-line parsing/validation. | Pass | 0.003 |
| API | `api_server_test` | HTTP server, structured errors, static files, JSON validation, oversized bodies, generic errors and access-log redaction. | Pass | 0.544 |
| Common / DB | `db_test` | SQLite schema and seed loading, constraints, foreign keys, transactions and database operations. | Pass | 0.052 |
| Auth | `auth_login_test` | Login for each role, generic invalid-credential handling, input rejection, Argon2id hashes and secret-free logs. | Pass | 5.171 |
| Transaction | `transaction_test` | Deposit core/API behavior, invalid and unknown-account handling, account status and rollback on audit failure. | Pass | 0.546 |
| Auth | `auth_session_test` | Token identity, malformed/unknown/expired tokens, logout and deactivated-user sessions. | Pass | 3.381 |
| Transaction | `withdraw_test` | Minimum balance, daily limit, date accounting, invalid input, account states and rollback. | Pass | 0.667 |
| Auth | `auth_lockout_test` | Three-failure lockout, counter reset, admin unlock, role refusal and not-found/conflict errors. | Pass | 11.597 |
| Cross-module smoke | `module_tests` | Seeded admin and credential, API error mapping, callable registration for all nine modules. | Pass | 0.559 |
| Account | `account_type_test` | Account type CRUD/list, validation, uniqueness, authorization, audit rollback and future-operation behavior. | Pass | 2.055 |
| Account | `account_status_test` | Status transitions, roles, invalid requests, exact-zero close, atomicity, audit rollback and transaction refusal. | Pass | 2.541 |
| Account | `account_opening_test` | Account number generation/collision, role access, minimum balance, active customer, validation, atomic creation and rollback. | Pass | 3.977 |
| Customer | `customer_test` | Registration, validation, unique IDs, customer search/details/update, ownership, deactivation, audit rollback and migrations. | Pass | 26.146 |
| Transaction | `transfer_test` | Transfer signs/references, validation, same-account and balance/minimum refusals, atomic update and rollback. | Pass | 0.556 |
| Reporting | `interest_test` | Interest period/rate calculations, duplicate runs, authorization and rollback. | Pass | 0.549 |
| Reporting | `statement_test` | Opening/closing balances, inclusive dates, empty range, ownership and transfer signs. | Pass | 2.422 |
| Reporting | `account_history_test` | Ownership and auditing, ordering, inclusive filters, pagination, invalid filters and empty history. | Pass | 4.370 |
| Reporting | `statement_csv_test` | CSV headers, encoding, quoting and spreadsheet-formula neutralization. | Pass | 0.112 |
| Account | `account_balance_test` | Customer account list and ownership, status/type display, balance formatting and ledger reconciliation. | Pass | 5.298 |
| Performance helpers | `perf_helpers_test` | Pure helper checks used by local performance scripts; this is not an end-to-end latency run. | Pass | 0.110 |
| Staff | `staff_test` | Staff create/list/update/deactivate, roles, sessions, historical attribution and dashboards. | Pass | 9.537 |
| Audit | `audit_test` | Audit record fields, transaction scope, immutability and rollback with balance changes. | Pass | 0.059 |
| Audit | `audit_viewer_test` | Admin-only audit browsing, filters, pagination, invalid filters and read-only behavior. | Pass | 9.241 |
| Auth / RBAC | `rbac_test` | Permission matrix, refused and anonymous requests, customer ownership and role-scoped routes. | Pass | 2.532 |
| API / TLS | `tls_test` | HTTPS startup, invalid certificate/key handling and plain HTTP behavior. | Pass | 0.152 |
| Auth / Logging | `log_secrets_test` | Passwords, one-time credentials and bearer tokens do not reach logs. | Pass | 2.747 |
| Cross-module validation | `validation_test` | API/core validation, no-mutation checks, customer query bounds and deterministic invalid-input fuzz requests. | Pass | 2.102 |
| Common / Calendar | `calendar_test` | UTC epoch/leap-day conversion and local-time round trip. | Pass | 0.018 |
| Cross-module error paths | `coverage_error_paths_test` | Closed database errors, invalid account generator rollback, forbidden customer deposit, and deposit/transfer overflow. | Pass | 0.385 |

There were no failing or skipped targets in the final run, so no bug issue links are required. An initial sandboxed run could not bind localhost test servers; the final run was repeated with local socket access and passed all targets.

## Test-file to requirement and Test Plan mapping

The primary Test Plan PDF v1.0 defines TC-01–TC-17. `TEST_PLAN_CONSOLIDATED.md` v1.1 adds TC-18–TC-22 and changes several earlier meanings. The supplementary Test Plan PDF repeats the primary mapping; the separate Test Cases DOCX assigns several overlapping numbers differently. Both mappings are shown where relevant; no numbering conflict is silently resolved. The prior documentation discrepancy is also recorded in [deviation 15](DEVIATIONS.md).

| TC ID | Primary Test Plan v1.0 and supplementary PDF | Consolidated Test Plan v1.1 | Separate Test Cases DOCX |
|---|---|---|---|
| TC-04 | Missing customer phone / FR-3 | Missing customer phone / FR-3 | Account opening minimum / FR-4 in DOCX. |
| TC-05 | Account opening minimum / FR-4 | Same meaning | Frozen-account operations / FR-5 in DOCX. |
| TC-06 | Frozen account / FR-5 | Withdrawal minimum and daily limit / FR-7 | Withdrawal minimum and daily limit / FR-7 in DOCX. |
| TC-07 | Deposit / FR-6 | Freeze and close account / FR-5 | Atomic transfer / FR-8 in DOCX. |
| TC-08 | Withdrawal minimum / FR-7 | Deposit / FR-6 | Account history ownership / FR-9/FR-10 in DOCX. |
| TC-09 | Atomic transfer / FR-8, NFR-3 | Daily withdrawal limit / FR-7 | Interest duplicate prevention / FR-11 in DOCX. |
| TC-10 | Same-account transfer / FR-8 | Atomic transfer / FR-8 | Performance / NFR-1 in DOCX. |
| TC-11 | Balance/history ownership / FR-9/FR-10 | Same ownership mapping | Credential protection plus audit trail / NFR-4/NFR-5 in DOCX. |
| TC-12 | Interest duplicate prevention / FR-11 | Bank Officer creation / FR-13 | No TC-12 in the supplementary set. |
| TC-13 | Staff deactivation / FR-13 | Staff deactivation and session invalidation / FR-13 | No TC-13 in the supplementary set. |
| TC-14 | Performance / NFR-1 | Role-specific dashboards / FR-14/NFR-2 | No TC-14 in the supplementary set. |
| TC-15 | Credential protection / NFR-4 | TLS / SEC-2 | No TC-15 in the supplementary set. |

The course **Deliverables Part-3** source document referenced by the issue is not present in `docs/`; its requested clauses are taken from issue #118 as supplied, not inferred from another deliverable document.

| Test file | Requirement IDs | Test Plan cases / mapping note |
|---|---|---|
| `tests/api_server_test.cpp` | NFR-7, NFR-4 | Primary TC-17 input/error handling and TC-15 log secrecy; consolidated TC-15 TLS is covered separately in `tls_test.cpp`. |
| `tests/config_test.cpp` | No direct FR/NFR ID | Configuration/parser infrastructure; no dedicated case in either Test Plan. |
| `tests/db_test.cpp` | NFR-3, NFR-7 | Supports primary TC-09 atomicity and TC-17 error handling; consolidated TC-10 / TC-10B. No direct DB-wrapper case is specified. |
| `tests/auth_login_test.cpp` | FR-1, NFR-4, SEC-2 | Primary TC-01 and TC-15; consolidated TC-01 / TC-15 credential handling. |
| `tests/transaction_test.cpp` | FR-6, NFR-5, NFR-7 | Primary TC-07 and TC-16/TC-17; consolidated TC-08 and TC-16. Deposit confirmation UI is outside this core test. |
| `tests/auth_session_test.cpp` | FR-2, SEC-1 | Consolidated TC-03; primary TC-02 includes logout and lockout but does not separately specify expiry. |
| `tests/withdraw_test.cpp` | FR-7, NFR-7 | Primary TC-08 and TC-17; consolidated TC-06 and TC-09. |
| `tests/auth_lockout_test.cpp` | FR-2 | Primary TC-02; consolidated TC-02 and TC-02B administrative unlock. |
| `tests/module_tests.cpp` | NFR-8 (smoke/support only) | No direct formal TC; verifies module route registration and common/API smoke behavior. |
| `tests/account_type_test.cpp` | FR-4, FR-7 | Supports primary TC-05/TC-08 and consolidated TC-05/TC-06; neither plan gives account-type administration its own case. |
| `tests/account_status_test.cpp` | FR-5, NFR-3 | Primary TC-06; consolidated TC-07; forced rollback relates to primary TC-09 / consolidated TC-10. |
| `tests/account_opening_test.cpp` | FR-4, NFR-3 | Primary and consolidated TC-05. |
| `tests/customer_test.cpp` | FR-3, NFR-4, NFR-5 | Primary TC-03/TC-04; consolidated TC-04. Customer search/update/deactivation have no distinct primary-plan TC. |
| `tests/transfer_test.cpp` | FR-8, NFR-3, NFR-7 | Primary TC-09/TC-10/TC-17; consolidated TC-10/TC-10B. |
| `tests/interest_test.cpp` | FR-11, NFR-3 | Primary TC-12; consolidated TC-09B. |
| `tests/statement_test.cpp` | FR-12 | Consolidated TC-18. Primary plan has no dedicated FR-12 case and calls for exploratory/UI verification. |
| `tests/account_history_test.cpp` | FR-10, SEC-1, NFR-7 | Primary TC-11/TC-17; consolidated TC-11/TC-17. |
| `tests/statement_csv_test.js` | FR-12 | Consolidated TC-18; no dedicated primary-plan case. |
| `tests/account_balance_test.cpp` | FR-9, SEC-1 | Primary TC-11; consolidated TC-11. |
| `tests/perf_helpers_test.py` | NFR-1 (helper only) | Supports primary TC-14 / consolidated TC-20 scripts; it does not itself verify response time. |
| `tests/staff_test.cpp` | FR-13, FR-14, NFR-2 | Primary TC-13; consolidated TC-12/TC-13 and TC-14/TC-19 dashboard checks. |
| `tests/audit_test.cpp` | NFR-3, NFR-5 | Primary TC-09/TC-16; consolidated TC-10/TC-16. |
| `tests/audit_viewer_test.cpp` | NFR-5, SEC-1 | Primary TC-16; consolidated TC-16. |
| `tests/rbac_test.cpp` | FR-1, FR-9, FR-11, SEC-1 | Primary TC-01/TC-11; consolidated TC-01/TC-11. Interest-role refusal is not a separate Test Plan case. |
| `tests/tls_test.cpp` | SEC-2, NFR-4 | Consolidated TC-15; primary TC-15 instead covers NFR-4 password/PIN protection. |
| `tests/log_secrets_test.cpp` | NFR-4, SEC-2 | Primary TC-15; consolidated TC-01 security controls. |
| `tests/validation_test.cpp` | NFR-7 | Primary TC-17; consolidated TC-02/TC-09/TC-10B/TC-11 error handling. |
| `tests/calendar_test.cpp` | NFR-9 | Consolidated TC-22 portability; no primary-plan case. |
| `tests/coverage_error_paths_test.cpp` | FR-4, NFR-3, NFR-7, SEC-1 | Primary TC-05/TC-09/TC-17; consolidated TC-05/TC-10/TC-10B. |

## Deviations and limitations

No production behavior changed and no SRS/SADS behavior deviation was introduced. The existing primary-versus-consolidated Test Plan numbering differences remain documented above and in deviation 15. The S-40 workflow does not retain CI JUnit XML, so platform-specific per-target durations are unavailable. The CI coverage artifact is linked above; its contents require an authenticated GitHub download. Local Mac measurements are supplemental, not substitutes for the requested CI coverage values.
