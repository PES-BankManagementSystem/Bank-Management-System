# Bank Management System

Software Test Plan

Version 1.3 | 7 November 2026 | IEEE Std 829-2008 format

PES University | Software Engineering and System Design | UE24CS341A

Team: Abhishek Hanamant Kinagi, Adithya Ajeeth, Aditya M and Aditya Vats.

Test Plan Identifier: **TP-BMS-2026-v1.3**. Governing requirements: SRS v1.2 (6 November 2026). This revision restores the IEEE 829 test plan sections that the v1.2 reissue dropped and brings them up to date with the built system. No tests were executed for this revision; the latest execution results are those of the S-43 System Validation Report (7 November 2026).

## Revision History

| Version | Date | Author | Changes |
|---|---|---|---|
| 1.0 | 15 Sep 2026 | QA Team | Initial IEEE 829 test plan with sections 1-14 and TC-01 to TC-17, written against SRS v1.0. Separate standalone test-case documents also existed. |
| 1.1 | 1 Nov 2026 | Abhishek Kinagi and team | S-35 (#103): merged the standalone suites into section 14 with continuous TC IDs; added TC-02B and TC-18 to TC-22; corrected the SRS citation date to 2 Sep 2026; built the requirements traceability matrix. |
| 1.2 | 4 Nov 2026 | Abhishek Kinagi and team | S-41 (#124): rechecked every case against the merged code and the deviations log; added the zero opening deposit, freeze/close request bodies, UTC daily-limit boundary and Administrator-only customer deactivation; made TC-19 the read-performance case; corrected TC-03 logout to HTTP 204; removed the superseded standalone files. The reissued PDF kept only section 5.1 and section 14. |
| 1.3 | 7 Nov 2026 | Aditya Vats | Restored IEEE 829 sections 1-13 from v1.0 and updated them for the built system and SRS v1.2. The suite classification moves into section 5.1, and the traceability matrix and cases into section 14. Each case now states its objective, type, priority and numbered steps. TC-01 uses the implemented `username` field. Amounts are written as INR. The matrix covers FR-15 to FR-17 and SEC-3 to SEC-6 and lists their coverage gaps. Case IDs and expected results are otherwise unchanged. |

<!-- pagebreak -->

## 1. Introduction

### 1.1 Purpose

This Test Plan defines the scope, approach, resources and schedule for testing the Bank Management System (BMS). It is written against the Software Requirements Specification (SRS) v1.2 dated 6 November 2026 and follows the test plan outline of IEEE Std 829-2008, as used by the course template. It is a project document; no formal IEEE conformance assessment is claimed.

### 1.2 Scope

Testing covers the functional requirements FR-1 to FR-17, the nonfunctional requirements NFR-1 to NFR-9 and the security requirements SEC-1 to SEC-6 of SRS v1.2. These span authentication and sessions, customer and account management, deposits, withdrawals and transfers, balances and transaction history, interest and statements, staff management, role dashboards, account types, user unlock and the audit trail. Features outside the product scope are listed in section 4.2.

### 1.3 References

| Reference | Document |
|---|---|
| R-1 | IEEE Std 829-2008, IEEE Standard for Software and System Test Documentation. |
| R-2 | Software Requirements Specification v1.2, `docs/SRS_Bank_Management_System.md` and PDF. |
| R-3 | Software Architecture and Design Specification v1.2, `docs/Software_Architecture_and_Design_Specification.docx` and PDF. |
| R-4 | Deviations log, `docs/DEVIATIONS.md`. |
| R-5 | System Validation Report (S-43, final 7 Nov 2026), `docs/System_Validation_Report.md`, with evidence in `docs/reports/system-validation/`. |
| R-6 | Test Summary Report (S-45), Unit Test Report, Code Coverage Report and Branch Coverage Report in `docs/`. |
| R-7 | Test Execution Log of the v1.0 run (S-29, 30 Oct 2026), `docs/Test_Execution_Log.md`. |
| R-8 | Manual procedures `docs/S-13-MANUAL-TEST.md` to `docs/S-21-MANUAL-TEST.md` and the TC-21 protocol `docs/USABILITY_WALKTHROUGH.md`. |
| R-9 | Software Engineering and System Design (UE24CS341A) course material, PES University. |

### 1.4 Document Conventions

Requirement identifiers follow SRS v1.2: FR for functional, NFR for nonfunctional, SEC for security requirements, SO for security objectives and BR for business rules. Test cases use the prefix TC- (for example TC-01). A letter suffix (for example TC-02B) marks a case added after v1.0 next to the related case, so that earlier IDs keep their meaning. Test suites use the prefix TS-. S-nn identifies the backlog story that delivered the feature. "Shall" marks a mandatory provision of this plan.

Amounts are in INR (the user interface shows them with the rupee sign) and timestamps are UTC. A case's priority is the SRS priority of its main functional requirement. Nonfunctional cases keep the v1.0 rule: security and integrity cases are High, the others Medium.

## 2. Test Plan Overview

| Field | Value |
|---|---|
| Test plan identifier | TP-BMS-2026-v1.3 (v1.0 was identified as BMS-TP-1.0) |
| Product | Bank Management System, C++17 back-end core with a browser front end |
| Governing requirements | SRS v1.2, 6 Nov 2026 |
| Latest execution | S-43 at `b0e8ca2` with the TC-22 retest at `00ffd7c`, 7 Nov 2026 (R-5) |
| Source and format | This Markdown file is the source; `docs/Test_Plan_BankManagementSystem.pdf` is exported from it by `scripts/export_test_plan.py`. |
| Status | Issued for team review; approvals in section 13 are not yet recorded. |

This plan is maintained with the SRS and design specification in the project's GitHub repository and is revised when requirements or implemented behavior change.

## 3. Test Items

The following items of the C++ back-end core, its database and the web front end are subject to test. Each execution records the source commit it used.

| Test item | Location | Description |
|---|---|---|
| Authentication and session module | `src/auth/` | Login, logout, session tokens, lockout, unlock, role-based access control |
| Customer management module | `src/customer/` | Customer registration, update, search and deactivation |
| Account management module | `src/account/` | Account opening, freeze, unfreeze and close; account-type configuration |
| Transaction module | `src/transaction/` | Deposits, withdrawals and fund transfers |
| Reporting module | `src/reporting/` | Balances, transaction history, interest crediting and statements |
| Staff management and dashboards | `src/staff/` | Bank Officer accounts and role-specific dashboards |
| Audit module | `src/audit/` | Append-only audit trail and Administrator audit viewer |
| API and common support | `src/api/`, `src/common/` | HTTP and HTTPS server, routing, error format, input validation, database helpers |
| Database | `db/` | Schema version 5, seed data and migrations (SQLite) |
| Web front end | `frontend/` | Presentation and data entry only; no business logic |
| Automated tests | `tests/` | CTest unit and integration tests |

## 4. Features to be Tested and Not to be Tested

### 4.1 Features to be Tested

| Requirement(s) | Feature | Test cases |
|---|---|---|
| FR-1, FR-2, SEC-1, SEC-5, SEC-6 | Sign-in, logout, session tokens, role-based access, lockout | TC-01, TC-02, TC-03, TC-11, TC-13 |
| FR-3 | Customer registration and deactivation | TC-04, TC-04B |
| FR-4, FR-5 | Account opening; freeze, unfreeze and close | TC-05, TC-07 |
| FR-6, FR-7 | Deposits and withdrawals, minimum balance and daily limit | TC-06, TC-08, TC-09 |
| FR-8, NFR-3 | Atomic fund transfer and invalid transfers | TC-10, TC-10B |
| FR-9, FR-10 | Balance inquiry and date-filtered transaction history | TC-11, TC-17 |
| FR-11, FR-12 | Interest crediting and account statements | TC-09B, TC-18 |
| FR-13, FR-14, FR-16 | Staff management, user unlock and role dashboards | TC-02B, TC-12, TC-13, TC-14 |
| FR-15 | Account-type configuration | Automated tests only (gap, see 14.1) |
| FR-17, SEC-4 | Audit viewer and append-only audit | TC-16 (partial) and automated tests |
| NFR-1, NFR-2 | Transaction and page-load performance | TC-14, TC-19, TC-20 |
| NFR-4, NFR-5, SEC-2, SEC-3 | Credential hashing, HTTPS, audit trail | TC-01, TC-12, TC-15, TC-16 |
| NFR-6 | Usability for first-time users | TC-21 |
| NFR-7 | Input validation and error handling | TC-02, TC-06, TC-09, TC-10B, TC-11 |
| NFR-8 | Modular architecture | CMake build and module inspection |
| NFR-9 | Portability across Linux, Windows, Chrome and Firefox | TC-22 |

### 4.2 Features Not to be Tested

The following are outside the product scope in SRS v1.2 sections 1.4 and Appendix D and are not tested: loan and EMI management; fixed and recurring deposits; cheque processing; ATM and debit-card operations; internet and mobile banking channels; interbank transfers (NEFT, RTGS, UPI); multi-branch and multi-currency operation; joint accounts and nominees; customer-initiated transfers; two-factor authentication and single sign-on; and concurrent multi-terminal operation with record-level locking.

This plan also does not cover concurrent load or stress testing, penetration testing, disaster recovery, encryption of the database at rest, tamper evidence against a privileged database administrator, completion of the operating system's native file-save dialog, or browsers and operating systems other than Chrome, Firefox, Linux and Windows.

## 5. Approach

### 5.1 Security Validation Approach

Security requirements (NFR-4, NFR-5, SEC-1 to SEC-6) are validated by:

- inspecting stored credentials to confirm that passwords are held only as salted Argon2id hashes and never appear in plain text in the database, logs or responses, apart from the documented one-time temporary password (DEVIATIONS.md row 3);
- confirming that the C++ back end enforces role and ownership checks on every protected request, so that a direct API call cannot bypass hidden user-interface controls, and that refused requests are logged;
- checking the session lifecycle: lockout on the third failed sign-in, Administrator-only unlock, token rejection after logout and after deactivation;
- checking HTTPS on a TLS-enabled build for network deployments;
- confirming that the audit trail is append-only by attempting to change or delete entries through the application;
- supplying invalid, boundary and adversarial input (negative or non-numeric amounts, unknown account numbers) to confirm that the system fails safely without changing data.

The cases are grouped into the following suites. TS-01 and TS-04 hold the security validation cases, and TC-11 is the primary ownership and access-denial case.

| Suite ID | Suite name | Description | Test cases |
|---|---|---|---|
| TS-01 | Authentication and lockout | Credential checks, session creation, lockout after three failures, Administrator unlock, logout | TC-01, TC-02, TC-02B, TC-03 |
| TS-02 | Customer and account operations | Customer registration, Administrator-only customer deactivation, account opening, status management | TC-04, TC-04B, TC-05, TC-06, TC-07 |
| TS-03 | Financial transactions | Deposits, withdrawals, daily limit, interest, atomic transfers | TC-08, TC-09, TC-09B, TC-10, TC-10B |
| TS-04 | Security and access control | Least privilege, role barriers, HTTPS, audit trail | TC-11, TC-14, TC-15, TC-16 |
| TS-05 | Staff and administration | Bank Officer creation and deactivation with session clearance | TC-12, TC-13 |
| TS-06 | Reporting | History queries and statement export | TC-17, TC-18 |
| TS-07 | Nonfunctional requirements | Performance (under 2 s for transactions, under 3 s for reads), usability, portability | TC-19, TC-20, TC-21, TC-22 |

### 5.2 Functional Testing Approach

Functional cases are designed with black-box techniques, equivalence partitioning and boundary value analysis, directly from the SRS requirements. Examples are the exact and below-minimum opening deposit, the zero-minimum account type, withdrawals on either side of UTC midnight, a repeated interest run for the same period and a transfer to the same account. Cases are executed through the HTTP/JSON API. A browser is used where the requirement is about the user interface: the deposit confirmation in TC-08, the return to sign-in in TC-03 and the CSV export in TC-18. Every SRS feature maps to at least one case in section 14.1 or to a recorded coverage gap.

### 5.3 Nonfunctional Testing Approach

Performance (NFR-1, NFR-2) is measured with TC-14, TC-19 and TC-20 on a database extended to about 3,000 accounts, using sequential requests, from submission to the persisted response. Each run records the hardware, data volume, warm-up and sample count. Usability (NFR-6) is validated by TC-21, a walkthrough with at least three first-time participants; it cannot be replaced by automated runs. Portability (NFR-9) is validated by TC-22: CI builds and tests the back end on Ubuntu and Windows, and the core and responsive browser flows run in Chrome and Firefox. Reliability (NFR-7) is covered by the error-path cases, and maintainability (NFR-8) by inspecting the CMake modules and running CTest.

### 5.4 Test Levels

- **Unit testing:** C++ functions and modules (validation, interest calculation, balance updates, sessions) in the CTest suite under `tests/`, written by the developer of each story. CI also measures line and branch coverage.
- **Integration testing:** the back-end core against SQLite and the HTTP API, through the API server tests and `scripts/run_system_validation.py`.
- **System testing:** the end-to-end cases of section 14 through the API and the browser scripts (`scripts/run_system_validation_browsers.py`, `scripts/browser-check/`).
- **Acceptance testing:** a walkthrough demonstration against the SRS with the course instructor or evaluator, following `docs/FINAL_DEMO.md`.
- **Regression testing:** every pull request runs the build and CTest on `ubuntu-latest` and `windows-latest`, the coverage job and the secret check in GitHub Actions.

## 6. Item Pass/Fail Criteria

A test case passes when every expected result in section 14.2 is observed and supported by recorded evidence, with no crash, unhandled error or unintended data change. Each case starts from its own freshly seeded database so that results do not depend on earlier cases. A case that cannot be run is reported as blocked or not run, and partial evidence as partial; neither counts as a pass. A feature passes when all of its linked cases pass.

A failed case is logged as a defect with a severity (Critical, Major or Minor) and re-tested after the fix, together with the cases its fix may affect. Numeric limits are strict: under 2 seconds for balance-changing operations and under 3 seconds for standard reads.

Testing is complete (exit criteria) when every planned case has passed or has an explicitly approved disposition recorded in `docs/DEVIATIONS.md`, all High-priority cases have passed, all CTest targets pass and CI is green. A documented deviation does not turn an unexecuted case into a pass.

Current status: the final S-43 report records 24 new fresh-seed passes, one retained pass from an earlier build (TC-15), no failures after retest and TC-21 not run (deviation 35). The exit criteria are therefore **not met**. Some passes carry recorded qualifications: TC-09 uses a prior-day date fixture rather than a live midnight rollover, TC-10 injects a failure rather than killing the process, and TC-18 verifies the generated CSV rather than the native file save.

## 7. Suspension Criteria and Resumption Requirements

Testing of dependent features is suspended when:

- a blocking defect prevents further execution (for example, a broken sign-in blocks every role-scoped case);
- the build fails, or the test environment or database is unavailable, corrupted or not isolated;
- a run risks exposing credentials or real personal data.

Testing resumes when the blocking defect is fixed and verified, the environment is restored and the database is re-seeded. Affected cases are re-run from their preconditions, never from the middle of an earlier run.

## 8. Test Deliverables

- This Test Plan: `docs/TEST_PLAN_CONSOLIDATED.md` and its PDF export `docs/Test_Plan_BankManagementSystem.pdf`.
- Test execution log of the v1.0 run: `docs/Test_Execution_Log.md`.
- System Validation Report with case-level evidence: `docs/System_Validation_Report.md` and `docs/reports/system-validation/`.
- Test Summary Report: `docs/Test_Summary_Report.md`.
- Unit Test, Code Coverage and Branch Coverage reports in `docs/`.
- Usability walkthrough record: `docs/USABILITY_WALKTHROUGH.md` (protocol; the study was not run).
- Defect and deviation records: GitHub issues and `docs/DEVIATIONS.md`.
- Automated test code and runners: `tests/`, `scripts/run_system_validation.py`, `scripts/run_system_validation_browsers.py`, `scripts/perf/` and `scripts/browser-check/`.

## 9. Environmental Needs

**Hardware:** a standard development laptop; 16 GB of RAM is recommended, and parallel build jobs shall be capped as described in the README.

**Software:** CMake 3.24 or newer and a C++17 compiler (GCC 9+ or Clang 10+ on Linux, Visual Studio 2019+ on Windows). CMake downloads the pinned dependencies (cpp-httplib, nlohmann/json, SQLite and libsodium); OpenSSL 3 is needed for the TLS build. Testing also needs the `sqlite3` command-line tool, Python 3 for the validation and performance scripts, Node.js with puppeteer-core for the browser scripts, current Chrome and Firefox, Git, and GitHub Actions (`ubuntu-latest`, `windows-latest`).

**Network:** the server listens on loopback (`127.0.0.1:8080`) by default. TC-15 uses a TLS-enabled build started with a test certificate and key (`BMS_TLS_CERT`, `BMS_TLS_KEY`).

**Test data:** each case uses a fresh database created from `db/schema.sql` and `db/seed.sql`, plus the fixtures stated in its preconditions. The performance cases add about 3,000 generated accounts with `scripts/perf/seed.py`. All data is synthetic, and evidence shall not contain passwords, tokens, private keys or database files.

## 10. Responsibilities

| Name | Responsibility |
|---|---|
| Abhishek Hanamant Kinagi | Test planning (v1.0 to v1.2), back-end and core test cases, security validation |
| Adithya Ajeeth | Front-end and user-interface test cases, usability walkthrough |
| Aditya M | Transaction and fund-transfer test cases, defect tracking |
| Aditya Vats | Account and customer management test cases, test environment and CI, test plan v1.3 |
| Course instructor or evaluator | Acceptance review of the demonstration against the SRS |

## 11. Schedule

| Activity | Dates | Status |
|---|---|---|
| Test plan v1.0 and test case design | September 2026 | Done |
| Sprint 0: project skeleton, database and CI | 12-16 Oct 2026 | Done |
| Sprint 1: unit tests with each feature story | 19-23 Oct 2026 | Done |
| Sprint 2: features, NFR verification; v1.0 cases executed (S-29) | 26-30 Oct 2026 | Done (execution 30 Oct) |
| Test plan v1.1 consolidation | 1 Nov 2026 | Done |
| Sprint 3: test plan v1.2, system validation (S-43) and test summary (S-45) | 2-7 Nov 2026 | Done; exit criteria not met (TC-21) |
| Test plan v1.3 (IEEE 829 sections restored) | 7 Nov 2026 | Document update; no execution |
| TC-21 study and dedicated cases for FR-15 to FR-17 and SEC-3 to SEC-6 | Not scheduled | Open |

## 12. Risks and Contingencies

| Risk | Contingency |
|---|---|
| Limited time because of academic deadlines | Run High-priority cases (authentication, transactions, transfers) first. |
| Integration problems between the C++ core, SQLite and the API | Build and test every pull request in CI; run integration tests early rather than at the end. |
| Team member unavailability | Shared ownership of the case set and familiarity across modules. |
| Requirements or implementation change after the cases are written | Reconcile with the SRS and `docs/DEVIATIONS.md` and issue a new plan version before claiming coverage. |
| First-time participants unavailable for TC-21 | Report TC-21 as not run and NFR-6 as not validated; do not substitute automated runs. This risk occurred (deviation 35). |
| Performance results not representative | Record hardware, data volume, load and measurement boundary; repeat under the stated conditions. |
| Windows toolchain differences | Build and test on `windows-latest` in CI for every pull request. |
| Real data or secrets in test evidence | Use synthetic data only; CI runs `scripts/check-secrets.sh`. |

## 13. Approvals

This Test Plan is approved for execution by the project team and reviewed by the course instructor or evaluator. No approval of v1.3 is recorded yet.

| Role | Name | Signature | Date |
|---|---|---|---|
| Team Lead | | | |
| Instructor / Evaluator | | | |

<!-- pagebreak -->

## 14. Test Cases

Twenty-six test cases are defined: TC-01 to TC-22 and TC-02B, TC-04B, TC-09B and TC-10B. They cover the functional requirements and the nonfunctional requirements for performance, atomicity, security, usability, reliability and portability.

### 14.1 Requirement Traceability Summary

The status column repeats SRS v1.2 Appendix E, which is based on the final S-43 report. "Not assessed" means that S-43 gave no separate verdict for that requirement.

| Requirement | Summary | Test cases | S-43 status and coverage notes |
|---|---|---|---|
| FR-1 | Sign-in, role routing, back-end authorization | TC-01, TC-15 | Validated; TC-15 retained from an earlier build |
| FR-2 | Lockout, session lifecycle and logout | TC-02, TC-02B, TC-03 | Validated; the 30-minute expiry is covered only by `auth_session_test.cpp` |
| FR-3 | Customer registration, update and deactivation | TC-04, TC-04B | Validated; search and update rely on the S-13 procedure and `customer_test.cpp` |
| FR-4 | Account opening and initial deposit | TC-05 | Validated |
| FR-5 | Freeze, unfreeze and close | TC-07 | Validated |
| FR-6 | Deposit with confirmation | TC-08 | Validated |
| FR-7 | Withdrawal minimum and UTC daily limit | TC-06, TC-09 | Validated; UTC boundary checked with a date fixture |
| FR-8 | Atomic transfer and invalid transfers | TC-10, TC-10B | Validated; rollback checked by an injected failure |
| FR-9 | Balance and account list | TC-11, TC-19 | Validated |
| FR-10 | Date-filtered, paginated history | TC-11, TC-17, TC-19 | Validated |
| FR-11 | Interest credited once per period | TC-09B | Validated; calculation basis provisional (ADR 003) |
| FR-12 | Statements and CSV export | TC-18 | Validated; native file save not observed |
| FR-13 | Staff management and unlock | TC-02B, TC-12, TC-13 | Validated |
| FR-14 | Role-specific dashboards | TC-14 | Validated |
| FR-15 | Account-type configuration | None; `account_type_test.cpp`, S-14 procedure | Not validated: no dedicated case (gap) |
| FR-16 | Locked-user list and unlock | TC-02B; `auth_lockout_test.cpp` | Not validated: listing and non-Administrator denial not in a case (gap) |
| FR-17 | Administrator audit viewer | TC-16 (partial); `audit_viewer_test.cpp` | Not validated: filters, pagination and role denial not in a case (gap) |
| NFR-1 | Transactions under 2 seconds | TC-20 | Validated under measured sequential load |
| NFR-2 | Dashboards, lists and history under 3 seconds | TC-14, TC-19 | Validated under measured load (API timing) |
| NFR-3 | Financial atomicity | TC-10, TC-10B; CTest | Validated |
| NFR-4 | Argon2id hashing, no plaintext logging, authorization | TC-01, TC-12, TC-15; `log_secrets_test.cpp` | Validated |
| NFR-5 | Append-only financial audit | TC-16 | Validated |
| NFR-6 | Usability for first-time users | TC-21 | Not validated: TC-21 not run (deviation 35) |
| NFR-7 | Input validation and error handling | TC-02, TC-06, TC-09, TC-10B, TC-11 | Validated |
| NFR-8 | Modular architecture | CMake build, CTest | Validated |
| NFR-9 | Portability | TC-22 | Validated within the recorded scope |
| SEC-1 | Authentication, role and ownership checks | TC-03, TC-11, TC-13; `rbac_test.cpp` | Validated |
| SEC-2 | Argon2id and credential handling | TC-01, TC-12, TC-15 | Validated |
| SEC-3 | HTTPS for network deployments | TC-15; `tls_test.cpp` | Not assessed |
| SEC-4 | Append-only audit, Administrator-only viewer | TC-16; `audit_viewer_test.cpp` | Not assessed |
| SEC-5 | Lockout and Administrator-only unlock | TC-02, TC-02B | Not assessed; non-Administrator unlock not in a case |
| SEC-6 | Session token, expiry and logout | TC-03, TC-13; `auth_session_test.cpp` | Not assessed; expiry and restart not in a case |

<!-- pagebreak -->

### 14.2 Detailed Test Cases

Each case states its requirements, suite, type and priority, its preconditions and numbered steps, and the expected result. Fixture values in the preconditions are set up explicitly before the run; they are not all present in the seed data.

#### TC-01: Valid User Authentication

**Requirements:** FR-1, SEC-2, NFR-4; **Story:** S-07; **Suite:** TS-01; **Type:** Functional (Security); **Priority:** High

**Objective:** Verify that a registered, active user can sign in with valid credentials.

**Preconditions:** A registered, active user exists in the SQLite `users` table.

**Steps:**

1. Send `POST /auth/login` with the user's `username` and correct password.
2. Inspect the response body and the user's `failed_attempts` value.

**Expected result:** HTTP 200 OK; the response contains a 64-character session token and the user's role; the password is never returned; the failed-attempt counter is reset to 0.

#### TC-02: Account Lockout after Three Consecutive Failed Logins

**Requirements:** FR-2, SEC-5, NFR-7; **Story:** S-09; **Suite:** TS-01; **Type:** Functional (Security); **Priority:** High

**Objective:** Verify that a user is locked after three consecutive failed sign-in attempts.

**Preconditions:** An active user exists with `failed_attempts = 0` and `status = 'active'`.

**Steps:**

1. Send `POST /auth/login` twice with an incorrect password.
2. Send a third `POST /auth/login` with an incorrect password.
3. Inspect the user's row in the `users` table.

**Expected result:** The first and second attempts return HTTP 401 with the generic message "Invalid user ID or password". The third attempt returns HTTP 401 with code `ACCOUNT_LOCKED` and a directive to contact an Administrator. In the database, `users.status = 'locked'` and `users.failed_attempts = 3`.

#### TC-02B: Administrative Account Unlock

**Requirements:** FR-13, FR-16, SEC-5; **Story:** S-09; **Suite:** TS-01; **Type:** Functional (Security); **Priority:** High

**Objective:** Verify that an Administrator can unlock a locked user.

**Preconditions:** A user is locked (`status = 'locked'`). An Administrator is signed in with a valid bearer token.

**Steps:**

1. As the Administrator, send `PATCH /users/{id}/unlock` for the locked user.
2. Inspect the user's row in the `users` table.
3. Sign in as the unlocked user with the correct password.

**Expected result:** HTTP 200 OK; the database shows `users.status = 'active'` and `users.failed_attempts = 0`; the user can sign in again immediately with the correct password.

#### TC-03: Session Expiry and Logout

**Requirements:** FR-2, SEC-1, SEC-6; **Story:** S-08; **Suite:** TS-01; **Type:** Functional (Security); **Priority:** High

**Objective:** Verify that logging out invalidates the session token.

**Preconditions:** A user is signed in with an active session token.

**Steps:**

1. Send `POST /auth/logout` with the token.
2. Send a protected request with the same token.
3. In the browser, log out and observe the page shown.

**Expected result:** Logout returns HTTP 204 No Content; the server removes the token from its in-memory session store; the later request receives HTTP 401 Unauthorized; the user interface returns to the sign-in screen.

#### TC-04: Customer Registration Validation

**Requirements:** FR-3, NFR-7; **Suite:** TS-02; **Type:** Functional; **Priority:** High

**Objective:** Verify that registration rejects a missing mandatory field and accepts a complete record.

**Preconditions:** A Bank Officer is signed in; the customer registration form is available.

**Steps:**

1. Submit a customer record without a phone number.
2. Resubmit the record with all mandatory fields valid.

**Expected result:** The incomplete request is rejected with a field-level validation error; the corrected request creates a customer and assigns a unique customer identifier.

#### TC-04B: Administrator-Only Customer Deactivation

**Requirements:** FR-3, SEC-1, BR-1, BR-6; **Story:** S-13; **Suite:** TS-02; **Type:** Functional (Security); **Priority:** High

**Objective:** Verify that only an Administrator can deactivate a customer and that deactivation keeps the customer's records.

**Preconditions:** An active customer exists; Administrator, Bank Officer and Customer sessions are available.

**Steps:**

1. As the Administrator, send `PUT /customers/{customerId}` with exactly `{"status":"inactive"}`.
2. Repeat the request with the Bank Officer and Customer tokens.
3. Repeat the Administrator request.
4. Attempt a sign-in and a new account opening for the deactivated customer, and use the customer's earlier session token.

**Expected result:** The Administrator receives HTTP 200 and the customer becomes inactive; the Bank Officer and Customer receive HTTP 403; repeated deactivation is idempotent. Customer sign-in and new account opening are refused, existing account and audit history are kept, and the customer's active sessions are invalidated.

#### TC-05: Account Opening Minimum Balance

**Requirements:** FR-4, BR-4, BR-10; **Story:** S-15; **Suite:** TS-02; **Type:** Functional; **Priority:** High

**Objective:** Verify the minimum opening deposit, including the zero-minimum account type.

**Preconditions:** An active customer exists; one account type has a positive minimum balance and another has a zero minimum.

**Steps:**

1. Send `POST /accounts` with an initial deposit below the positive minimum.
2. Send `POST /accounts` with the exact minimum.
3. Send `POST /accounts` for the zero-minimum type with `initialDeposit = "0.00"`.

**Expected result:** The below-minimum request returns HTTP 422 with `BELOW_MINIMUM_BALANCE` and creates no rows. The exact-minimum and zero-minimum requests return HTTP 201, create unique active accounts and record the opening deposit as the first transaction. A zero opening amount is permitted only for a zero-minimum type.

#### TC-06: Withdrawal Minimum Balance and Daily Limit

**Requirements:** FR-7, NFR-7, BR-4; **Suite:** TS-02; **Type:** Functional; **Priority:** High

**Objective:** Verify that withdrawals breaching the minimum balance or the daily limit are refused.

**Preconditions:** An active account has a balance above its configured minimum; a daily withdrawal limit is configured.

**Steps:**

1. Attempt a withdrawal that would take the balance below the minimum.
2. Attempt a withdrawal that exceeds the daily limit.
3. Make a valid withdrawal.

**Expected result:** The first two requests are rejected with specific errors and no balance change; the valid request succeeds and records the debit.

#### TC-07: Freeze and Close Account

**Requirements:** FR-5, BR-5; **Story:** S-16; **Suite:** TS-02; **Type:** Functional; **Priority:** High

**Objective:** Verify the account status transitions and the restrictions on frozen and closed accounts.

**Preconditions:** An active account exists; a Bank Officer or Administrator is signed in. A zero-balance account is used for closure.

**Steps:**

1. Send `PATCH /accounts/{accountNumber}/status` with `{"action":"freeze"}`, then `{"action":"unfreeze"}`.
2. Send `{"action":"close"}` for an account with a non-zero balance, then for the zero-balance account (active or frozen).
3. Attempt a deposit, withdrawal and transfer against a frozen and a closed account, and view their records.
4. Repeat the current action on an account.

**Expected result:** Each valid transition returns HTTP 200 and the requested status. Closing a non-zero account returns HTTP 409 with `ACCOUNT_BALANCE_NONZERO`; closing a zero-balance active or frozen account succeeds. Deposits, withdrawals and transfers against frozen or closed accounts return HTTP 422, and the account and audit data remain viewable. Repeating the current action is idempotent.

#### TC-08: Deposit Funds with Confirmation Dialog

**Requirements:** FR-6 (SRS 3.1); **Story:** S-17; **Suite:** TS-03; **Type:** Functional (UI); **Priority:** High

**Objective:** Verify that a deposit is confirmed before it is applied and shows the resulting balance.

**Preconditions:** Account `ACC001` is active with a balance of INR 10,000.00.

**Steps:**

1. As a Bank Officer, start a cash deposit of INR 5,000.00 into `ACC001`.
2. Check the confirmation dialog.
3. Confirm the deposit.

**Expected result:** The user interface shows a confirmation with the account number (`ACC001`) and the deposit amount (INR 5,000.00). After confirmation, the ledger updates atomically and the success view shows the resulting balance of INR 15,000.00.

#### TC-09: Daily Withdrawal Limit Check

**Requirements:** FR-7, BR-9, NFR-7; **Story:** S-18; **Suite:** TS-03; **Type:** Functional; **Priority:** High

**Objective:** Verify the daily withdrawal limit and its reset at UTC midnight.

**Preconditions:** A savings account has a balance of INR 50,000.00; the daily withdrawal limit is INR 25,000.00; stored timestamps are UTC.

**Steps:**

1. Attempt a withdrawal of INR 30,000.00 within one UTC calendar day.
2. After the UTC date changes, make a valid withdrawal.

**Expected result:** Step 1 returns HTTP 422 with `DAILY_LIMIT_EXCEEDED`, the balance is unchanged and the message states the INR 25,000 limit. Withdrawals before and after UTC midnight count towards separate daily totals; local-time midnight is not used.

#### TC-09B: Credit Account Interest Once per Period

**Requirements:** FR-11, BR-8; **Story:** S-22; **Suite:** TS-03; **Type:** Functional; **Priority:** Medium

**Objective:** Verify that interest is credited only once for a period.

**Preconditions:** An Administrator is signed in; an eligible interest-bearing account has not yet been credited for the selected period.

**Steps:**

1. Run interest crediting for the period.
2. Repeat the same run.

**Expected result:** The first run credits the calculated interest and records one interest transaction; the repeated run is rejected and does not change the balance.

#### TC-10: Atomic Transfer Between Accounts

**Requirements:** FR-8, NFR-3, BR-7; **Story:** S-19; **Suite:** TS-03; **Type:** Functional (Integrity); **Priority:** High

**Objective:** Verify that a transfer updates both accounts together or not at all.

**Preconditions:** Account A has INR 10,000.00 and account B has INR 5,000.00. Transfers run inside one `BEGIN IMMEDIATE` database transaction.

**Steps:**

1. Transfer INR 3,000.00 from A to B.
2. Check both balances and the transaction entries.
3. Repeat a transfer with a failure forced after the debit is written, and check both balances again.

**Expected result:** Account A has INR 7,000.00 and account B INR 8,000.00, with two balanced transaction entries. If the transfer is interrupted, the whole transaction rolls back.

#### TC-10B: Invalid Transfer Requests

**Requirements:** FR-8, NFR-7; **Story:** S-19; **Suite:** TS-03; **Type:** Functional; **Priority:** High

**Objective:** Verify that invalid transfers are refused without side effects.

**Preconditions:** Two active accounts exist; the source has a known balance.

**Steps:**

1. Attempt a transfer to the same account.
2. Attempt a transfer exceeding the source balance.

**Expected result:** Both requests are rejected with no balance or ledger changes; valid transfers remain atomic.

#### TC-11: Customer Balance and Transaction History Access

**Requirements:** FR-9, FR-10, SEC-1, BR-3; **Stories:** S-20, S-21; **Suite:** TS-04; **Type:** Functional (Security); **Priority:** High

**Objective:** Verify that a customer can view only their own balance and history.

**Preconditions:** Customer X owns `ACC001`; another customer owns `ACC002`; both accounts have dated transactions.

**Steps:**

1. As customer X, view the balance of `ACC001`.
2. View the date-filtered history of `ACC001`.
3. Request the balance and history of `ACC002`.

**Expected result:** The customer's own balance and newest-first, date-filtered history are returned; access to `ACC002` is denied and the denied request is audited.

#### TC-12: Create Bank Officer Account

**Requirements:** FR-13, NFR-4; **Story:** S-24; **Suite:** TS-05; **Type:** Functional; **Priority:** Medium

**Objective:** Verify that an Administrator can create a Bank Officer who can then sign in.

**Preconditions:** An Administrator is signed in.

**Steps:**

1. Send `POST /staff` with `username = "officer_new"` and the required staff-creation fields.
2. Inspect the stored password value.
3. Sign in as the new officer.

**Expected result:** HTTP 201 Created; the officer is stored with an Argon2id hash; the officer can authenticate immediately.

#### TC-13: Deactivate Bank Officer and Session Invalidation

**Requirements:** FR-13, SEC-1, SEC-6, BR-6; **Story:** S-24; **Suite:** TS-05; **Type:** Functional (Security); **Priority:** Medium

**Objective:** Verify that deactivating a Bank Officer ends their sessions and keeps their history.

**Preconditions:** The officer has an active session token.

**Steps:**

1. As the Administrator, send `PATCH /staff/{officerUserId}` with `{"status":"inactive"}`.
2. Send a request with the officer's existing token.
3. View a transaction the officer processed earlier.
4. Send the same `PATCH` request with a non-Administrator token.

**Expected result:** HTTP 200 OK and `users.status = 'inactive'`; every active token of the officer is rejected on later requests; the officer ID is kept on historical transaction audit entries. The endpoint is Administrator-only.

#### TC-14: Role-Specific Dashboards

**Requirements:** FR-14, NFR-2; **Story:** S-25; **Suite:** TS-04; **Type:** Functional and Performance; **Priority:** Medium

**Objective:** Verify that each role receives its own dashboard within the response-time limit.

**Preconditions:** Sessions are available for an Administrator, a Bank Officer and a Customer.

**Steps:**

1. Send `GET /dashboard` as each role.
2. Measure each response time.

**Expected result:** The Administrator receives system-wide counts (customers, accounts, staff) and today's volume; the Bank Officer receives the transactions they processed today; the Customer receives the list of their own accounts and balances. Each response arrives in under 3.0 seconds (NFR-2).

#### TC-15: Secure HTTPS / TLS Configuration

**Requirements:** SEC-2, SEC-3, FR-1, NFR-4; **Story:** S-28; **Suite:** TS-04; **Type:** Nonfunctional (Security); **Priority:** High

**Objective:** Verify that a TLS-enabled server encrypts traffic and refuses plain HTTP.

**Preconditions:** A TLS-enabled build is started with a test certificate and private key, for example on port 8443.

**Steps:**

1. Open an HTTPS connection with TLS 1.3.
2. Send a plain HTTP request to the TLS port.
3. Sign in over HTTPS.

**Expected result:** The TLS handshake succeeds; plain HTTP requests to the secure port are rejected or upgraded; all payload traffic is encrypted in transit.

#### TC-16: Append-Only Audit Trail

**Requirements:** NFR-5, SEC-4, FR-17 (partial); **Story:** S-10; **Suite:** TS-04; **Type:** Nonfunctional (Security and Audit); **Priority:** High

**Objective:** Verify that every balance change is audited and that audit entries cannot be changed.

**Preconditions:** A deposit, a withdrawal and a transfer have been completed.

**Steps:**

1. As the Administrator, inspect the audit records of the three operations.
2. Attempt to update or delete an audit entry through the application.

**Expected result:** Each balance-changing operation has an audit entry with timestamp, affected entity, amount and operator; update and delete attempts are rejected.

#### TC-17: Date-Filtered Transaction History

**Requirements:** FR-10; **Story:** S-21; **Suite:** TS-06; **Type:** Functional; **Priority:** High

**Objective:** Verify the date filter, ordering and paging of transaction history.

**Preconditions:** A signed-in user may view an account's transaction history.

**Steps:**

1. Request the history with inclusive `from` and `to` dates.
2. Page through the results and check their order.

**Expected result:** Only transactions within the date range are returned, newest first, with signed amounts and no duplicate records across pages.

#### TC-18: Account Statement Generation and Export

**Requirements:** FR-12; **Story:** S-23; **Suite:** TS-06; **Type:** Functional; **Priority:** Medium

**Objective:** Verify that a statement for a date range is generated and can be exported.

**Preconditions:** A customer account has 10 or more transactions over the past 60 days.

**Steps:**

1. Send `GET /accounts/{id}/statement?from=2026-09-01&to=2026-10-18`.
2. Export the statement from the user interface.

**Expected result:** HTTP 200 OK; a chronologically ordered statement is returned with opening balance, closing balance, debit and credit entries, and a formatted export.

#### TC-19: Account List and History Read Performance

**Requirements:** NFR-2, FR-9, FR-10; **Story:** S-25; **Suite:** TS-07; **Type:** Nonfunctional (Performance); **Priority:** Medium

**Objective:** Verify that the account list and history load in under 3 seconds with representative data.

**Preconditions:** The database contains representative account and transaction data (about 3,000 accounts); signed-in sessions can access permitted accounts.

**Steps:**

1. Measure `GET /dashboard` for a Customer's account list.
2. Measure `GET /accounts/{accountNumber}/transactions` for a permitted, date-filtered history.

**Expected result:** Each response returns only authorized data, keeps the documented order and filters, and completes in under 3 seconds. Role-specific dashboard correctness is covered once, by TC-14.

#### TC-20: Transaction Performance Target Verification

**Requirements:** NFR-1; **Story:** S-30; **Suite:** TS-07; **Type:** Nonfunctional (Performance); **Priority:** Medium

**Objective:** Verify that balance-changing transactions complete in under 2 seconds.

**Preconditions:** The database holds about 3,000 accounts; the system is under normal simulated load.

**Steps:**

1. Run deposit, withdrawal and transfer operations.
2. Measure the time from submission to the persisted success response.

**Expected result:** Each balance-changing transaction completes within 2 seconds, including database persistence.

#### TC-21: First-Time User Usability Walkthrough

**Requirements:** NFR-6; **Story:** S-32; **Suite:** TS-07; **Type:** Nonfunctional (Usability); **Priority:** Medium

**Objective:** Verify that first-time users complete core tasks unaided.

**Preconditions:** Three first-time participants without prior training are available; the protocol in `docs/USABILITY_WALKTHROUGH.md` is followed.

**Steps:**

1. Each participant performs a Bank Officer deposit without help.
2. Each participant performs a Customer history lookup without help.
3. Record task outcomes and observations.

**Expected result:** 100% task completion on the first attempt without verbal prompts; findings are documented in `docs/USABILITY_WALKTHROUGH.md`.

#### TC-22: Cross-Platform and Cross-Browser Portability

**Requirements:** NFR-9; **Story:** S-31; **Suite:** TS-07; **Type:** Nonfunctional (Portability); **Priority:** Medium

**Objective:** Verify that the system runs on Linux and Windows and in Chrome and Firefox.

**Preconditions:** Builds are produced on Linux (GCC) and Windows (MSVC).

**Steps:**

1. Run the back end on Ubuntu 24.04 and Windows 11.
2. Use the web front end in Google Chrome and Mozilla Firefox, including the responsive layouts.

**Expected result:** Both builds run without platform-specific errors; the user interface renders identically, with its responsive glassmorphism layout, in Chrome and Firefox.
