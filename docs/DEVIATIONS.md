# Deviations log

Every place where the implementation differs from the SRS v1.0, the SADS v1.0 or the Test Plan is listed here, with the reason. The log is reviewed at the end of each sprint (S-39).

| # | Affected document and section | Deviation | Reason | Story |
|---|---|---|---|---|
| 1 | SADS 3.3 (Error Handling, concurrency conflicts) | SQLite does not have row-level locking. Balance-changing operations take a database-wide write lock (`BEGIN IMMEDIATE`) instead, so concurrent writes run one at a time. | SQLite was chosen as the relational database ([ADR 001](decisions/001-tech-stack.md)). SRS Appendix D lists record-level locking as future work, and serialised writes give the same guarantee that no update is lost. | S-01 |
| 2 | SADS 2.4 / SEC-1 (authenticated operator identity) | The S-17 deposit route records seeded Bank Officer user ID `2` as the operator because authentication and session management are not implemented yet. The operator ID is server-side and cannot be supplied by the client. | This keeps the deposit's audit and transaction records complete until S-07 provides a verified session identity. The unauthenticated demo route must not be used in a production deployment. | S-17 (#44) |
| 3 | SRS Appendix C TBD-5 (interest basis) | TBD-5 provisional resolution: interest uses each account's month-end UTC balance, actual days in that calendar year, and half-up rounding to paise. Completed `YYYY-MM` periods only; a zero-rounded credit is skipped. Historical opening balance is inferred from the transaction ledger because the schema has no opening-balance column. | S-22 needs a concrete reproducible calculation before the repo owner confirms the rule. See [ADR 003](decisions/003-interest-calculation-basis.md). | S-22 (#54) |
