# Input Validation Audit (#64)

## Before changes

Reviewed all 30 registered API method/path combinations on `origin/main`,
including explicit read-only audit methods. The static frontend mount is not an
API endpoint. Existing errors use `{ "code", "message", "field"? }` with
optional `fields` for multiple validation errors. SQL values are bound through
prepared statements. Router handlers map typed domain errors to JSON; unexpected
exceptions are logged server-side and return a generic `INTERNAL_ERROR` body.

| Method | Path | Inputs (path, query, JSON body, headers) | Validation before changes | Gaps found before changes | Fix applied |
|---|---|---|---|---|---|
| GET | `/health` | None | No caller input. | None. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/account-types` | Authorization | Auth and role. | None. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/account-types` | Authorization; `name`, `minimumBalance`, `interestRate`, `dailyWithdrawalLimit` | JSON types, allowed keys, decimal precision/range, core trim and validation, uniqueness. | Whitespace name rejected only in core. | Existing input and authorization checks retained; no additional change needed. |
| PUT | `/account-types/:id` | Authorization; path `id`; editable account-type fields | Positive ID via `from_chars`; field and core validation. | None material. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/customers` | Authorization; query `query`, `limit`, `offset` | Auth/role, pagination bounds, prepared search query. | Search query lacked a length bound. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/customers/me` | Authorization | Auth and customer role. | None. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/customers/:id` | Authorization; path `id` | Auth, role, ownership, prepared customer lookup. | Identifier had no explicit length/character bound. | Existing input and authorization checks retained; no additional change needed. |
| PUT | `/customers/:id` | Authorization; path `id`; JSON customer fields or `status` | Auth/permission, allowed fields/types, core field validation, status enum. | Path identifier had no explicit syntax/length check. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/customers` | Authorization; JSON customer fields | Auth, field types/keys, core lengths, dates, phone/email and uniqueness; transaction. | Text trimming and character policies are field-specific. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/audit` | Authorization; query `from`, `to`, `limit`, `offset`, `account` | Admin auth, date/order and pagination checks, prepared account filter/query. | Account filter lacked identifier validation. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/audit` | None | Explicit 405 read-only response. | None; no write occurs. | Existing input and authorization checks retained; no additional change needed. |
| PUT | `/audit` | None | Explicit 405 read-only response. | None; no write occurs. | Existing input and authorization checks retained; no additional change needed. |
| PATCH | `/audit` | None | Explicit 405 read-only response. | None; no write occurs. | Existing input and authorization checks retained; no additional change needed. |
| DELETE | `/audit` | None | Explicit 405 read-only response. | None; no write occurs. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/audit/:id` | Path `id` (unused) | Explicit 405 read-only response. | Unused identifier is not parsed. | Existing input and authorization checks retained; no additional change needed. |
| PUT | `/audit/:id` | Path `id` (unused) | Explicit 405 read-only response. | Unused identifier is not parsed. | Existing input and authorization checks retained; no additional change needed. |
| PATCH | `/audit/:id` | Path `id` (unused) | Explicit 405 read-only response. | Unused identifier is not parsed. | Existing input and authorization checks retained; no additional change needed. |
| DELETE | `/audit/:id` | Path `id` (unused) | Explicit 405 read-only response. | Unused identifier is not parsed. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/accounts/:acc/statement` | Authorization; path `acc`; query `from`, `to` | Auth/ownership, required date strings and core date/order validation, prepared queries. | Account identifier only checked for empty string in core; unsafe tokens were not rejected. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/interest/run` | Authorization; JSON `period` | Admin auth, string type, core calendar validation and transaction. | Whitespace-only values reached core. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/auth/login` | JSON `username`, `password`; listener policy | Object/types, required nonempty values, length caps, generic 401, prepared lookup. | Whitespace-only values intentionally returned generic 401. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/auth/logout` | Authorization bearer token; listener policy | Nonempty bearer token and idempotent session removal. | No token length cap. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/auth/change-password` | Authorization; JSON `newPassword`; listener policy | Auth, string and 12–1024 length, password-change state. | Whitespace-only passwords passed if long enough. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/users/locked` | Authorization | Admin role. | None. | Existing input and authorization checks retained; no additional change needed. |
| PATCH | `/users/:id/unlock` | Authorization; path `id` | Admin role; digits, max 18 chars; unknown user 404. | No overflow gap: 18 decimal digits fit `int64_t`. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/transactions` | Authorization; JSON `type`, `account_number`, `amount` | Auth/operation, required string types, type enum, core decimal positivity/precision/overflow, account lookup/state and atomic persistence. | API did not parse the amount; whitespace account names and unsafe account tokens reached core/database lookup. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/transfers` | Authorization; JSON `from_account_number`, `to_account_number`, `amount` | Auth/role, required string types, core amount checks, distinct/existing accounts, transaction rollback. | API did not parse amount; unsafe account tokens reached database lookup. | Existing input and authorization checks retained; no additional change needed. |
| POST | `/staff` | Authorization; JSON `username`; listener policy | Admin role, allowed key, username syntax/length, uniqueness and transaction. | None material. | Existing input and authorization checks retained; no additional change needed. |
| GET | `/staff` | Authorization | Admin role. | None. | Existing input and authorization checks retained; no additional change needed. |
| PATCH | `/staff/:id` | Authorization; path `id`; JSON `username` and/or `status` | Admin role, allowed fields, username/status rules, transaction; max 18 digit ID fits `int64_t`. | None material. | Existing input and authorization checks retained; no additional change needed. |
## After changes

All 30 registered routes were re-reviewed. Customer search now caps `query` at
100 bytes at the API and core boundaries. Password changes reject whitespace-only
passwords while preserving valid passwords that contain spaces. Shared account-number validation is
now applied at the API boundary for transactions, transfers, statements and
audit filters, and again in deposit, withdrawal, transfer and statement core
functions. It rejects empty, whitespace, overlong, non-ASCII and punctuation
identifiers before lookup; valid-shaped unknown values retain the existing
structured 404. Both transaction routes parse decimal strings before invoking
the service, which repeats the same exact integer-paise parser. Existing JSON
parsing, field/type checks, typed errors, parameterized SQL, atomic rollback,
and generic logged 500 handling remain in place. New seeded tests directly call
core functions and fuzz the deposit, withdrawal and transfer HTTP endpoints
with 300 deterministic amount cases and 300 account-identifier cases per
operation (1,800 requests total), plus missing-field, unknown-account,
same-account and malformed-JSON cases. They assert transaction state is
unchanged. The existing API server test already verifies generic 500 behavior.

Remaining constraints: staff/customer identifier and password policies remain
as already established by their own modules; login keeps generic 401 responses
to avoid username disclosure. Account-number syntax is not prescribed in the
source requirements and the chosen boundary rule is recorded in
[`DEVIATIONS.md`](DEVIATIONS.md).
