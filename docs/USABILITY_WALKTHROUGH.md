# Usability Walkthrough Report

- **Document ID:** UWR-2026-01
- **Story:** [S-32] Run a usability walkthrough (#63)
- **Requirement:** NFR-6 (Usability – first-time users complete core tasks unaided)
- **Date of Execution:** 18 October 2026
- **Facilitator:** Abhishek Kinagi
- **Application Version:** v0.1.0 (Sprint 2 Build)
- **Environment:** Google Chrome 129 / Mozilla Firefox 131 on Windows 11 and Linux

---

## 1. Executive Summary

A formal usability walkthrough was conducted in accordance with NFR-6. Three participants who had never seen or used the Bank Management System (BMS) were selected as test subjects. Each participant was tasked with completing two core banking workflows independently and unaided:

1. **Task 1 (Bank Officer):** Process a cash deposit with the confirmation dialog check (SRS 3.1).
2. **Task 2 (Customer):** Look up account balances and review transaction history.

**Result:** All three participants (**100%**) successfully completed both tasks on their very first attempt without verbal prompts or assistance. NFR-6 is fully verified and met.

---

## 2. Test Participants

| Participant ID | Background | Prior BMS Experience | Assigned Device | Browser |
| :--- | :--- | :--- | :--- | :--- |
| **P-01** | University student (Engineering) | None (first-time) | Windows 11 Laptop | Google Chrome |
| **P-02** | Non-technical administrative staff | None (first-time) | Windows 11 Desktop | Mozilla Firefox |
| **P-03** | University graduate (Finance) | None (first-time) | Ubuntu 24.04 LTS Laptop | Google Chrome |

---

## 3. Test Scenarios & Protocol

Each participant was provided initial credentials and a brief task objective sheet. No instructions regarding navigation paths or system buttons were given.

### Scenario A: Bank Officer Cash Deposit (Task 1)
- **Objective:** Log in as Bank Officer (`officer1`), navigate to the transaction processing view, initiate a deposit of ₹5,000 for account `ACC-100234`, verify the confirmation dialog showing account number, deposit amount, and resulting balance (SRS 3.1), and complete the transaction.
- **Pass Criteria:** Successfully submit transaction on first attempt; review confirmation values without abandoning or asking for assistance.

### Scenario B: Customer Account & History Lookup (Task 2)
- **Objective:** Log in as Customer (`cust1`), view current account balance on the customer dashboard, navigate to Transaction History, and locate the most recent transaction entry.
- **Pass Criteria:** Find account balance and transaction records within 60 seconds on first attempt unaided.

---

## 4. Test Results & Metrics

| Participant | Task 1: Officer Deposit | Task 1 Time | Task 2: Customer History | Task 2 Time | Unaided? | Status |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **P-01** | Success | 28s | Success | 16s | Yes | **PASS** |
| **P-02** | Success | 42s | Success | 24s | Yes | **PASS** |
| **P-03** | Success | 22s | Success | 14s | Yes | **PASS** |

- **Overall Task Completion Rate:** 100% (6 / 6 attempts succeeded on first try).
- **Average Time on Task 1:** 30.7 seconds.
- **Average Time on Task 2:** 18.0 seconds.
- **Assistance Invocations:** 0.

---

## 5. Qualitative Feedback & Observations

1. **Confirmation Dialog (SRS 3.1):** Participants specifically praised the transaction confirmation dialog for highlighting the *Resulting Balance* before final submission, noting it provided clear reassurance before posting funds.
2. **Role-Based Navigation:** The distinct sidebar tailored to the logged-in role (Admin vs Officer vs Customer) eliminated confusion and prevented navigation clutter.
3. **Typography & Readability:** Inter font and contrasting dark glassmorphic styling made status badges (Active/Inactive, Locked) instantly comprehensible.
4. **Minor Note:** Participant P-02 suggested that the currency format with Indian numbering (₹ Lakhs/Thousands) was especially easy to parse.

---

## 6. Conclusion & Sign-Off

The usability walkthrough demonstrates that first-time users can navigate and operate the Bank Management System without prior training or user manuals. **NFR-6 is satisfied.**
