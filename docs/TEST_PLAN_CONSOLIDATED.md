# Test Plan & Consolidated Test Cases (v1.1)

- **Document ID:** TP-BMS-2026-v1.1
- **Course:** `UE24CS341A` (Software Engineering and System Design)
- **Story:** [S-35] Consolidate the Test Plan and test cases (#66)
- **Supersedes:** `Test_Cases_Bank_Management_System.docx` and `Test_Plan_Test_Cases_BankManagementSystem.pdf` (merged into Section 14 to resolve conflicting numbering)
- **Governing SRS Reference:** SRS v1.1 (Dated **02/09/2026**; corrected from erroneous 11/09/2026 citation)

---

## 1. Revision History & Remediation Log

| Version | Date | Author | Description |
| :---: | :---: | :---: | :--- |
| **1.0** | 15 Sep 2026 | QA Team | Initial draft with split standalone test suites. |
| **1.1** | 18 Oct 2026 | Abhishek Kinagi & Team | **Consolidation & Gap Resolution (S-35):**<br>1. Merged standalone test suites into unified Section 14 with continuous TC IDs.<br>2. Removed redundant legacy files from `docs/`.<br>3. Corrected SRS citation date from 11/09/2026 to 02/09/2026.<br>4. Added test cases for FR-12 (statements), FR-14 (dashboards), FR-7 (daily limit), admin unlock (TC-02B), NFR-2 (performance), NFR-6 (usability), and NFR-9 (portability).<br>5. Refined TC-15 wording regarding HTTPS TLS handshake.<br>6. Updated Section 5.1 to classify TC-11 as a Security/Role-Enforcement test case.<br>7. Built 100% complete Requirements Traceability Matrix. |

---

## 2. Section 5.1: Test Suite Classification (Updated)

| Suite ID | Suite Name | Description | Included Test Cases |
| :---: | :--- | :--- | :--- |
| **TS-01** | **Authentication & Lockout** | Credential checks, session creation, lockout after 3 fails, admin unlock | TC-01, TC-02, TC-02B, TC-03 |
| **TS-02** | **Customer & Account Ops** | Customer onboarding, account opening, status management | TC-04, TC-05, TC-06, TC-07 |
| **TS-03** | **Financial Transactions** | Deposits, withdrawals, daily limits, atomic transfers | TC-08, TC-09, TC-10, TC-10B |
| **TS-04** | **Security & Access Control** | Least privilege, role barriers, HTTPS, audit trail | **TC-11**, TC-14, TC-15, TC-16 |
| **TS-05** | **Staff & Administration** | Bank Officer CRUD, deactivation session clearance | TC-12, TC-13 |
| **TS-06** | **Reporting & Analytics** | History queries, statements export, role dashboards | TC-17, TC-18, TC-19 |
| **TS-07** | **Non-Functional Requirements**| Performance (<3s load), usability walkthrough, cross-platform | TC-20, TC-21, TC-22 |

*Note: As required by S-35, **TC-11** (Role-Based Endpoint Barrier) is officially cataloged in Section 5.1 under Security Cases.*

---

## 3. Section 14: Consolidated Master Test Cases

### TC-01: Valid User Authentication (FR-1, SEC-2, S-07)
- **Preconditions:** Registered active user exists in SQLite `users` table.
- **Input:** `POST /auth/login` with valid `user_id` and correct password.
- **Expected Result:** HTTP 200 OK; returns 64-character session token and user role; password is never returned; failed attempt counter reset to 0.

### TC-02: Account Lockout after Three Consecutive Failed Logins (FR-2, S-09)
- **Preconditions:** Active user exists with `failed_attempts = 0`, `locked = 0`.
- **Input:** 3 consecutive `POST /auth/login` requests with incorrect passwords.
- **Expected Result:**
  - 1st & 2nd attempts: HTTP 401 with generic message "Invalid user ID or password".
  - 3rd attempt: HTTP 401 with code `ACCOUNT_LOCKED` and directive to contact an Administrator.
  - In database: `users.locked = 1` and `users.failed_attempts = 3`.

### TC-02B: Administrative Account Unlock (FR-13, S-09) *(NEW)*
- **Preconditions:** User is locked (`locked = 1`). Administrator is logged in with valid bearer token.
- **Input:** `PATCH /users/{id}/unlock` issued by Administrator.
- **Expected Result:** HTTP 200 OK; database updates `users.locked = 0` and `users.failed_attempts = 0`; user can immediately log in again with correct password.

### TC-03: Session Expiry and Logout (FR-2, SEC-1, S-08)
- **Preconditions:** User is logged in with active session token.
- **Input:** User sends `POST /auth/logout`; subsequent request sent with invalidated token.
- **Expected Result:** Logout returns HTTP 200; token removed from `sessions` table; subsequent request receives HTTP 401 Unauthorized; UI redirects to login screen.

### TC-07: Freeze and Close Inactive Account (FR-5, S-16)
- **Preconditions:** Customer account exists with zero balance.
- **Input:** Bank Officer requests account freeze/closure via API.
- **Expected Result:** Account `active` flag set to 0; further deposits or withdrawals on this account are rejected with HTTP 400.

### TC-08: Deposit Funds with Confirmation Dialog (FR-6, SRS 3.1, S-17)
- **Preconditions:** Account `ACC-100234` active with balance ₹10,000.
- **Input:** Bank Officer initiates ₹5,000 cash deposit.
- **Expected Result:** UI renders modal confirmation showing account number (`ACC-100234`), deposit amount (`₹5,000.00`), and resulting balance (`₹15,000.00`); upon confirmation, ledger updates atomically and balance becomes ₹15,000.

### TC-09: Daily Withdrawal Limit Check (FR-7, S-18) *(NEW)*
- **Preconditions:** Savings account with balance ₹50,000; daily withdrawal limit is ₹25,000.
- **Input:** Attempt withdrawal of ₹30,000 in a single calendar day.
- **Expected Result:** HTTP 400 Bad Request; error code `LIMIT_EXCEEDED`; balance remains unchanged; message states daily limit of ₹25,000 exceeded.

### TC-10: Atomic Transfer Between Accounts (FR-8, S-19)
- **Preconditions:** Account A has ₹10,000, Account B has ₹5,000.
- **Input:** Transfer ₹3,000 from A to B within `BEGIN IMMEDIATE` transaction.
- **Expected Result:** Account A balance = ₹7,000, Account B balance = ₹8,000; two balanced transaction entries written; if server interrupts during transfer, entire transaction rolls back.

### TC-11: Role-Based Authorization Enforcement (SEC-1, S-10)
- **Preconditions:** User logged in as Customer.
- **Input:** Customer attempts `GET /staff` or `POST /staff` (Administrator only).
- **Expected Result:** HTTP 403 Forbidden; error code `FORBIDDEN`; request blocked before handler logic executes.

### TC-12: Create Bank Officer Account (FR-13, S-24)
- **Preconditions:** Administrator logged in.
- **Input:** `POST /staff` with `id = "officer_new"`, `name = "Alice"`, `password = "SecretPass1"`.
- **Expected Result:** HTTP 201 Created; officer stored with Argon2id hash; officer can immediately authenticate.

### TC-13: Deactivate Bank Officer & Session Invalidation (FR-13, S-24)
- **Preconditions:** Officer has an active session token.
- **Input:** Administrator issues `PATCH /staff/officer_new` with `{"active": false}`.
- **Expected Result:** HTTP 200 OK; `users.active = 0`; all active tokens for `officer_new` deleted from `sessions`; next request with officer's token returns HTTP 401; officer ID retained on historical transaction audit entries.

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

### TC-18: Account Statement Generation & Export (FR-12, S-23) *(NEW)*
- **Preconditions:** Customer account with 10+ transactions over past 60 days.
- **Input:** `GET /accounts/{id}/statement?from=2026-09-01&to=2026-10-18`.
- **Expected Result:** HTTP 200 OK; returns chronologically ordered statement with opening balance, closing balance, debit/credit entries, and formatted export.

### TC-20: Performance Target Verification (NFR-2, S-30) *(NEW)*
- **Preconditions:** Database populated with 1,000 customers and 3,000 accounts.
- **Input:** Execute 100 concurrent requests against `GET /dashboard` and `GET /accounts/{id}/balance`.
- **Expected Result:** 95th percentile response time is under 1.5 seconds; 100% of requests complete in under 3.0 seconds.

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
| **FR-3** | Register new customer record | API / Integration | TC-04 |
| **FR-4** | Open new bank account for customer | API / Integration | TC-05 |
| **FR-5** | Freeze and close accounts | API / Functional | TC-07 |
| **FR-6** | Deposit funds with confirmation dialog | Functional / UI | **TC-08** (SRS 3.1) |
| **FR-7** | Withdraw funds with minimum balance & daily limit | Automated / API | **TC-09** |
| **FR-8** | Atomic fund transfer between accounts | Unit / Integration | TC-10 |
| **FR-9** | Balance-checking transaction verification | API / Unit | TC-08, TC-09, TC-10 |
| **FR-10** | View account balance | API / UI | TC-08, TC-14 |
| **FR-11** | View transaction history with date filters | API / UI | TC-17, TC-21 |
| **FR-12** | Account statement generation & export | Automated / API | **TC-18** |
| **FR-13** | Staff management & administrative unlock | Functional / UI | **TC-02B**, TC-12, TC-13 |
| **FR-14** | Role-specific dashboards | Functional / API | **TC-14** |
| **NFR-1** | Capacity (up to 3,000 accounts) | Benchmark | TC-20 |
| **NFR-2** | Response time (< 3 seconds) | Performance test | **TC-14, TC-20** |
| **NFR-3** | ACID transaction integrity | Unit test | TC-10 |
| **NFR-4** | Secure password hashing (Argon2id) | Unit / Security | TC-01 |
| **NFR-5** | Tamper-evident append-only audit trail | Security audit | TC-16 |
| **NFR-6** | Usability for first-time users | User walkthrough | **TC-21** |
| **NFR-7** | Reliability & graceful error handling | API error test | TC-02, TC-09, TC-11 |
| **NFR-8** | Modular architecture by functional area | Static analysis | CMake build check |
| **NFR-9** | Portability: Linux/Windows & Chrome/Firefox | Environment test | **TC-22** |
| **SEC-1** | Role-based access control & token expiry | Automated / API | TC-03, TC-11, TC-13 |
| **SEC-2** | Encrypted transport (HTTPS) & Argon2 hashing | Security audit | TC-01, **TC-15** |

---

## 5. Summary of Remediation Actions (S-35 Done Criteria)

1. **Documents Merged:** Standalone test documents consolidated into Section 14 above; redundant files removed.
2. **Missing Test Cases Added:** Explicit TCs written for FR-12 (TC-18), FR-14 (TC-14), FR-7 daily limit (TC-09), Admin unlock (TC-02B), NFR-2 (TC-20), NFR-6 (TC-21), and NFR-9 (TC-22).
3. **Traceability Table:** Covers 100% of functional requirements (FR-1 to FR-14), non-functional requirements (NFR-1 to NFR-9), and security rules (SEC-1, SEC-2).
4. **SRS Citation Corrected:** Date standardized to 02/09/2026.
5. **TC-15 & TC-11 Refined:** HTTPS handshake criteria clarified, and TC-11 categorized under Section 5.1 Security.
