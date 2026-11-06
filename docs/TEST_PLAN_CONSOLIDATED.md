# Test Plan & Consolidated Test Cases (v1.2)

- **Document ID:** TP-BMS-2026-v1.2
- **Course:** `UE24CS341A` (Software Engineering and System Design)
- **Story:** [S-41] Update the test cases and Test Plan for sprint changes (#117)
- **Status:** Issued for Sprint 3 execution; supersedes the standalone test-case and test-plan files
- **Prepared by:** QA Team, Abhishek Kinagi
- **Related source documents:** The standalone DOCX/PDF files were superseded and removed in v1.2; their coverage is consolidated here.
- **Governing SRS Reference:** SRS v1.1 (Dated **02/09/2026**; corrected from erroneous 11/09/2026 citation)

---

## 1. Revision History & Remediation Log

| Version | Date | Author | Description |
| :---: | :---: | :---: | :--- |
| **1.0** | 15 Sep 2026 | QA Team | Initial draft with split standalone test suites. |
| **1.1** | 1 Nov 2026 | Abhishek Kinagi & Team | **Consolidation & Gap Resolution (S-35):**<br>1. Merged standalone test suites into unified Section 14 with continuous TC IDs.<br>2. Retained the legacy source documents as supporting references.<br>3. Corrected SRS citation date from 11/09/2026 to 02/09/2026.<br>4. Added test cases for FR-12 (statements), FR-14 (dashboards), FR-7 (daily limit), admin unlock (TC-02B), NFR-2 (performance), NFR-6 (usability), and NFR-9 (portability).<br>5. Refined TC-15 wording regarding HTTPS TLS handshake.<br>6. Preserved the canonical TC-11 balance/history ownership mapping.<br>7. Built 100% complete Requirements Traceability Matrix. |
| **1.2** | 4 Nov 2026 | Abhishek Kinagi & Team | **Sprint-change alignment (S-41):**<br>1. Rechecked every case against the merged implementation and deviations log.<br>2. Added explicit zero-opening-deposit coverage, request bodies/status codes for freeze/close and customer deactivation, and UTC daily-limit reset behavior.<br>3. Resolved the duplicate dashboard coverage by making TC-19 the account-list/history performance case.<br>4. Reissued this document as an IEEE-style PDF and superseded the standalone DOCX/PDF artifacts.<br>5. Updated the RTM and security suite classification.<br>6. Corrected TC-03's logout success expectation to HTTP 204 to match the implemented endpoint; the subsequent request with the invalidated token must still return HTTP 401. |

---

## 2. Section 5.1: Test Suite Classification and Security Validation (Updated)

| Suite ID | Suite Name | Description | Included Test Cases |
| :---: | :--- | :--- | :--- |
| **TS-01** | **Authentication & Lockout** | Credential checks, session creation, lockout after 3 fails, admin unlock | TC-01, TC-02, TC-02B, TC-03 |
| **TS-02** | **Customer & Account Ops** | Customer onboarding, Administrator-only customer deactivation, account opening, status management | TC-04, TC-04B, TC-05, TC-06, TC-07 |
| **TS-03** | **Financial Transactions** | Deposits, withdrawals, daily limits, atomic transfers | TC-08, TC-09, TC-09B, TC-10, TC-10B |
| **TS-04** | **Security & Access Control** | Least privilege, role barriers, HTTPS, audit trail | TC-11, TC-14, TC-15, TC-16 |
| **TS-05** | **Staff & Administration** | Bank Officer CRUD, deactivation session clearance | TC-12, TC-13 |
| **TS-06** | **Reporting & Analytics** | History queries, statements export, role dashboards | TC-17, TC-18 |
| **TS-07** | **Non-Functional Requirements**| Performance (<2s transactions, <3s reads), usability walkthrough, cross-platform | TC-19, TC-20, TC-21, TC-22 |

*Security validation in this section covers authentication, lockout, RBAC, session invalidation, encrypted transport, and audit immutability through TS-01 and TS-04. **TC-11** remains the primary ownership and date-filter access case.*

---

## 3. Section 14: Consolidated Master Test Cases

### TC-01: Valid User Authentication (FR-1, SEC-2, S-07)
- **Preconditions:** Registered active user exists in SQLite `users` table.
- **Input:** `POST /auth/login` with valid `user_id` and correct password.
- **Expected Result:** HTTP 200 OK; returns 64-character session token and user role; password is never returned; failed attempt counter reset to 0.

### TC-02: Account Lockout after Three Consecutive Failed Logins (FR-2, S-09)
- **Preconditions:** Active user exists with `failed_attempts = 0` and `status = 'active'`.
- **Input:** 3 consecutive `POST /auth/login` requests with incorrect passwords.
- **Expected Result:**
  - 1st & 2nd attempts: HTTP 401 with generic message "Invalid user ID or password".
  - 3rd attempt: HTTP 401 with code `ACCOUNT_LOCKED` and directive to contact an Administrator.
  - In database: `users.status = 'locked'` and `users.failed_attempts = 3`.

### TC-02B: Administrative Account Unlock (FR-13, S-09) *(NEW)*
- **Preconditions:** User is locked (`status = 'locked'`). Administrator is logged in with valid bearer token.
- **Input:** `PATCH /users/{id}/unlock` issued by Administrator.
- **Expected Result:** HTTP 200 OK; database updates `users.status = 'active'` and `users.failed_attempts = 0`; user can immediately log in again with correct password.

### TC-03: Session Expiry and Logout (FR-2, SEC-1, S-08)
- **Preconditions:** User is logged in with active session token.
- **Input:** User sends `POST /auth/logout`; subsequent request sent with invalidated token.
- **Expected Result:** Logout returns HTTP 204 No Content; the server removes the token from its in-memory session store; subsequent request receives HTTP 401 Unauthorized; UI redirects to login screen.

### TC-04: Customer Registration Validation (FR-3)
- **Preconditions:** Bank Officer is authenticated; customer registration form is available.
- **Input:** Submit a customer record without a phone number, then resubmit with all mandatory fields valid.
- **Expected Result:** The incomplete request is rejected with a field-level validation error; the corrected request creates a customer and assigns a unique customer identifier.

### TC-04B: Administrator-Only Customer Deactivation (FR-3, SEC-1, S-13)
- **Preconditions:** An active customer exists; Administrator, Bank Officer, and Customer sessions are available.
- **Input:** Administrator sends `PUT /customers/{customerId}` with exactly `{"status":"inactive"}`; repeat with the Officer and Customer tokens.
- **Expected Result:** Administrator receives HTTP 200 and the customer becomes inactive; Officer and Customer receive HTTP 403; repeated deactivation is idempotent; customer login and new account opening are refused, existing account and audit history remain retained, and active customer sessions are invalidated.

### TC-05: Account Opening Minimum Balance (FR-4, S-15)
- **Preconditions:** Active customer exists; one account type has a positive minimum and another has a zero minimum.
- **Input:** `POST /accounts` with an initial deposit below the positive minimum, then with the exact minimum; repeat for the zero-minimum type using `initialDeposit = "0.00"`.
- **Expected Result:** The below-minimum request returns HTTP 422 with `BELOW_MINIMUM_BALANCE` and creates no rows. Exact-minimum and zero-minimum requests return HTTP 201, create unique active accounts, and record the opening deposit as the first transaction; the zero opening amount is permitted only for a zero-minimum type.

### TC-06: Withdrawal Minimum Balance and Daily Limit (FR-7)
- **Preconditions:** Active account has a balance above its configured minimum; a daily withdrawal limit is configured.
- **Input:** Attempt a withdrawal that breaches the minimum balance, one that exceeds the daily limit, then a valid withdrawal.
- **Expected Result:** The first two requests are rejected with specific errors and no balance change; the valid request succeeds and records the debit.

### TC-07: Freeze and Close Account (FR-5, S-16)
- **Preconditions:** Active account exists; Bank Officer or Administrator is authenticated. Use a zero-balance account for closure.
- **Input:** Send `PATCH /accounts/{accountNumber}/status` with `{"action":"freeze"}`, then `{"action":"unfreeze"}`, then `{"action":"close"}`.
- **Expected Result:** Each valid transition returns HTTP 200 and the requested status. Closing a non-zero account returns HTTP 409 with `ACCOUNT_BALANCE_NONZERO`; closing a zero-balance active or frozen account succeeds. Deposits, withdrawals, and transfers against frozen/closed accounts return HTTP 422; retained account and audit data remain viewable. Repeating the current action is idempotent.

### TC-08: Deposit Funds with Confirmation Dialog (FR-6, SRS 3.1, S-17)
- **Preconditions:** Account `ACC001` active with balance ₹10,000.
- **Input:** Bank Officer initiates ₹5,000 cash deposit.
- **Expected Result:** UI renders modal confirmation showing account number (`ACC001`) and deposit amount (`₹5,000.00`); upon confirmation, ledger updates atomically and the post-success view shows the resulting balance (`₹15,000.00`).

### TC-09: Daily Withdrawal Limit Check (FR-7, S-18) *(NEW)*
- **Preconditions:** Savings account with balance ₹50,000; daily withdrawal limit is ₹25,000; stored timestamps are UTC.
- **Input:** Attempt withdrawal of ₹30,000 in one UTC calendar day, then repeat a valid withdrawal after the UTC date changes.
- **Expected Result:** HTTP 422 with `DAILY_LIMIT_EXCEEDED`; balance remains unchanged; message states the ₹25,000 limit. Withdrawals before and after the UTC midnight boundary are counted in separate daily totals; local-time midnight is not used.

### TC-09B: Credit Account Interest Once per Period (FR-11, S-22)
- **Preconditions:** Administrator is authenticated; an eligible interest-bearing account has not yet been credited for the selected period.
- **Input:** Run interest crediting for the period, then repeat the same run.
- **Expected Result:** The first run credits the calculated interest and records one interest transaction; the duplicate run is rejected and does not change the balance.

### TC-10: Atomic Transfer Between Accounts (FR-8, S-19)
- **Preconditions:** Account A has ₹10,000, Account B has ₹5,000.
- **Input:** Transfer ₹3,000 from A to B within `BEGIN IMMEDIATE` transaction.
- **Expected Result:** Account A balance = ₹7,000, Account B balance = ₹8,000; two balanced transaction entries written; if server interrupts during transfer, entire transaction rolls back.

### TC-10B: Invalid Transfer Requests (FR-8, S-19)
- **Preconditions:** Two active accounts exist; source has a known balance.
- **Input:** Attempt a transfer to the same account and a transfer exceeding the source balance.
- **Expected Result:** Both requests are rejected with no balance or ledger changes; valid transfers remain atomic.

### TC-11: Customer Balance and Transaction History Access (FR-9, FR-10, S-20, S-21)
- **Preconditions:** Customer X owns `ACC001`; another customer owns `ACC002`; both accounts have dated transactions.
- **Input:** Customer X views the balance and date-filtered history for `ACC001`, then requests `ACC002`.
- **Expected Result:** Own balance and reverse-chronological, date-filtered history are returned; access to `ACC002` is denied and the denied request is audited.

### TC-12: Create Bank Officer Account (FR-13, S-24)
- **Preconditions:** Administrator logged in.
- **Input:** `POST /staff` with `username = "officer_new"` and the required staff-creation fields.
- **Expected Result:** HTTP 201 Created; officer stored with Argon2id hash; officer can immediately authenticate.

### TC-13: Deactivate Bank Officer & Session Invalidation (FR-13, S-24)
- **Preconditions:** Officer has an active session token.
- **Input:** Administrator issues `PATCH /staff/{officerUserId}` with `{"status":"inactive"}`.
- **Expected Result:** HTTP 200 OK; `users.status = 'inactive'`; all active tokens for the officer are rejected by subsequent requests; officer ID is retained on historical transaction audit entries. The endpoint is Administrator-only.

### TC-14: Role-Specific Dashboards (FR-14, NFR-2, S-25) *(NEW)*
- **Preconditions:** Authenticated as Administrator, Bank Officer, or Customer.
- **Input:** `GET /dashboard`.
- **Expected Result:**
  - Administrator receives system-wide counts (customers, accounts, staff) and today's volume.
  - Bank Officer receives transactions they processed today.
  - Customer receives list of their owned accounts and balances.
  - Endpoint responds in < 3.0 seconds (satisfying NFR-2).

### TC-15: Secure HTTPS / TLS Configuration (SEC-2, S-28)
- **Preconditions:** TLS certificate and private key configured on server.
- **Input:** Client initiates HTTPS connection with TLS 1.3 over port 8443.
- **Expected Result:** Successful TLS handshake; plain HTTP requests to secure port are rejected or upgraded; all payload traffic encrypted in transit.

### TC-16: Append-Only Audit Trail (NFR-5, S-10)
- **Preconditions:** Deposit, withdrawal, and transfer operations have been completed.
- **Input:** Inspect their audit records and attempt to update or delete an entry through the application.
- **Expected Result:** Each balance-changing operation has an audit entry containing timestamp, affected entity, amount, and operator; update and delete attempts are rejected.

### TC-17: Date-Filtered Transaction History (FR-10, S-21)
- **Preconditions:** An authenticated user can view an account's transaction history.
- **Input:** Request history with inclusive `from` and `to` dates and inspect result ordering.
- **Expected Result:** Only transactions within the date range are returned, newest first, with signed amounts and no duplicate records across pages.

### TC-18: Account Statement Generation & Export (FR-12, S-23) *(NEW)*
- **Preconditions:** Customer account with 10+ transactions over past 60 days.
- **Input:** `GET /accounts/{id}/statement?from=2026-09-01&to=2026-10-18`.
- **Expected Result:** HTTP 200 OK; returns chronologically ordered statement with opening balance, closing balance, debit/credit entries, and formatted export.

### TC-19: Account List and History Read Performance (FR-9, FR-10, NFR-2, S-25)
- **Preconditions:** Database contains representative account and transaction data; authenticated sessions can access permitted accounts.
- **Input:** Measure `GET /dashboard` for a Customer account list and `GET /accounts/{accountNumber}/transactions` for a permitted date-filtered history.
- **Expected Result:** Each response returns only authorized data, preserves the documented ordering and filters, and completes in under 3 seconds. Role-specific dashboard correctness remains covered once, by TC-14.

### TC-20: Transaction Performance Target Verification (NFR-1, S-30) *(NEW)*
- **Preconditions:** Database populated with approximately 3,000 accounts; system is under normal simulated load.
- **Input:** Execute deposit, withdrawal, and transfer operations and measure the time through persisted success response.
- **Expected Result:** Each balance-affecting transaction completes within 2 seconds, including database persistence.

### TC-21: First-Time User Usability Walkthrough (NFR-6, S-32) *(NEW)*
- **Preconditions:** 3 first-time users without prior training.
- **Input:** Each participant performs Bank Officer deposit and Customer history lookup unaided.
- **Expected Result:** 100% task completion rate on first attempt without verbal prompts; findings documented in Usability Walkthrough Report (`docs/USABILITY_WALKTHROUGH.md`).

### TC-22: Cross-Platform & Cross-Browser Portability (NFR-9, S-31) *(NEW)*
- **Preconditions:** Builds produced on Linux (GCC) and Windows (MSVC).
- **Input:** Run backend on Ubuntu 24.04 and Windows 11; access web frontend via Google Chrome and Mozilla Firefox.
- **Expected Result:** Both OS binaries execute without platform-specific errors; UI renders identically with responsive glassmorphism across Chrome and Firefox.

---

## 4. Requirements Traceability Matrix (RTM)

| Requirement ID | Requirement Description | Verification Method | Associated Test Cases |
| :---: | :--- | :---: | :--- |
| **FR-1** | User authentication with ID & password/PIN | Automated / API | TC-01, TC-15 |
| **FR-2** | Session management, logout & 3-fail lockout | Automated / Unit | TC-02, TC-02B, TC-03 |
| **FR-3** | Register and deactivate customer record | API / Integration | TC-04, TC-04B |
| **FR-4** | Open new bank account for customer | API / Integration | TC-05 |
| **FR-5** | Freeze and close accounts | API / Functional | TC-07 |
| **FR-6** | Deposit funds with confirmation dialog | Functional / UI | **TC-08** (SRS 3.1) |
| **FR-7** | Withdraw funds with minimum balance & daily limit | Automated / API | **TC-09** |
| **FR-8** | Atomic fund transfer between accounts | Unit / Integration | TC-10 |
| **FR-9** | View account balance | API / UI | TC-11 |
| **FR-10** | View transaction history with date filters | API / UI | TC-11, TC-17 |
| **FR-11** | Credit account interest once per period | API / Integration | TC-09B |
| **FR-12** | Account statement generation & export | Automated / API | **TC-18** |
| **FR-13** | Staff management & administrative unlock | Functional / UI | **TC-02B**, TC-12, TC-13 |
| **FR-14** | Role-specific dashboards | Functional / API | **TC-14** |
| **NFR-1** | Balance-affecting transaction response time (< 2 seconds) | Performance test | TC-20 |
| **NFR-2** | Dashboard, account-list, and history response time (< 3 seconds) | Performance test | **TC-14, TC-19** |
| **NFR-3** | ACID transaction integrity | Unit test | TC-10 |
| **NFR-4** | Secure password hashing (Argon2id) | Unit / Security | TC-01 |
| **NFR-5** | Tamper-evident append-only audit trail | Security audit | TC-16 |
| **NFR-6** | Usability for first-time users | User walkthrough | **TC-21** |
| **NFR-7** | Reliability & graceful error handling | API error test | TC-02, TC-09, TC-10B, TC-11 |
| **NFR-8** | Modular architecture by functional area | Static analysis | CMake build check |
| **NFR-9** | Portability: Linux/Windows & Chrome/Firefox | Environment test | **TC-22** |
| **SEC-1** | Role-based access control & token expiry | Automated / API | TC-03, TC-11, TC-13 |
| **SEC-2** | Encrypted transport (HTTPS) & Argon2 hashing | Security audit | TC-01, **TC-15** |

---

## 5. Summary of Remediation Actions (S-41 Done Criteria)

1. **Implementation alignment:** All cases were checked against merged source, automated tests, manual-test instructions, and `docs/DEVIATIONS.md`.
2. **Sprint changes covered:** Explicit cases now cover zero opening deposits (TC-05), the freeze/close action body and conflict rules (TC-07), UTC daily-limit boundaries (TC-09), and Administrator-only customer deactivation (TC-04B).
3. **Overlap removed:** TC-14 is the sole role-dashboard behavior case; TC-19 verifies account-list/history performance.
4. **Traceability table:** Covers 100% of functional requirements (FR-1 to FR-14), non-functional requirements (NFR-1 to NFR-9), and security rules (SEC-1, SEC-2).
5. **SRS citation corrected:** Date standardized to 02/09/2026; the v1.2 PDF is the issued copy.
