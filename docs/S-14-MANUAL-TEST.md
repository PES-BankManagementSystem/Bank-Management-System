# S-14 account types manual test

Use a disposable local database. A fresh schema includes the v4 account-type constraints and the seed file supplies Savings and Current.

1. Build the project and create a fresh seeded database:

   ```powershell
   cmake --build build --config Debug
   sqlite3 .\build\s14-manual.sqlite ".read db/schema.sql"
   sqlite3 .\build\s14-manual.sqlite ".read db/seed.sql"
   ```

2. Start the server and open `http://127.0.0.1:8080`:

   ```powershell
   .\build\bin\bms_server.exe --host 127.0.0.1 --port 8080 --db .\build\s14-manual.sqlite --frontend .\frontend
   ```

3. Sign in as `admin` with `Password123!`. Confirm **Account types** is present in navigation and the page lists Savings and Current, with INR and `% p.a.` units. Confirm the note says changes apply to future operations only.
4. Add a type with a unique name, a minimum balance, an annual interest rate and a daily limit. Confirm the success message and the new table row. Try a duplicate name with different capitalization and confirm the inline name error.
5. Edit the new type. Confirm the same form is pre-filled, save different values and verify the row changes. Enter an invalid decimal (for example `1.001`) and verify the entered values remain and the corresponding inline field error appears.
6. Sign out and sign in as `officer1`. Confirm **Account types** and its navigation item are not shown. The officer may read `GET /account-types` through the API, but cannot create or edit types.
7. As an Administrator, verify a zero daily withdrawal limit is accepted and means no withdrawals. Changes to the type must not alter existing transaction rows or balances; the next withdrawal must use the updated type rules.

Expected: only Administrators see the configuration page; server-side authorization controls reads/writes independently of page visibility. A Bank Officer may list types via the API for the future account-opening flow.
