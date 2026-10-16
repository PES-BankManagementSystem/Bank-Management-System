# Deviations log

Every place where the implementation differs from the SRS v1.0, the SADS v1.0 or the Test Plan is listed here, with the reason. The log is reviewed at the end of each sprint (S-39).

| # | Affected document and section | Deviation | Reason | Story |
|---|---|---|---|---|
| 1 | SADS 3.3 (Error Handling, concurrency conflicts) | SQLite does not have row-level locking. Balance-changing operations take a database-wide write lock (`BEGIN IMMEDIATE`) instead, so concurrent writes run one at a time. | SQLite was chosen as the relational database ([ADR 001](decisions/001-tech-stack.md)). SRS Appendix D lists record-level locking as future work, and serialised writes give the same guarantee that no update is lost. | S-01 |
