# SRS v1.2 requirement ID handoff

S-44 / #120; 6 November 2026. Consumers: S-41 (#117 / PR #124), S-46 (#122 / PR #129), S-43 (#119 / PR #130). This tracked handoff has not been sent as a message or merged into those stories.

The final list is **FR-1 through FR-17** (17 total). Existing FR-1 through FR-14 and NFR-1 through NFR-9 retain their IDs. Staff (FR-13), dashboards (FR-14), interest (FR-11) and statements (FR-12) already have IDs and are not duplicated. BR-1 through BR-8 label previously unnumbered rules; BR-9/BR-10 identify the existing UTC/configuration rules.

| New ID | Scope / role / priority | S-41 coverage action | S-46 design action |
|---|---|---|---|
| FR-15 | Account-type CRUD/list; Administrator writes, Officer/Admin reads; Medium | No full CRUD case exists. Add S-14 configuration/bounds/RBAC checks. TC-05/06/09 overlap is insufficient; account_type_test.cpp and manual notes are supplementary, not executed evidence. | Map Account Mgmt, GET/POST/PUT account-types and Officer read permission. |
| FR-16 | Locked-user listing/Admin unlock; High | Link TC-02B to FR-16 without removing FR-2/FR-13, plus denied-role coverage. | Map GET /users/locked and PATCH /users/{id}/unlock. |
| FR-17 | Admin read-only audit viewer, date/account filters and pagination; Medium | TC-16 covers financial audit/immutability only. Add viewer filter/pagination/read-role cases; audit_viewer_test.cpp is supplementary. | Map Audit Trail Module, GET /audit and audit.js; mutations remain 405. |

## Security ID alignment

SEC-1 authentication/RBAC, SEC-2 hashed credentials with scoped initial-credential handoff, SEC-3 network TLS and SEC-4 append-only financial audit retain SADS meanings. S-41 also associates TLS with SEC-2; retain that link and add SEC-3 explicitly. New SEC-5 identifies three-failure lockout/Admin recovery; SEC-6 identifies current-session logout, 30-minute fixed expiry and inactive-user rejection. SO-1/SO-2/SO-3 preserve S-34's three objectives with logout scope corrected.

All six SEC IDs have statements/acceptance criteria in SRS 5.3. Candidate links: SEC-3 TC-15; SEC-4 TC-16 plus viewer tests; SEC-5 TC-02/02B; SEC-6 TC-03/13 plus session tests. SEC-1/SEC-2 remain Not validated as in S-43; SEC-3 through SEC-6 have no independent S-43 verdict. Do not infer full acceptance from related historical passes.

## Baselines and disagreements

- Main: 1175a652edc433377dc2c87e2728e88948a08eb7.
- S-41 plan cc4b10200c531687aae83b855c5a59b246c25b37 is MERGED (#124). TC-03 expects 204/old-token 401; S-43's historical row quotes 200. TC-01 must use username, not user_id. Update SRS citation to v1.2, 6 Nov 2026.
- S-43 report 4cf8b3ca55d04a9d28bdcb0041d63d710ab619a8 is MERGED (#130). NFR-8 is Validated; all other existing FR/NFR/SEC IDs are Not validated. New IDs remain unassessed.
- S-46 #129 is OPEN. Reconcile Admin account-read permissions, absent /customers/{id}/accounts route, default HTTP/optional TLS, one-time credentials, current-token-only logout, and optional/logical schema relationships.
- TC-08/09 numeric preconditions differ from seeds and require explicit fixtures. Legacy standalone TC numbering is not canonical Sprint 3 numbering.
- ADR 003 interest basis remains provisional; do not invent owner approval.
- Original pre-commit resulting-balance preview, blanket minimum-balance wording and no-record-mutation-on-rejection wording are qualified visibly in SRS 3.1, NFR-3 and NFR-7, and DEVIATIONS.md row 34. These reconciliations require owner review.
- Merged main already resolves the duplicate row 28 and uses unique rows 28-32 for S-41/S-43/S-45. S-44 follows with rows 33-34; all SRS/handoff references are updated.

TC-21 remains NOT RUN, TC-22 PARTIAL, fresh-seed reruns pending, overall S-43 exit NOT MET. This adds coverage obligations, not invented results.
