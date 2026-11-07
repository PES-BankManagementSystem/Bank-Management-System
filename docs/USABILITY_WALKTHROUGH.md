# TC-21 Usability Walkthrough

**Status: NOT RUN — participant evidence is outstanding.**

This is the collection protocol and pending record for S-43 (#119), Test Plan
v1.2 TC-21 (NFR-6). It is not a completed usability study. The general UI feedback
from three external users referenced in S-32 (#63) does not qualify: it did not
record first-time eligibility, both required tasks, first-attempt unaided outcomes,
or task-level observations. The deposit and history workflows were also unavailable
on `main` at that session's time, according to the clarification supplied on #119.
No historical feedback or automated browser run is counted as TC-21 completion.

## Session prerequisites

- Recruit three participants who have never used this application and have had no
  prior training. Use anonymous IDs P1, P2 and P3; do not commit personal data.
- Record session date, moderator, full tested commit SHA, OS and browser version.
- Build that revision and verify Bank Officer deposit and Customer history work.
- Give each participant their own disposable database initialized from that
  revision's `db/schema.sql` and `db/seed.sql`. Record fixture adjustments.
- Use demo credentials and fictitious account data only. Record consent to taking
  anonymous observations; recording video is optional and requires consent.

## Neutral task cards

Give participants the task objective and credentials, without a demonstration,
navigation instructions, hints, or verbal prompts. Start with the sign-in screen.

1. **Bank Officer:** Using the supplied Bank Officer credentials, deposit INR
   5,000 into ACC001. Confirm that the deposit succeeded and identify its new
   balance. Use a declared INR 10,000 starting balance if aligning with TC-08;
   otherwise record the seed balance and the expected new balance explicitly.
2. **Customer:** Using the supplied Customer credentials, find ACC001's transaction
   history for the supplied date range and identify the deposit's amount and date.
   Choose the UTC date of the deposit so that the task has a known correct answer.

Observe silently. Record navigation errors, hesitations, completion time and the
participant's stated outcome. If the participant asks for help, record the request.
Any assistance, retry, or incomplete task prevents an unaided first-attempt pass;
record later assisted completion separately. Ask for feedback only after both
first attempts are over. Do not improve the instructions between participants
without declaring a new protocol/run.

## Session record

| Field | Value |
|---|---|
| Date and moderator | Pending |
| Tested build SHA | Pending — record actual participant build, not an earlier API build |
| OS / browser and version | Pending |
| Schema/seed revision and fixture adjustments | Pending |
| Task date range and expected results | Pending |

| Participant | First-time / no training confirmed | Consent recorded | Fresh database reference |
|---|---|---|---|
| P1 | Pending | Pending | Pending |
| P2 | Pending | Pending | Pending |
| P3 | Pending | Pending | Pending |

| Participant | Task | First attempt completed | Unaided / prompts or help | Time | Observations and result evidence |
|---|---|---|---|---|---|
| P1 | Officer deposit | Pending | Pending | Pending | Pending |
| P1 | Customer history | Pending | Pending | Pending | Pending |
| P2 | Officer deposit | Pending | Pending | Pending | Pending |
| P2 | Customer history | Pending | Pending | Pending | Pending |
| P3 | Officer deposit | Pending | Pending | Pending | Pending |
| P3 | Customer history | Pending | Pending | Pending | Pending |

## Acceptance and follow-up

TC-21 passes only if all six task attempts succeed unaided on the first attempt
(3/3 participants complete both tasks; 6/6 task successes). Pending entries are
not passes. Link anonymized observations and supporting ledger/history evidence,
record usability findings and follow-up defects, and update TC-21 and NFR-6 in
`System_Validation_Report.md`. Until then S-43 remains open and the dependent
S-46 (#122) `v1.0.0` tag/release remains on hold.
