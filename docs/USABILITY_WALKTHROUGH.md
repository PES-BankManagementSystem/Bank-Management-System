# TC-21 Usability Walkthrough

**Status: NOT RUN - documented deviation 35.**

This is the unused collection protocol and non-execution record for S-43 (#119), Test Plan
v1.2 TC-21 (NFR-6). It is not a completed usability study. The general UI feedback
from three external users referenced in S-32 (#63) does not qualify: it did not
record first-time eligibility, both required tasks, first-attempt unaided outcomes,
or task-level observations. The deposit and history workflows were also unavailable
on `main` at that session's time, according to the clarification supplied on #119.
No historical feedback or automated browser run is counted as TC-21 completion.

## Disposition recorded on 2026-11-07

At the user's request, TC-21 is recorded as not run under
[deviation 35](DEVIATIONS.md#tc-21-disposition-deviation-35), rather than as a
pending prerequisite to administrative closure of S-43. No participants were
enrolled and no session took place. P1/P2/P3 below are proposed anonymous IDs,
not actual participant records. The #63 session is not substituted as evidence.
NFR-6 remains **not validated**, and the original all-cases-executed exit
criterion remains **not met**. The protocol below is retained for any future
qualifying study. No usability success or repo-owner approval is claimed.

## Session prerequisites (future study)

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
| Date and moderator | Not recorded (session not run) |
| Tested build SHA | Not recorded (session not run) — record actual participant build, not an earlier API build |
| OS / browser and version | Not recorded (session not run) |
| Schema/seed revision and fixture adjustments | Not recorded (session not run) |
| Task date range and expected results | Not recorded (session not run) |

| Participant | First-time / no training confirmed | Consent recorded | Fresh database reference |
|---|---|---|---|
| P1 | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P2 | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P3 | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |

| Participant | Task | First attempt completed | Unaided / prompts or help | Time | Observations and result evidence |
|---|---|---|---|---|---|
| P1 | Officer deposit | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P1 | Customer history | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P2 | Officer deposit | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P2 | Customer history | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P3 | Officer deposit | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |
| P3 | Customer history | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) | Not recorded (session not run) |

## Acceptance and residual risk

A future TC-21 pass still requires all six task attempts to succeed unaided on
the first attempt (3/3 participants complete both tasks; 6/6 task successes).
No recorded values exist for this run, so no completion rate can be calculated.
First-time discoverability and unaided task completion remain unmeasured.

The requested S-43 closure records this omission under deviation 35; it does not
change the Test Plan's pass criteria or turn non-execution into a pass. If a study
is later conducted, record its actual build, observations and evidence here and
update TC-21/NFR-6 in the validation report. Release readiness and the other S-46
conditions remain separate decisions; no release is performed by this change.
