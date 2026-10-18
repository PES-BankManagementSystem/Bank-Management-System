# ADR 002: Account Types and Withdrawal Limits

- **Status:** Accepted
- **Date:** 2026-10-18
- **Story:** S-03
- **Resolves:** SRS Appendix C TBD-2 (account types), TBD-3 (daily withdrawal limit)

## Context

The SRS leaves the specific account types and withdrawal limits open (TBD-2 and TBD-3) but requires:
- Different account types with varying minimum balances, interest rates, and withdrawal limits (SRS 4.2-4.7)
- Daily withdrawal limits that vary by account type and are counted per account per calendar day (SRS 5.5, FR-10)
- Interest calculations based on account type and balance (SRS 5.5, FR-11)

The implementation must support:
- Savings accounts with minimum balance and interest
- Current accounts with (typically) no minimum balance and no interest
- Configurable withdrawal limits per account type
- Proper validation of withdrawal attempts against daily limits

## Decision

We propose the following account types and withdrawal limits:

| Account Type | Minimum Balance | Interest Rate (p.a.) | Daily Withdrawal Limit |
|--------------|-----------------|----------------------|------------------------|
| Savings      | 1,000.00 INR    | 4.00%                | 50,000.00 INR          |
| Current      | 5,000.00 INR    | 0.00%                | 200,000.00 INR         |

### Rationale

1. **Savings Account**:
   - Minimum balance encourages saving behavior
   - Moderate interest rate (4.00%) attracts long-term deposits
   - Conservative withdrawal limit (50,000.00 INR) balances accessibility with security

2. **Current Account**:
   - Higher minimum balance (5,000.00 INR) reflects premium service expectations
   - No interest as funds are expected for frequent transactions
   - Higher withdrawal limit (200,000.00 INR) accommodates business needs

### Implementation Details

- Minimum balances and withdrawal limits are stored in INTEGER paise (e.g., 1,000.00 INR = 100000 paise)
- Interest rates are stored in basis points (e.g., 4.00% = 400 basis points) to avoid floating-point precision issues
- Daily withdrawal limits are enforced per account per calendar day using transaction timestamps
- The interest rate for Current accounts is set to 0.00% as these accounts are typically used for frequent transactions rather than savings

## Consequences

### Positive
- Clear differentiation between account types meets diverse customer needs
- Interest on savings encourages deposit growth
- Withdrawal limits provide security against fraudulent large-scale withdrawals
- Basis point storage ensures exact interest calculations
- Integer storage for monetary values prevents floating-point rounding errors

### Negative
- Higher minimum balance for Current accounts may deter some customers
- Fixed interest rates require manual updates when market rates change
- Daily limit calculations require careful timestamp handling to ensure calendar day accuracy

### Related Work
- TBD-5 (interest basis) is left for S-22 as it requires more detailed calculation methodology
- The implementation aligns with NFR-3 (ACID transactions) and FR-10/FR-11 (withdrawal limits and interest calculations)
