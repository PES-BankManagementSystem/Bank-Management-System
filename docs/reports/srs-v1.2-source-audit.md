# SRS v1.2 source and reconciliation audit

S-44 (#120), 6 November 2026. The user authorized Markdown reconstruction after Step 0 confirmed only a PDF and amendment exist. Isolated branch docs/120-S-44-final-srs starts from b66f97c0179f2e91ad8c36f020d403c426ca880d; original root CMakeLists.txt/db/.gitkeep changes are excluded.

## Sources examined

The original 19-page PDF was fully extracted before export; its hash is in srs-v1.2-traceability.json. S-34's amendment, merged SADS paragraphs/tables, merged and proposed consolidated plans, full latest pushed S-43 report, deviations, all three ADRs, schema v5/migrations, route/RBAC/session modules and all five per-story manual notes were examined with local tools. No web research. Deliverables Part-1, course SRS template and Mini-project FAQ files were not found; their retained citations do not imply independent inspection.

## Dependencies

S-34 (#65/#104), S-09 (#39/#88), S-14 (#51/#97) and S-27 (#58/#92) are merged. S-41 (#117/#124), S-43 (#119/#130) and S-46 (#122/#129) are OPEN/unmerged. Test IDs follow aff2a7b; requirement statuses follow df491ec; SADS from merged main remains the design baseline.

## Original ID inventory

| ID | Original scope |
|---|---|
| FR-1 | Authentication, role landing and RBAC |
| FR-2 | Logout, lockout and Admin recovery |
| FR-3 | Customer create/view/update/deactivate |
| FR-4 | Account opening/minimum deposit |
| FR-5 | Freeze/unfreeze/zero closure/retention |
| FR-6 | Deposit and transaction details |
| FR-7 | Withdrawal minimum/daily limit |
| FR-8 | Atomic internal transfer |
| FR-9 | Account balances |
| FR-10 | Owned/date-filtered history |
| FR-11 | Interest/duplicate prevention |
| FR-12 | Statements/export |
| FR-13 | Staff CRUD/deactivate/unlock |
| FR-14 | Role dashboards |
| NFR-1 | 2-second persisted transactions |
| NFR-2 | 3-second standard views |
| NFR-3 | Atomicity/balance integrity |
| NFR-4 | Credentials/authorization |
| NFR-5 | Append-only audit |
| NFR-6 | Three-person first-use usability |
| NFR-7 | Graceful invalid-input handling |
| NFR-8 | Modular maintainability |
| NFR-9 | Two-platform/two-browser portability |

The original eight business rules were unnumbered: Admin-only staff/type writes; staff-only customer/account/transaction operations; Customer ownership; withdrawal/transfer source minimum; zero closure; lifecycle retention; one ledger row per balance change/two per transfer; once-per-period interest. BR-1 through BR-8 preserve that order with explicit reconciliations; BR-9/BR-10 label existing UTC/type-update decisions.

## Built-feature gap table

| Built feature / code or manual evidence | Existing requirement | Disposition |
|---|---|---|
| Login/RBAC/role landing; auth/routes.cpp/rbac.cpp | FR-1/NFR-4 | Preserve; actual username/password API, no separate PIN. |
| Random fixed-lifetime tokens/current-token logout; auth/routes.cpp | FR-2/S-34 SO-3 | Correct all-sessions wording; add SEC-6. |
| Three-failure lockout/Admin recovery; unlock.js/auth routes | FR-2/FR-13 (embedded unlock) | Add independent FR-16, preserving original IDs. |
| Customer register/search/view/update; records.cpp | FR-3 | Required email/credential exception explicit. |
| Admin-only deactivation; S-13 notes | FR-3 allowed Officer too | Reconcile row 11; no reactivation route. |
| Independent customer lifecycle/own profile; customer routes | FR-3/scope | Explain optional unique login link and contact/login separation. |
| Opening and zero-minimum deposit; account_opening.cpp/S-15 | FR-4 | Admin permission, atomic opening ledger/audit and v5 exception explicit. |
| Freeze/unfreeze/zero closure; account_status.cpp/S-16 | FR-5 | Staff roles, idempotency and retained history explicit. |
| Type CRUD/list/bounds; account_types.cpp/S-14 | Scope/BR, no independent FR | Add FR-15; master case gap remains. |
| Deposit/withdraw confirmation; transaction modules, app.js/withdraw.js | FR-6/FR-7/section 3.1 | Expose missing pre-commit resulting-balance preview. |
| UTC daily limit; withdraw.cpp | FR-7 | Explain cumulative per-account/type limit and transfer exclusion. |
| Atomic paired transfer; transfer.cpp | FR-8 | Full rollback, not a separately committed reversal. |
| Owned account list/balance; account_queries.cpp | FR-9 | Admin reads and actual own-list route explicit. |
| UTC/snapshot-paginated history; reporting routes/history.js/S-21 | FR-10 | Explain signed data, staff roles, refusal-audit limitation. |
| Completed-month interest; reporting/interest.cpp | FR-11 | Retain provisional basis; no new approval. |
| Print/CSV statement; statement.cpp/statement_csv.js | FR-12 | Actual export forms; no built-in statement PDF claim. |
| Officer create/list/rename/deactivate/reactivate; staff routes | FR-13 | Keep existing ID; make supported changes explicit. |
| Role dashboards; staff routes/landing.js | FR-14 | Keep existing ID, no duplicate FR. |
| Atomic append-only financial audit; record.cpp/schema | NFR-5 | Preserve, no cryptographic tamper-proof claim. |
| Admin filtered/paginated viewer; audit routes/audit.js | No independent FR, NFR-5 covers storage | Add FR-17; TC-16 covers only part of its scope. |

## Deviation classification

| Main row | Effect on v1.2 |
|---|---|
| 1 | Design implementation: serialized SQLite writes, not weakened atomicity; 2.5/NFR-3. |
| 3-4 | Accepted/refined customer login, required email and one-time credential exception; FR-3/NFR-4/SEC-2. |
| 6 | PROVISIONAL interest basis, not approved business decision; FR-11/TBD-5 stays provisional. |
| 7 | One-time staff credentials/retained identity; FR-13/NFR-4/SEC-2. |
| 9-10 | Own-profile privacy, optional login/independent lifecycle; FR-3/interfaces/model. |
| 11 | Permission change to Admin-only deactivation, idempotency/no reactivation; FR-3/BR-1. |
| 12 | Type bounds, future-operation rules and Officer GET; FR-15/FR-7/BR-10. |
| 13 | Staff status roles, idempotency, active/frozen zero closure; FR-5/BR-5. |
| 14 | UTC dates/signed statements/print and CSV/staff reads; FR-12. |
| 17 | Explicit implementation input bounds; NFR-7. |
| 18-19 | Admin reads, actual own-list route, UTC stable pagination, refusal-audit limits; FR-9/FR-10. |
| 20 | Admin opening, zero deposit and multiple same-type accounts; FR-4/BR-7. |
| 21 | UTC withdrawal day/transfer exclusion; FR-7/BR-9. |
| 22 | Full rollback instead of compensating reversal; FR-8/NFR-3. |
| 23-24 | Actual unlock/config/audit routes and permissions; interfaces/FR-15/16/17. |

Rows 2 and 15-16 are resolved/historical implementation/execution notes, except row 16's API-versus-page timing limitation remains material to NFR-2. Rows 5/8 are test-ID disagreements rather than changed product requirements. Rows 25-27 concern demo/sprint delivery. These unrelated rows remain unchanged.

## Disagreements and approval limits

Appendix F lists role, status-code, payload, seed-fixture, legacy-TC, model and security-ID disagreements. New row 29 explicitly records three wording reconciliations requiring owner review: resulting-balance preview timing, blanket minimum-balance scope and rejected-request security bookkeeping. These are not assertions of owner approval or passed tests. S-34's global logout claim is corrected to the code's single-token behavior; inactive users are denied on subsequent requests.

The schema-derived model includes all seven v5 tables. Customer-login is optional at both ends. transactions.interest_run_id has no FK; transfer pairing and audit entity_id are logical associations. The old design/er-diagram.mmd is preserved for S-46 to reconcile.

## Validation and integration

All existing FR/NFR/SEC verdicts are copied from the pinned S-43 requirement table; only Not validated/unassessed rows are added for new IDs. Of 17 FRs and 9 NFRs, NFR-8 alone is Validated and 25 are Not validated. TC-21 remains NOT RUN, TC-22 PARTIAL, exit NOT MET. No system tests were simulated or rerun by this writing task.

See srs-new-fr-ids.md for consumer actions and row collisions: S-44 allocates rows 28-29 from main while unmerged S-43 independently uses 28-30. Preserve both and update cross-references on integration. No PR is opened or merge performed by this task.
