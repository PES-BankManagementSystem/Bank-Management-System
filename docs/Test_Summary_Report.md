# Test Summary Report — S-45 (#121)

**Status:** Sprint 3 report for human review. The application build and measurements use the tested base commit below; these report-only corrections are layered on that commit.

## 1. Scope and tested base

This report consolidates the S-41 Test Plan v1.2 cases, the S-43 System Validation results, the unit-test and coverage reports, and fresh local NFR measurements taken from the latest fetched `origin/main` snapshot. It covers the issue requirements for a current test summary, traceability, NFR-1/NFR-2 and open risks.

- **Application base commit tested for this draft (full SHA):** `4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8`. This is the current `origin/main` commit to which `feature/121-test-summary` was fast-forwarded before the report corrections; the final documentation commit will have a different SHA.
- **Commit date:** 2026-11-06.
- **Measurement date:** 2026-11-06.
- **Local build:** CMake Release, AppleClang 17, ARM64, in `/tmp/bms-121-main-4cf8b3c/build-release`; the old project `build/` directory was not used. The full local CTest run passed 31/31 targets in 20.48 seconds.
- **Build configuration:** `cmake -S . -B /tmp/bms-121-main-4cf8b3c/build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64`, with the installed Command Line Tools libc++ include path supplied as `CMAKE_CXX_FLAGS` because this host otherwise could not find `<cstddef>`.
- **Database:** two separate fresh SQLite databases under `/tmp`, each initialized from `db/schema.sql` and `db/seed.sql`, then extended with 3,000 generated accounts (3,005 total). The NFR-1 database was used only for transaction timing; the NFR-2 database was separately seeded and used for read/dashboard timing.
- **Measurement scope:** local single-machine, serial requests. The result is not a concurrent-load or cross-platform benchmark.

### Linked evidence

- [Unit Test Report](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8/docs/Unit_Test_Report.md)
- [System Validation Report](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8/docs/System_Validation_Report.md)
- [Code Coverage Report](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8/docs/Code_Coverage_Report.md)
- [Branch Coverage Report](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8/docs/Branch_Coverage_Report.md)
- [S-41 Test Plan v1.2](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8/docs/TEST_PLAN_CONSOLIDATED.md)
- [SRS v1.1](SRS_v1.1_UPDATE.md) · [SADS](Software_Architecture_and_Design_Specification.docx) · [Deviations log](DEVIATIONS.md)

The S-41 plan v1.2 has no separate old-to-new ID crosswalk and no standalone exit-criteria section. The mapping below is a semantic crosswalk derived from the v1.0 objectives and the v1.2 test descriptions/traceability matrix; it does not claim that S-41 published this table.

## 2. Test case ID crosswalk

| Test Plan v1.0 ID and objective | S-41 v1.2 ID(s) | Mapping note |
|---|---|---|
| TC-01 — login and role routing | TC-01; TC-14 | Login is TC-01; role-specific dashboard behavior is now tested separately by TC-14. |
| TC-02 — lockout after failed login and administrator unlock | TC-02; TC-02B; TC-03 | Lockout is TC-02, unlock is TC-02B, and logout/session invalidation is isolated in TC-03. |
| TC-03 — create customer | TC-04 | Customer registration is TC-04 in v1.2. |
| TC-04 — reject customer with missing field | TC-04 | v1.2 TC-04 covers registration validation. |
| TC-05 — account opening minimum | TC-05 | Same primary objective; v1.2 adds exact-minimum and zero-minimum opening coverage. |
| TC-06 — frozen-account operations | TC-07 | Freeze/unfreeze/close behavior is TC-07 in v1.2. |
| TC-07 — deposit | TC-08 | Deposit and confirmation behavior is TC-08. |
| TC-08 — withdrawal minimum balance | TC-06; TC-09 | Minimum balance is TC-06; daily limit is separately TC-09. |
| TC-09 — atomic transfer | TC-10 | Atomic transfer is TC-10. |
| TC-10 — reject same-account transfer | TC-10B | Invalid transfers are TC-10B. |
| TC-11 — balance/history ownership | TC-11; TC-17 | Ownership/balance is TC-11; date-filtered history is TC-17. |
| TC-12 — prevent duplicate interest credit | TC-09B | Interest idempotency is TC-09B. |
| TC-13 — officer deactivation and historical attribution | TC-13 | Same primary objective. |
| TC-14 — balance-changing transaction performance (NFR-1) | TC-20 | NFR-1 performance is TC-20. |
| TC-15 — credential handling | TC-01; TC-15 | Credential/hash checks map to TC-01; TLS is separately covered by TC-15. |
| TC-16 — append-only audit | TC-16 | Same primary objective. |
| TC-17 — invalid input and graceful errors | TC-02; TC-09; TC-10B; TC-11 | Reliability/error checks are distributed across these v1.2 cases; v1.2 TC-17 instead covers date-filtered history. |

Additional v1.2 cases with no equivalent standalone v1.0 case are TC-04B (administrator-only customer deactivation), TC-18 (statement/CSV), TC-19 (account-list/history performance), TC-21 (usability), and TC-22 (cross-platform/browser portability). TC-14 is also new dashboard behavior coverage; the dashboard-routing step in old TC-01 is only a partial objective match.

## 3. Sprint 3 system test results

The S-43 report records 26 v1.2 cases: 23 historical PASS, 2 PARTIAL, 0 FAIL, and 1 NOT RUN. “Executed” includes partial runs. Only TC-15 was confirmed by S-43 as independently fresh-seeded; other historical passes are observations, not completed final fresh-seed acceptance. The S-45 NFR run adds current-base measurements for TC-14/TC-19/TC-20 but does not rerun the remaining system cases.

| v1.2 case | S-43 result | S-45 evidence/status note |
|---|---|---|
| TC-01 — valid authentication | PASS | Historical role logins; CTest also passed. |
| TC-02 — lockout | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-02B — administrative unlock | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-03 — logout/session invalidation | PARTIAL | Logout returned 204 and old token returned 401; UI redirect was not verified. |
| TC-04 — registration validation | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-04B — customer deactivation | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-05 — account opening | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-06 — withdrawal minimum/daily limit | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-07 — freeze/close | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-08 — deposit confirmation | PASS | Historical result; seed balance differed from the plan precondition. |
| TC-09 — daily limit/UTC boundary | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-09B — interest once per period | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-10 — atomic transfer | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-10B — invalid transfers | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-11 — customer balance/history ownership | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-12 — Bank Officer creation | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-13 — officer deactivation/session invalidation | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-14 — role-specific dashboards | PASS | Re-measured on current base via API and browser; see NFR-2. |
| TC-15 — HTTPS/TLS | PASS | S-43 identifies this as the one independently fresh-seeded case. |
| TC-16 — append-only audit | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-17 — date-filtered history | PASS | Historical result; fresh-seed acceptance still outstanding. |
| TC-18 — statement/CSV | PASS | Historical Blob content verified; native browser save event was not captured. |
| TC-19 — account-list/history performance | PASS | Re-measured on a separately fresh-seeded current-base database; see NFR-2. |
| TC-20 — transaction performance | PASS | Re-measured on a fresh current-base database; see NFR-1. |
| TC-21 — first-time usability | NOT RUN | Requires three genuine first-time participants; no participant results were simulated. |
| TC-22 — cross-platform/browser portability | PARTIAL | CI/build evidence exists in S-43; fresh Chrome/Firefox responsive comparison is missing. |

### Test execution totals

| Measure | Count | Percentage |
|---|---:|---:|
| Total planned v1.2 cases | 26 | 100.0% |
| Executed, including partial | 25 | 96.2% of planned |
| PASS | 23 | 88.5% of planned; 92.0% of executed |
| PARTIAL | 2 | 7.7% of planned |
| FAIL | 0 | 0.0% of planned |
| NOT RUN | 1 | 3.8% of planned |

Counts reconcile: 23 PASS + 2 PARTIAL + 0 FAIL + 1 NOT RUN = 26 planned; executed = 23 + 2 + 0 = 25.

### Automated unit-test run

The [Unit Test Report](Unit_Test_Report.md) reports 31/31 CTest targets passed. A fresh Release build and complete CTest run from the exact base commit in this draft also passed **31/31 targets**, with 0 failed, in 20.48 seconds. The first sandboxed attempt could not bind loopback sockets; the full run was repeated with local socket permission and passed. CTest targets are not the same unit as Test Plan system cases, so these totals are reported separately.

## 4. Coverage summary

Coverage figures below are the local macOS gcovr results in the coverage reports, after the S-42 test-only additions. They are not CI percentages. The latest successful main CI run is [run 42257808066](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42257808066), for immutable commit `4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8`; it produced the [`coverage-report` artifact 13524755371](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42257808066#artifacts). Anonymous artifact download returned HTTP 401, and `gh auth status` reports an invalid token.

| Module | Line coverage | Branch coverage | Line ≥80% | Branch ≥60% |
|---|---:|---:|---|---|
| account | 92.0% | 52.0% | Yes | No |
| api | 88.2% | 44.0% | Yes | No |
| audit | 100.0% | 54.3% | Yes | No |
| auth | 93.6% | 53.6% | Yes | No |
| common | 80.2% | 45.6% | Yes | No |
| customer | 96.7% | 54.5% | Yes | No |
| main | 18.2% | 9.3% | No | No |
| reporting | 95.8% | 51.9% | Yes | No |
| staff | 97.2% | 47.4% | Yes | No |
| transaction | 97.4% | 51.3% | Yes | No |
| **Overall** | **92.1%** | **50.7%** | — | — |

- **Below the 80% line threshold:** `main` (18.2%).
- **Below the 60% branch threshold:** all ten modules and overall branch coverage (50.7%).
- **CI coverage values:** NOT AVAILABLE: authenticate with a valid GitHub account, download `coverage-report` artifact 13524755371 from [successful CI run 42257808066](https://github.com/PES-BankManagementSystem/Bank-Management-System/actions/runs/42257808066), and read `summary.json`; anonymous artifact download returned HTTP 401. Do not substitute the local coverage figures above.

## 5. Requirement coverage

The S-41 v1.2 RTM contains 25 requirement IDs: FR-1–FR-14, NFR-1–NFR-9, SEC-1 and SEC-2. The table below answers the requested “at least one mapped passing case” measure using S-43 case outcomes and the current-base NFR remeasurements. This measure is not the same as final requirement validation: S-43 withheld validation unless all linked cases met its independent fresh-seed acceptance condition.

| Coverage measure | Requirements | Result |
|---|---|---:|
| At least one mapped PASS result | FR-1–FR-14; NFR-1–NFR-5; NFR-7–NFR-8; SEC-1–SEC-2 | 23/25 (92.0%) |
| No passing mapped case | NFR-6 (TC-21 not run); NFR-9 (TC-22 partial) | 2/25 (8.0%) |
| Strict validated status in S-43 | NFR-8 only; remaining rows are marked “Not validated” in the source report | 1/25 (4.0%) |

The 23/25 measure means a mapped case has a PASS observation; it does not override partial cases or the fresh-seed limitation. S-43's stricter 1/25 measure is retained as the source report's validation decision. The NFR-1 and NFR-2 local reruns provide new threshold observations but do not independently rerun every linked system case.

## 6. Defects

`gh auth status` failed: the saved GitHub token for `adityamahesh728-dot` is invalid. No authentication repair or issue query was attempted.

**Defect totals, severity counts, and open/closed counts: NOT AVAILABLE: needs a gh login or a pasted issue list.** The S-43 report records that its earlier issue review found no open critical defect at that review snapshot; that historical observation is not a current issue-label count and is not substituted here.

| Severity/status | Open | Closed | Total |
|---|---:|---:|---:|
| Critical | NOT AVAILABLE | NOT AVAILABLE | NOT AVAILABLE |
| Major | NOT AVAILABLE | NOT AVAILABLE | NOT AVAILABLE |
| Minor | NOT AVAILABLE | NOT AVAILABLE | NOT AVAILABLE |
| **All severities** | **NOT AVAILABLE** | **NOT AVAILABLE** | **NOT AVAILABLE** |

## 7. NFR-1 and NFR-2 remeasurement

### Requirements and method

- **NFR-1 threshold:** each deposit, withdrawal, or transfer, including database persistence, under 2,000 ms; source: SRS v1.1 §5.1 and Test Plan v1.2 TC-20.
- **NFR-2 threshold:** dashboards, account lists, and transaction history under 3,000 ms; source: SRS v1.1 §5.1 and Test Plan v1.2 TC-14/TC-19.
- **Dataset:** 3,005 total accounts (3,000 generated); each database started from schema and seed SQL. Five generated accounts have 300 historical transactions each; other generated accounts have 12.
- **Repetitions:** 3 warmups, then 30 measured requests per API operation/view/role. The SRS and Test Plan do not prescribe the run count or an acceptance percentile; 3/30 follows the existing S-43/helper measurement protocol. Mean, median, p95, p99 and maximum are descriptive; no percentile is claimed as the acceptance rule.
- **Load:** one serial local client, no concurrent user load. The documents say “normal usage” / “normal simulated load” without defining concurrency or workload. Therefore these are local threshold observations, not proof under an agreed normal-load profile.
- **Machine/build:** MacBook Air (MacBookAir10,1), Apple M1, 8 cores (4 performance and 4 efficiency), 8 GB RAM; macOS 15.5 (24F74), ARM64; AppleClang 17.0.0.17000013; CMake 4.4.4; Release. CMake was configured with `CMAKE_OSX_ARCHITECTURES=arm64` and the installed Command Line Tools libc++ include directory so this host's compiler could find `<cstddef>`; no source or optimization flags were changed for that workaround.
- **Scope:** local, single-machine measurements. Another OS, machine, CI runner, or production workload may differ.

### NFR-1 transaction timing

Fresh database: [`/tmp/bms-perf-121.sqlite`](reports/nfr-measurements-api-2026-11-06.json). Each request was authenticated. The helper verified the response balance for a measured transaction against its persisted SQLite balance.

| Operation | Samples | Mean | Median | p95 | Maximum | Threshold | Sample result |
|---|---:|---:|---:|---:|---:|---:|---|
| Deposit | 30 | 0.787 ms | 0.765 ms | 0.911 ms | 1.041 ms | <2,000 ms | Below threshold |
| Withdrawal | 30 | 0.801 ms | 0.770 ms | 1.049 ms | 1.057 ms | <2,000 ms | Below threshold |
| Transfer | 30 | 0.944 ms | 0.936 ms | 1.060 ms | 1.083 ms | <2,000 ms | Below threshold |

All measured transaction responses succeeded and the sampled persistence check matched. The sample maxima meet the numeric limit for this serial local run. Formal performance under “normal simulated load” is NOT AVAILABLE: the SRS/Test Plan do not specify the load profile needed to reproduce that condition.

### NFR-2 API and S-25 page timing

Fresh independent database: [`/tmp/bms-perf-121-nfr2.sqlite`](reports/nfr2-measurements-2026-11-06.json). API timings include 3 warmups and 30 measured requests per view. The dashboard page timing uses headless Chrome 156.0.8078.80; each sample navigated to `/`, waited for document load, dispatched the authenticated role session, and timed until the S-25 dashboard's `/dashboard` request completed and its DOM rendered. Raw browser timings are in [`nfr2-dashboard-page-2026-11-06.json`](reports/nfr2-dashboard-page-2026-11-06.json).

| View | Samples | Mean | p95 | Maximum | Threshold | Sample result |
|---|---:|---:|---:|---:|---:|---|
| `GET /dashboard` — Administrator | 30 | 5.879 ms | 12.999 ms | 25.236 ms | <3,000 ms | Below threshold |
| `GET /dashboard` — Bank Officer | 30 | 4.960 ms | 5.275 ms | 5.414 ms | <3,000 ms | Below threshold |
| `GET /dashboard` — Customer | 30 | 0.484 ms | 0.636 ms | 0.771 ms | <3,000 ms | Below threshold |
| `GET /customers/me/accounts` — Customer account list | 30 | 0.348 ms | 0.425 ms | 0.429 ms | <3,000 ms | Below threshold |
| `GET /accounts/PF000001/transactions?from=2023-01-01&to=2026-12-31` — history | 30 | 1.365 ms | 6.522 ms | 16.298 ms | <3,000 ms | Below threshold |
| S-25 page — Administrator (navigation + API + render) | 30 | 29.656 ms | 39.082 ms | 53.545 ms | <3,000 ms | Below threshold |
| S-25 page — Bank Officer (navigation + API + render) | 30 | 27.505 ms | 30.385 ms | 30.614 ms | <3,000 ms | Below threshold |
| S-25 page — Customer (navigation + API + render) | 30 | 17.762 ms | 29.827 ms | 31.195 ms | <3,000 ms | Below threshold |

All requests returned HTTP 200 and the dashboard responses had the expected role. These maxima meet the numeric threshold in the measured serial run. Formal NFR-2 performance under “normal simulated load” is NOT AVAILABLE: the SRS/Test Plan do not define a concurrency or workload profile. This does not claim CI, Windows, or production performance.

## 8. Test Plan pass/fail and exit-criteria verdicts

S-41 Test Plan v1.2 does not define a separate numbered exit-criteria list. Its §6 says a feature passes only when all associated cases pass; §8 lists the test summary report as a deliverable. S-43's system validation review makes its release checks explicit. The verdicts below separate those sources.

| Criterion | Verdict | Evidence |
|---|---|---|
| Test Plan §6 — all associated cases for a feature pass | **NOT MET overall** | TC-03 and TC-22 are partial and TC-21 is not run; see the [S-43 case table](https://github.com/PES-BankManagementSystem/Bank-Management-System/blob/4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8/docs/System_Validation_Report.md) and this report's §3. |
| Test Plan §8 — prepare a Test Summary Report | **MET as a draft deliverable** | This report consolidates the linked reports and evidence. Human review remains outstanding. |
| S-43 release check — all planned cases executed | **NOT MET** | 25/26 cases were executed; TC-21 was not run and TC-22 is partial. |
| S-43 release check — independent fresh-seed acceptance per case | **NOT MET** | S-43 confirms TC-15 only; its other historical passes did not have independently confirmed fresh seeds. S-45 adds fresh seeded performance datasets, not a rerun of every case. |
| S-43 release check — no critical defect open | **NOT AVAILABLE** | Current severity/status issue data require `gh` authentication or a pasted issue list; see §6. |
| NFR-1 numeric timing in this local sample | **Below threshold in the measured run** | 90 measured persisted transaction requests; maximum 1.083 ms against 2,000 ms. This does not establish the unspecified normal-load condition. |
| NFR-2 numeric timing in this local sample | **Below threshold in the measured run** | API and rendered S-25 role dashboard maxima were below 3,000 ms; see §7 and raw reports. This does not establish the unspecified normal-load condition. |
| Overall S-43 exit decision | **NOT MET** | All-cases-executed and independent-seed checks are not met; current critical-defect status is unavailable. |

## 9. Open risks and known gaps

- The feature branch was fast-forwarded to tested application base `4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8` before these report-only edits. The final documentation commit will be newer than the application commit that was built and measured.
- Defect totals and current severity/open/closed labels are unavailable because `gh auth status` reports an invalid token.
- The SRS/Test Plan do not define what “normal usage” or “normal simulated load” means, nor a concurrency, run count, or acceptance percentile. Current local measurements used one serial client; a team-approved load profile is needed for a conclusive performance verdict.
- TC-21 still needs three genuine first-time participants. TC-22 still needs fresh Chrome and Firefox responsive-flow checks and a rendering comparison.
- S-43's historical passes remain short of independent fresh-seed acceptance for all but TC-15. The current performance reruns do not replace that system-validation work.
- Only `main` (18.2%) is below the 80% local line-coverage threshold; every module is below the 60% local branch-coverage threshold. The required CI coverage percentages remain unavailable from the coverage artifact.
- The merged S-43 report preserves some prerequisite-state text written before S-41/S-43 merged. Its case evidence is retained as historical; this summary links to it at the current main commit and flags the stale snapshot in [DEVIATIONS.md](DEVIATIONS.md).
- Windows was not run locally. The S-43 report links prior Ubuntu/Windows CI evidence, but this draft does not claim a fresh Windows run at the current SHA.

## 10. Historical Sprint 2 results — S-29, v1.0 IDs

The S-29 report used the original v1.0 TC-01–TC-17 numbering and a then-tested source commit `14291f31d9c32f8f4b719c1f56194578afe2e995`. Its recorded system-test summary was 17 total cases: 13 PASS, 0 FAIL, 4 BLOCKED, 0 not executed; 76.5% passed out of all planned cases and 100% of executable cases passed. The four blocked cases were the old TC-01 dashboard walkthrough, TC-05 account opening, TC-06 account-state/viewing flow, and TC-11 balance/history APIs.

Historical one-sample NFR-1 timings were deposit 1.54 ms, withdrawal 1.01 ms and transfer 1.24 ms, each below 2 seconds for that run. The old NFR-2 `GET /` shell measurement was 39.10 ms but was explicitly not a dashboard, account-list or history measurement. These S-29 values are preserved as history only and are superseded by the S-45 measurements above. See the earlier [Test Execution Log](Test_Execution_Log.md) and [S-30 raw performance results](perf/results.json).

## 11. Assumptions and source limitations

- The v1.0-to-v1.2 crosswalk is based on test objectives and the v1.2 RTM; S-41 does not provide a formal crosswalk table.
- The “23/25 requirements covered” calculation means at least one associated test has a PASS observation. It is distinct from the S-43 “validated” status, which is stricter and requires its independent fresh-seed criterion.
- The 3-warmup/30-measurement protocol follows the existing S-43/helper procedure; it is not specified as a required run count by the SRS or S-41 plan. Percentiles are included as descriptive statistics only.
- `gh auth status` is invalid, so no current GitHub defect labels were read. No severity or issue count has been inferred from the absence of local test failures.
- The NFR measurements apply to the application base SHA above, not a separate build of the final documentation commit. No application source changed after the measurements.
