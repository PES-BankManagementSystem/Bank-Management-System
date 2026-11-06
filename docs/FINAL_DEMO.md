# Final Demo Script

This is the script for the final evaluation. It covers the core use cases for all three roles, the deviations to explain, and what each member delivered.

The whole script takes about 10 minutes. The recorded demos are [Sprint 1](../demos/sprint-1.mp4) and [Sprint 2](../demos/sprint-2.mp4). Requirement IDs follow SRS v1.2.

The script was last run end to end on 6 Nov 2026, in headless Firefox on a fresh database, against `main` at `4cf8b3c`. Every step gave the result shown here, with no JavaScript errors. That run clicked the CSV download in step 12 but did not confirm the saved file, and this exact script has not been run in Chrome.

## Before the demo

Start from a fresh database, because the script changes balances. From the repository root on Linux:

```sh
cmake -S . -B build -DBUILD_TESTING=OFF
cmake --build build --target bms_server --parallel 4
rm -f bms.db
sqlite3 bms.db < db/schema.sql
sqlite3 bms.db < db/seed.sql
./build/bin/bms_server
```

Open <http://127.0.0.1:8080>. All seeded users have the password `Password123!`.

| Role | Username | Seeded accounts |
|---|---|---|
| Bank Officer | `officer1` | None |
| Customer | `customer1` (John Doe) | ACC001 and ACC004 |
| Customer | `customer2` (Jane Smith) | ACC002 (frozen) and ACC005 |
| Administrator | `admin` | None |

## Bank Officer

Sign in as `officer1`.

| # | Do this | Show | Requirement |
|---|---|---|---|
| 1 | Open the dashboard | Transactions processed today | FR-14 |
| 2 | Deposit 500.00 into ACC001 | The new balance and a confirmation step | FR-6 |
| 3 | Withdraw 200.00 from ACC001 | The new balance | FR-7 |
| 4 | Transfer 100.00 from ACC001 to ACC004 | Both accounts change together | FR-8, NFR-3 |
| 5 | Deposit 10.00 into ACC002 | Refused, because the account is frozen | FR-6, NFR-7 |
| 6 | Register a customer | The customer ID and one-time password | FR-3 |
| 7 | Open a Savings account for that customer with 5000.00 | The review step, then the account number | FR-4 |
| 8 | Search for Jane, choose **View**, then freeze and unfreeze ACC005 | The status changes | FR-5 |
| 9 | Choose **Close account** on ACC005 | Refused, because the balance is not zero | FR-5 |

## Customer

Sign out and sign in as `customer1`.

| # | Do this | Show | Requirement |
|---|---|---|---|
| 10 | Open the dashboard | Only ACC001 (₹50,200.00) and ACC004 (₹100,100.00), with balances that include steps 2-4 | FR-9, FR-14 |
| 11 | Open **History**, choose ACC001 and filter by date | The filtered transactions | FR-10 |
| 12 | Open **Statements** for ACC001 and download the CSV | Opening and closing balances, and the file | FR-12 |

## Administrator

Sign out and sign in as `admin`.

| # | Do this | Show | Requirement |
|---|---|---|---|
| 13 | Open the dashboard | Bank-wide counts and today's transactions | FR-14 |
| 14 | Add an account type | It appears in the list | FR-15 |
| 15 | Create a Bank Officer | The one-time password, and the officer in the list | FR-13 |
| 16 | Search for Bob and deactivate the customer | The record stays, with status Inactive | FR-3 |
| 17 | Run interest for last month | Accounts credited and the total. The form clears, so choose the month again for a second run, which is refused | FR-11 |
| 18 | Open **Audit trail** and choose **Show entries** | The changes made above, newest first | FR-17, NFR-5, SEC-4 |

To show lockout (FR-2, SEC-5), sign in with a wrong password three times as `officer2`, then unlock the user from **Locked users** as `admin` (FR-16).

## Deviations to explain

The full list, with reasons, is in [DEVIATIONS.md](DEVIATIONS.md).

| Row | What differs | Why |
|---|---|---|
| 1 | SQLite takes a database-wide write lock, not row locks | SQLite has no row locks, and writes running one at a time still lose no updates |
| 6 | Interest uses the month-end balance and actual days in the year | The SRS left the basis open (TBD-5) |
| 11 | Only an Administrator can deactivate a customer | The S-13 story is stricter than FR-3 |
| 21 | The daily withdrawal limit resets at 05:30 IST | It is counted per UTC day |
| 22 | A failed transfer rolls back with no reversal entry | One database transaction gives "both or neither" directly |
| 23 | Some API routes differ from SADS v1.0 | Unlock applies to all users, and later stories needed more routes; the SADS now matches |
| 25, 26, 27 | The demo videos are late, and their length differs from the stories | Sprint 1 was recorded late from end-of-sprint code, S-38 waited for the dashboards, and Sprint 2 was recorded at a human pace |
| 29, 30 | System validation is incomplete: TC-21 was not run, TC-22 is partial, and most cases lack an independent fresh-seed run | The results are reported as they stand in the [System Validation Report](System_Validation_Report.md), and nothing was simulated |

## What each member delivered

Points and stories come from `.github/backlog/backlog.yml`. A dash means no merged pull request says it closes that story.

### Aditya Vats (PES1UG24CS032): 33 points

| Story | Title | Points | Issue | PR | State |
|---|---|---|---|---|---|
| S-01 | Finalize tech stack and record the decision | 1 | #31 | #72 | closed |
| S-02 | Create the repository skeleton and build system | 3 | #32 | #73 | closed |
| S-04 | Set up the HTTP API server skeleton and error format | 3 | #34 | #74 | closed |
| S-10 | Enforce role-based access control in the back-end core | 5 | #40 | #93 | closed |
| S-26 | Record every balance change in an append-only audit trail | 3 | #48 | #91 | closed |
| S-37 | Record and upload the Sprint 1 demo video | 1 | #49 | #112 | closed |
| S-27 | Audit trail viewer for Administrators | 2 | #58 | #92 | closed |
| S-28 | HTTPS configuration and secrets hygiene | 2 | #59 | #94 | closed |
| S-31 | Verify portability on two platforms and two browsers | 2 | #62 | #111 | closed |
| S-36 | Update the SADS: traceability, API and data design | 2 | #67 | #95 | closed |
| S-38 | Record the Sprint 2 demo video and finish the README | 2 | #68 | #125 | closed |
| S-39 | Maintain the deviations log | 1 | #69 | #110 | closed |
| S-40 | Measure code and branch coverage in CI | 3 | #116 | #123 | closed |
| S-46 | Finalize the SADS, review the deviations log and freeze the release | 3 | #122 | - | open |

### Abhishek Hanamant Kinagi (PES1UG24CS018): 32 points

| Story | Title | Points | Issue | PR | State |
|---|---|---|---|---|---|
| S-05 | Build the web front-end shell and shared UI components | 3 | #35 | #76, #85 | closed |
| S-07 | Log in with user ID and password/PIN | 5 | #37 | #86 | closed |
| S-08 | Session handling and logout | 3 | #38 | #87 | closed |
| S-09 | Lock accounts after three failed logins; admin unlock | 3 | #39 | #88 | closed |
| S-24 | Manage Bank Officer accounts | 3 | #56 | #89 | closed |
| S-25 | Role-specific dashboards | 5 | #57 | #113 | closed |
| S-32 | Run a usability walkthrough | 1 | #63 | - | closed |
| S-34 | Fix SRS gaps: use case diagram, security objectives, references | 3 | #65 | #104 | closed |
| S-35 | Consolidate the Test Plan and test cases | 3 | #66 | #103 | closed |
| S-41 | Update the test cases and the Test Plan for sprint changes | 3 | #117 | #124 | closed |

### Adithya Ajeeth (PES1UG24CS022): 36 points

| Story | Title | Points | Issue | PR | State |
|---|---|---|---|---|---|
| S-03 | Design database schema, migrations and seed data | 5 | #33 | #75 | closed |
| S-11 | Register a new customer | 3 | #41 | #84 | closed |
| S-12 | Search, view and update customer records | 3 | #42 | #90 | closed |
| S-15 | Open a new account for a customer | 5 | #43 | #105 | closed |
| S-20 | View account balances | 2 | #47 | #106 | closed |
| S-13 | Deactivate a customer | 2 | #50 | #96 | closed |
| S-14 | Manage account types | 3 | #51 | #97 | closed |
| S-16 | Freeze, unfreeze and close accounts | 3 | #52 | #102 | closed |
| S-21 | View transaction history with a date filter | 3 | #53 | #107 | closed |
| S-43 | Execute system validation and write the System Validation Report | 5 | #119 | #130 | closed |
| S-44 | Finalize the SRS as v1.2 in IEEE format | 2 | #120 | - | open |

### Aditya M (PES1UG24CS027): 37 points

| Story | Title | Points | Issue | PR | State |
|---|---|---|---|---|---|
| S-06 | Set up the unit-test framework and run it in CI | 2 | #36 | #77 | closed |
| S-17 | Deposit money into an account | 3 | #44 | #78 | closed |
| S-18 | Withdraw money with minimum-balance and daily-limit checks | 5 | #45 | #79 | closed |
| S-19 | Transfer funds between two accounts atomically | 5 | #46 | #80 | closed |
| S-22 | Run interest crediting for a period | 5 | #54 | #82 | closed |
| S-23 | Generate and export account statements | 3 | #55 | #98 | closed |
| S-29 | Execute the Test Plan and write the test summary report | 3 | #60 | #99 | closed |
| S-30 | Verify performance targets | 2 | #61 | #100 | closed |
| S-33 | Harden input validation and error handling | 2 | #64 | #101 | closed |
| S-42 | Write the Unit Test, Code Coverage and Branch Coverage reports | 5 | #118 | #126 | closed |
| S-45 | Update the Test Summary Report with the Sprint 3 results | 2 | #121 | - | open |
