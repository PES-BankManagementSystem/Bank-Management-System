# System Validation Report

| | |
|---|---|
| **Backlog item** | S-43 (#119) |
| **Execution date** | 2026-11-06 |
| **Execution baseline** | Latest merged `main`, `b66f97c` |
| **Proposed test target** | Test Plan v1.2 in [S-41 PR #124](https://github.com/PES-BankManagementSystem/Bank-Management-System/pull/124); the PR is open and mergeable, so the plan is not yet merged |
| **Result** | **Incomplete — exit criteria are not met** |

## Prerequisite review - 2026-11-06

Fresh `git fetch origin` confirmed main at `b66f97c0179f2e91ad8c36f020d403c426ca880d`. The validation branch remains at `e74a8fcf08ecc05747ae53dc5c8675645925901e`. Existing uncommitted root `CMakeLists.txt` and deleted `db/.gitkeep` were left untouched. S-41 [PR #124](https://github.com/PES-BankManagementSystem/Bank-Management-System/pull/124) is **OPEN, MERGEABLE**, at `aff2a7bd71d175b740e1e78415cb3f083d6c61f2`. Its merge is the required gate for final-plan execution; no merge/rebase or new application test execution was performed in this review, and there is **no new validated build SHA**. S-25 is MERGED via PR #113, merged 2026-11-03 at commit `42e174ce28a270e782873a1c7177df9194e0e730`; issue #57 is closed. Open PRs are #124 (Test Plan), #129 (SADS/final demo documentation), and #130 (this validation report); no open application-code PR was listed. The open-issue listing and bug-label query returned no open bug issues.

Chrome is installed at `C:/Program Files/Google/Chrome/Application/chrome.exe`. Firefox was not found in standard system/per-user installation paths or Windows App Paths registration. Neither browser was exercised in this review; TC-22 remains **PARTIAL**, with Firefox unavailable in the checked locations. See [prerequisite evidence](./reports/system-validation/prerequisite-review/2026-11-06.json).

Waiting for #124: integrate main without force-pushing; record the resulting build SHA; build and run TC-03 with the merged 204 expectation, old-token 401 and browser sign-in redirect; independently fresh-seed TC-01, TC-02, TC-02B, TC-03, TC-04, TC-04B, TC-05, TC-06, TC-07, TC-08, TC-09, TC-09B, TC-10, TC-10B, TC-11, TC-12, TC-13, TC-14, TC-16, TC-17, TC-18, TC-19 and TC-20; and run TC-22 core/responsive flows in available browsers. TC-21 remains NOT RUN: requires three genuine first-time participants; none arranged in the validation window. Historical results below are retained as observations, not final fresh-seed acceptance.

## PR integration review

To resolve PR #130 conflicts, main at `b66f97c` was used as the application baseline and the S-43 report/evidence were carried forward in an isolated worktree. The merge retains both histories. Application, frontend, database, build configuration and tests match main; older account commits on the validation branch do not override their already-merged versions. Main's deviations and review log are preserved, with S-43 entries renumbered 28-30. The original working directory's local changes were untouched. This is a documentation integration, not a new validation run. S-41 #124 remains open; final-plan reruns and the exit decision remain pending.

## 1. Scope and method

The [SRS v1.1](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/b66f97c/docs/SRS_v1.1_UPDATE.md) is the requirements baseline. The v1.2 plan was used as the execution target because it adds and clarifies TC-04B, TC-05, TC-07, TC-09, and TC-19. Results from the initial API/UI run against `42e174c` are retained where the corresponding case was unchanged; focused v1.2 retests, dashboard/TLS checks, and the build/CTest run used `b66f97c`. All server-side checks used disposable databases. No production data was used.

Only TC-15 is confirmed to have run against a fresh database initialized from schema and seed specifically for that case. These executed cases lack a confirmed, independent fresh-seed run: TC-01, TC-02, TC-02B, TC-03, TC-04, TC-04B, TC-05, TC-06, TC-07, TC-08, TC-09, TC-09B, TC-10, TC-10B, TC-11, TC-12, TC-13, TC-14, TC-16, TC-17, TC-18, TC-19, TC-20, and TC-22. TC-01 through TC-14 except TC-15 and TC-16 through TC-18 were exercised sequentially on shared disposable seeded databases. TC-19 and TC-20 used purpose-built 3,005-account performance data; the evidence does not establish a separate fresh seed/reset for each case. TC-22 lacks fresh cross-platform browser runs and corresponding test databases. This is a limitation, not a pass against the fresh-seed-per-case criterion. UTC-boundary testing used a synthetic prior-day transaction seeded before the server started; the append-only trigger correctly prevented changing an existing transaction timestamp.

The browser UI was used for TC-08 and TC-18. TC-21 was **NOT RUN** because it requires three genuine first-time participants; none were arranged in the validation window. No results were simulated. TC-22 has current Ubuntu/Windows CI evidence, but no fresh frontend walkthrough was run in both Google Chrome and Mozilla Firefox, and responsive rendering was not compared across those browsers.

## 2. Run summary

| Result | Cases |
|---|---|
| Historical pass (fresh-seed acceptance outstanding except TC-15) | 23 |
| Partial | 2 (TC-03, TC-22) |
| Not run | 1 (TC-21) |
| Failed | 0 |
| Historically executed (includes partial) | 25 |
| Total planned | 26 |
| Confirmed independent fresh-seed passes | 1 (TC-15) |

The all-state issue search found no existing bug issue for the TC-03 logout-status mismatch (the logout search found only the closed S-08 issue [#38](https://github.com/PES-BankManagementSystem/Bank-Management-System/issues/38); searching `204` found none). No new bug issue is raised: the observed 204 is the current endpoint behavior, and the 200 expectation needs reconciliation in S-41 [PR #124](https://github.com/PES-BankManagementSystem/Bank-Management-System/pull/124), rather than presuming which side is defective. The open-issue review found no open bug issue or critical defect. TC-03 remains partial. Execution limitations and deviations are recorded in [DEVIATIONS.md](./DEVIATIONS.md).

## 3. Build and automated tests

- Latest merged `main` (`b66f97c`) built successfully with MinGW/OpenSSL.
- CTest: **31/31 passed**.
- GitHub Actions run [42039320517](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42039320517): Ubuntu and Windows build-and-test jobs passed.
- `git diff --check` passed; all 16 evidence JSON files parsed, the 37 local report links resolved, and the added report/evidence files had no trailing whitespace.

## 4. Historical test-case execution

The PASS labels below describe historical observations only; all cases other than TC-15 still lack the required independent fresh-seed acceptance. No case was newly executed in this prerequisite review.

The initial API evidence is in [api-results.json](./reports/system-validation/evidence/api-results.json); credentials, bearer tokens, and temporary passwords are redacted. Focused v1.2 evidence is linked per case.

| Case / role | Steps run | Expected result | Actual result | Status | Evidence | Requirements |
|---|---|---|---|---|---|---|
| **TC-01 — Valid authentication**; Administrator, Bank Officer, Customer | Sign in as each seeded role and inspect response shape. | HTTP 200, correct role and 64-character token; no password returned. | All three roles returned HTTP 200 with correct roles and 64-character tokens; no credentials are recorded. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-1, SEC-2 |
| **TC-02 — Three-failure lockout**; Customer | Submit three incorrect passwords and inspect user state. | First two attempts are generic 401s; third is `ACCOUNT_LOCKED`; user is locked with three failures. | Three 401s; third returned `ACCOUNT_LOCKED`; state was locked with three failures. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-2, NFR-7 |
| **TC-02B — Administrative unlock**; Administrator, Customer | Lock Customer2, unlock it, inspect state, then sign in. | Unlock returns 200, restores active/zero-failure state, and permits login. | Unlock returned 200; state became active with zero failures; subsequent login returned 200. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-2, FR-13 |
| **TC-03 — Logout and invalidation**; Bank Officer | Log in, log out, then request the protected dashboard with the old token. | Logout returns HTTP 200; the invalidated token receives 401; the UI redirects to sign-in. | Logout returned 204 No Content, while the subsequent dashboard request returned 401. The UI redirect after logout was not verified. The status differs from the plan; see the S-41 reconciliation note below. | **PARTIAL — status mismatch and UI step** | [API results](./reports/system-validation/evidence/api-results.json) | FR-2, SEC-1 |
| **TC-04 — Customer registration validation**; Bank Officer | Submit a missing-phone registration, then valid unique customer records. | Missing phone receives a field error; valid requests create unique customer identifiers. | Missing phone returned 400 with a `phone` field error; valid requests returned 201 with distinct customer codes. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-3 |
| **TC-04B — Administrator-only customer deactivation**; Administrator, Bank Officer, Customer | Try deactivation by Officer and Customer; deactivate as Administrator, repeat, then test login/account opening and retained data. | Only Administrator succeeds; repeat is idempotent; sessions and new account opening are refused; existing records remain. | Officer/Customer received 403; Administrator and repeat returned 200; old session and fresh login returned 401; new account opening was refused with 409 `CUSTOMER_INACTIVE`; existing account/transaction counts were unchanged. | **PASS** | [TC-04B evidence](./reports/system-validation/evidence/tc04b-v12.json) | FR-3, SEC-1 |
| **TC-05 — Minimum and zero-deposit account opening**; Bank Officer | Try below-minimum, exact-minimum, and zero deposit on a zero-minimum type; inspect account and opening transaction. | Below-minimum returns 422 without rows; valid cases return 201 and record the opening transaction, including zero where permitted. | Below-minimum returned 422 with no account created; exact-minimum and zero-minimum/zero-deposit openings returned 201; the zero opening was recorded as INR 0.00. | **PASS** | [TC-05 evidence](./reports/system-validation/evidence/tc05-v12.json) | FR-4 |
| **TC-06 — Minimum balance and daily limit**; Bank Officer | Attempt minimum-balance and daily-limit violations, then a valid withdrawal; compare balances. | Invalid withdrawals are rejected without balance changes; valid withdrawal is recorded. | Both invalid requests returned 422 with specific errors and no balance change; valid withdrawal returned 201 and was recorded. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-7, NFR-7 |
| **TC-07 — Freeze and close account**; Bank Officer | Exercise freeze/unfreeze/close, non-zero close, repeated actions, and deposit/withdrawal/transfer on frozen or closed accounts. | Non-zero close returns 409; valid transitions work; frozen/closed transactions are rejected; history remains. | Non-zero close returned 409 `ACCOUNT_BALANCE_NONZERO`; transitions and repeated actions behaved as specified; frozen transactions returned 422; retained account/history remained available. | **PASS** | [TC-07 evidence](./reports/system-validation/evidence/tc07-v12.json) | FR-5 |
| **TC-08 — Confirmed deposit**; Bank Officer | Use the deposit UI, review and accept the INR 5,000 confirmation for ACC001; inspect success and ledger. | Dialog identifies account and amount; confirmation records the deposit and updated balance. | Dialog named ACC001 and INR 5,000; UI reported success; database ledger records the deposit by the Bank Officer. Balance moved from INR 50,010 to INR 55,010. The plan's INR 10,000 starting-balance precondition differs from the seed. | **PASS — seed variance** | [TC-08 UI/ledger evidence](./reports/system-validation/evidence/tc08-v12-ui.json), [prior screenshot](./reports/system-validation/evidence/TC08-deposit-success.png) | FR-6 |
| **TC-09 — Daily withdrawal limit and UTC boundary**; Bank Officer | Exceed the configured daily limit, verify unchanged balance, then compare withdrawals across a UTC date boundary using a synthetic prior-day row. | Excess returns 422 `DAILY_LIMIT_EXCEEDED`; rejected balance is unchanged; UTC dates have separate totals. | Limit rejection returned 422 without balance change; a post-boundary withdrawal succeeded against a synthetic prior-day fixture. Existing transaction timestamps could not be edited due to append-only enforcement. | **PASS** | [TC-09 evidence](./reports/system-validation/evidence/tc09-v12.json) | FR-7, NFR-7 |
| **TC-09B — Interest once per period**; Administrator | Run interest for a period, then repeat the same period. | First run credits once; duplicate run is rejected without another balance change. | First run returned 201 and credited; duplicate returned 409 `INTEREST_ALREADY_RUN`. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-11 |
| **TC-10 — Atomic transfer**; Bank Officer | Transfer between two accounts and compare balances and debit/credit references. | Both balances update atomically and entries share a transfer reference. | Transfer returned 201; balances updated as expected and the debit/credit entries shared a reference. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-8, NFR-3 |
| **TC-10B — Invalid transfers**; Bank Officer | Try same-account and insufficient-funds transfers; compare balances and ledger. | Both are rejected with no balance or ledger mutation. | Both requests were rejected; account balances and transaction counts were unchanged. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-8, NFR-7 |
| **TC-11 — Customer balance/history access**; Customer | Read own balance/history; request another customer's account and inspect audit. | Own data succeeds; cross-customer access is denied consistently and denial is audited. | Own requests returned 200; other/unknown account requests returned the same 403; denial audit row was present. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-9, FR-10, SEC-1 |
| **TC-12 — Create Bank Officer**; Administrator | Create a staff user, authenticate as the new Officer, and change the initial password. | HTTP 201; protected credential storage; new Officer can authenticate. | Creation returned 201; new Officer authenticated and completed the required password change. Temporary password is redacted. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-13 |
| **TC-13 — Deactivate Bank Officer/session**; Administrator, Bank Officer | Create Officer, record a deposit, deactivate the Officer, use old token, and inspect attribution. | Officer becomes inactive, old session is rejected, historical operator identity remains. | Deactivation returned 200; old token received 401; historic transaction retained the deactivated operator ID. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json), [TC-13 evidence](./reports/system-validation/evidence/tc13-staff-session-attribution.json) | FR-13, SEC-1 |
| **TC-14 — Role-specific dashboards**; Administrator, Bank Officer, Customer | Call `GET /dashboard` as each role; inspect role data and response times. | Role-specific data only; each response is under 3 seconds. | All roles returned HTTP 200 with distinct expected fields; customer accounts matched database ownership. Responses took 2.83–10.77 ms. | **PASS** | [TC-14 evidence](./reports/system-validation/evidence/tc14-v12-dashboard.json), [prior screenshots](./reports/system-validation/evidence/TC14-bank-officer-dashboard.png), [customer dashboard screenshot](./reports/system-validation/evidence/TC14-customer-dashboard.png) | FR-14, NFR-2 |
| **TC-15 — HTTPS/TLS**; Administrator | Start TLS-enabled server with a fresh seed; constrain client to TLS 1.3, sign in, and send plaintext HTTP to the TLS listener. | TLS 1.3 and HTTPS login succeed; plaintext is refused. | TLS 1.3 negotiated; HTTPS login returned 200 as Administrator; plaintext connection was reset/closed without an HTTP response. | **PASS** | [TC-15 latest-main evidence](./reports/system-validation/evidence/tc15-v12-tls.json), [earlier TLS evidence](./reports/system-validation/evidence/tc15-tls.json) | FR-1, SEC-2 |
| **TC-16 — Append-only audit**; Administrator | Inspect audit rows for financial actions and attempt update/delete through the application. | Financial actions have audit entries; edits/deletes are refused. | Audit entries were present; update and delete attempts returned 405. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | NFR-5 |
| **TC-17 — Date-filtered history**; Bank Officer | Request history with inclusive date bounds across pages; inspect ordering, signs, and duplicates. | Only in-range entries, newest first, signed correctly, without duplicate pages. | Both pages returned 200; entries were newest-first, debits negative, and no duplicates were observed. | **PASS** | [API results](./reports/system-validation/evidence/api-results.json) | FR-10 |
| **TC-18 — Statement and CSV export**; Customer | View statement and click Download CSV; inspect generated CSV rows and balances. | HTTP 200; chronological statement with opening/closing balances, entries, and formatted export. | Statement showed 10 recent deposits; the button generated a 707-byte CSV Blob (17 lines) with chronological entries, INR 50,000 opening and INR 50,010 closing balances. The Blob was fetched before its one-second revocation; the browser's native save event was not captured. | **PASS — Blob content verified** | [CSV evidence](./reports/system-validation/evidence/tc18-csv-download.json), [statement screenshot](./reports/system-validation/evidence/TC18-customer-statement.png) | FR-12 |
| **TC-19 — Account-list/history read performance**; Customer, Bank Officer | On 3,005 accounts, collect 30 customer-dashboard and 30 date-filtered history samples. | Authorized results preserve filters/order and remain under 3 seconds. | All samples returned successful responses and authorized account data; max customer dashboard was 29.534 ms and max history was 27.829 ms. | **PASS** | [TC-19 performance evidence](./reports/system-validation/evidence/tc19-v12-performance.json) | FR-9, FR-10, NFR-2 |
| **TC-20 — Transaction performance**; Bank Officer | On 3,005 accounts, warm each operation with 3 samples, then measure 30 persisted deposits, withdrawals, and transfers. | Each transaction completes within 2 seconds including persistence. | All 90 samples passed and persistence matched responses. Mean/max: deposit 14.563/45.378 ms; withdrawal 16.613/33.362 ms; transfer 13.939/30.162 ms. | **PASS** | [TC-20 performance evidence](./reports/system-validation/evidence/tc20-v12-performance.json) | NFR-1 |
| **TC-21 — First-time usability walkthrough**; 3 first-time participants | No session with the three first-time participants was scheduled for this run. | All three complete both tasks unaided on first attempt; findings are documented. | **NOT RUN** — requires three genuine first-time participants; none arranged in the validation window. No participant results were simulated. | **NOT RUN** | No participant evidence; [proposed v1.2 plan](https://github.com/PES-BankManagementSystem/Bank-Management-System/pull/124) | NFR-6 |
| **TC-22 — Cross-platform/browser portability**; Ubuntu/Windows, Chrome/Firefox | Verify current CI for both operating systems and review browser evidence. | Current build runs on Ubuntu and Windows; UI renders responsively and identically in Chrome and Firefox. | Latest-main Ubuntu and Windows build/test jobs passed. The integrated browser was Chromium-based. Missing: fresh Chrome and Firefox UI runs against this revision, responsive-layout checks in both, and a comparison of their rendering. Historical S-31 results do not fill these gaps. | **PARTIAL** | [CI run 42039320517](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42039320517), [historical S-31 portability report](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/b66f97c/docs/Test_Summary_Report.md#portability-verification-nfr-9-s-31) | NFR-9 |

## 5. Requirement validation

A requirement is validated only when all linked cases pass the final fresh-seed criterion. Historical observations remain in the evidence/gap column; they do not establish final acceptance. NFR-8 retains its existing build/CTest validation, which is independent of database state.

| Requirement | Test case IDs | Status | Evidence or gap |
|---|---|---|---|
| FR-1 | TC-01, TC-15 | Not validated | Authentication and TLS login passed. |
| FR-2 | TC-02, TC-02B, TC-03 | Not validated | Lockout/unlock and session invalidation passed; TC-03 returned 204 instead of the plan's 200. |
| FR-3 | TC-04, TC-04B | Not validated | Registration validation and Administrator-only deactivation passed. |
| FR-4 | TC-05 | Not validated | Below-minimum rejection, exact minimum, and zero-minimum opening passed. |
| FR-5 | TC-07 | Not validated | Freeze/unfreeze/close, blocked transactions, and retained history passed. |
| FR-6 | TC-08 | Not validated | UI confirmation and persisted deposit passed; seed balance differed from the plan precondition. |
| FR-7 | TC-09 | Not validated | Daily-limit and UTC-boundary behavior passed; TC-06 additionally covered minimum-balance rules. |
| FR-8 | TC-10 | Not validated | Atomic transfer passed; TC-10B additionally covered invalid-transfer rejection. |
| FR-9 | TC-11 | Not validated | Account balance ownership checks passed. |
| FR-10 | TC-11, TC-17 | Not validated | Ownership, date filtering/order, and pagination passed. |
| FR-11 | TC-09B | Not validated | Interest credited once; duplicate run rejected. |
| FR-12 | TC-18 | Not validated | Statement and generated CSV content passed; native browser save event was not captured. |
| FR-13 | TC-02B, TC-12, TC-13 | Not validated | Administrative unlock and staff/customer lifecycle behavior passed. |
| FR-14 | TC-14 | Not validated | Role-specific dashboards returned the expected role data. |
| NFR-1 | TC-20 | Not validated | 30 measured transactions per operation; maximum was 45.378 ms against 2 seconds. |
| NFR-2 | TC-14, TC-19 | Not validated | Role dashboards and account-list/history reads were below 3 seconds. |
| NFR-3 | TC-10; CTest | Not validated | Atomic transfer behavior and 31/31 CTest targets passed. |
| NFR-4 | TC-01; CTest | Not validated | Authentication evidence and the automated test suite passed. |
| NFR-5 | TC-16 | Not validated | Financial audit entries were present; mutation attempts returned 405. |
| NFR-6 | TC-21 | Not validated | Three-person first-time participant study was not run. |
| NFR-7 | TC-02, TC-09, TC-10B, TC-11 | Not validated | Structured rejection paths and unchanged-state checks passed. |
| NFR-8 | CMake build; CTest | Validated | Latest-main build succeeded; 31/31 CTest targets passed. |
| NFR-9 | TC-22 | Not validated | Ubuntu/Windows CI passed; fresh Chrome and Firefox UI checks, responsive checks in both, and cross-browser rendering comparison are missing. |
| SEC-1 | TC-03, TC-11, TC-13 | Not validated | Logout/deactivation invalidation and ownership checks passed. |
| SEC-2 | TC-01, TC-15 | Not validated | Credential handling and TLS 1.3 checks passed. |

## 6. Story manual-test notes

The manual notes are referenced here; related automated/API checks do not substitute for every UI step in those notes.

| Manual note | Related coverage | Not freshly repeated |
|---|---|---|
| [S-13 customer deactivation](./S-13-MANUAL-TEST.md) | TC-04B and TC-13 cover customer/staff lifecycle and session invalidation. | Full customer-deactivation UI walkthrough. |
| [S-14 account types](./S-14-MANUAL-TEST.md) | TC-06/TC-09 and [zero-limit retest](./reports/system-validation/evidence/s14-zero-limit.json) cover withdrawal limits. | Full Administrator add/edit/duplicate/field-validation UI flow. |
| [S-15 account opening](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/b66f97c/docs/S-15-MANUAL-TEST.md) | TC-05 covers API rejection, opening, and ledger creation. | Full review/back/confirm UI flow. |
| [S-16 account status](./S-16-MANUAL-TEST.md) | TC-07 covers status transitions and retained history. | Full status-management UI walkthrough. |
| [S-21 transaction history](./S-21-MANUAL-TEST.md) | TC-11/TC-17/TC-19 cover ownership, filters, ordering, pagination, and performance. | Full manual history-page walkthrough. |

## 7. Defects and Test Plan exit criteria

- **Defects:** No product defect requiring a bug issue was identified in completed checks; no bug issue was raised. The TC-03 status mismatch is recorded as a plan/API deviation.
- **All planned cases executed:** **Not met.** TC-21 was not run; TC-22 remains partial.
- **Freshly seeded database per case:** **Not met.** TC-01–TC-14 except TC-15, TC-16–TC-20, and TC-22 lack confirmed independent fresh-seed runs; see the explicit case list above.
- **Exit-criteria source:** The proposed v1.2 plan contains no explicit exit-criteria section; the S-43 criteria were checked directly. All-cases-executed is not met, while no-critical-defect-open is met.
- **No critical defect open:** **Met** at the time of the issue review.
- **Overall Test Plan exit decision:** **Not met.**
- **S-43 definition of done:** A reviewed/merged PR that closes #119, green CI for that PR, and repo-owner approval remain outstanding.

**Overall exit-criteria verdict: NOT MET.** The proposed S-41 v1.2 plan must be merged before it can be treated as the final approved baseline. TC-21 remains **NOT RUN** for the exact reason above. Remaining work is to run the listed cases on individually fresh-seeded databases; verify TC-03 against the merged expectation (the open S-41 revision specifies HTTP 204), including its logout UI redirect, in S-41 [PR #124](https://github.com/PES-BankManagementSystem/Bank-Management-System/pull/124); and complete the Chrome/Firefox responsive and visual comparison for TC-22. No session time is proposed or booked.
