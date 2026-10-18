# ADR 003: Interest Calculation Basis

- **Status:** Provisional
- **Date:** 2026-10-18
- **Story:** S-22 (#54)
- **Resolves:** SRS Appendix C TBD-5

## Context

The SRS and existing design records specify annual account-type rates but leave the balance basis for periodic interest to S-22. The schema stores balances in paise, rates in basis points, and each transaction's resulting balance and UTC timestamp.

## Provisional Decision

- Periods use `YYYY-MM` and represent a UTC calendar month. A run is accepted only after that month has ended, because its month-end balance is not final sooner.
- The basis is the balance at the end of the month's last UTC day, before the interest run's credit. It is the resulting balance of the last transaction on or before that instant.
- For a period with no transaction yet for an account, infer its opening balance from the earliest later transaction by reversing that transaction's signed amount from its resulting balance. If there are no transactions for the account, use its current balance as its unchanged opening balance. Accounts opened after the period end are skipped.
- Interest is `basis_balance_paise × annual_rate_basis_points × days_in_period / (10,000 × days_in_calendar_year)`. The denominator uses 365 or 366 days according to the period's calendar year.
- Round to the nearest paise, with exact half-paise values rounded up. Calculation uses integer arithmetic only.
- If the rounded amount is zero, skip the credit transaction and audit entry. A zero-credit period run is still recorded and cannot be repeated.

This decision is provisional and requires repo-owner confirmation. The `accounts` table has no opening-balance column, so the ledger-based inference above is used for historical periods.

## Consequences

- Month-end calculations are reproducible from the transaction ledger and do not use the account's present balance for a past month.
- A run cannot be started until the month is complete in UTC.
- If account opening funds were not represented in the ledger and later transaction history cannot establish them, the schema does not contain enough information to reconstruct the historical opening balance.
