# S-16 account status manual test

Use a disposable local database so the seeded account history remains available for other tests.

1. Build and create a fresh seeded database:

   ```powershell
   cmake --build build
   sqlite3 .\build\s16-manual.sqlite ".read db\schema.sql"
   sqlite3 .\build\s16-manual.sqlite ".read db\seed.sql"
   ```

2. Record account and history counts, then set the active test account's balance to zero so closure can be exercised (the seeded transaction history is intentionally retained):

   ```powershell
   sqlite3 .\build\s16-manual.sqlite "SELECT account_number,status,balance FROM accounts;"
   sqlite3 .\build\s16-manual.sqlite "SELECT COUNT(*) FROM transactions;"
   sqlite3 .\build\s16-manual.sqlite "UPDATE accounts SET balance=0 WHERE account_number='ACC001';"
   ```

3. Start the server and open `http://127.0.0.1:8080`:

   ```powershell
   .\build\bin\bms_server.exe --host 127.0.0.1 --port 8080 --db .\build\s16-manual.sqlite --frontend .\frontend
   ```

4. Sign in as `admin` with `Password123!`. Open **Customers**, view `CUS-00000001`, and confirm ACC001 has an **Active** badge and **Freeze** / **Close account** actions. Freeze it and confirm the dialog; verify the badge refreshes to **Frozen** and **Unfreeze** / **Close account** actions appear.
5. On the deposit screen, attempt a deposit to ACC001. Confirm the friendly rejection identifies the frozen account and says no balance/history changed. Verify the balance and transaction count are unchanged in SQLite.
6. Unfreeze ACC001 and confirm it returns to **Active**. Freeze it again, close it (confirm the dialog), and confirm the badge shows **Closed** with no actions. Verify the account and transaction rows remain.
7. With an active non-zero account, attempt **Close account**. Confirm the inline error asks to withdraw or transfer the remaining balance first and shows the balance; the detail page stays open and the account remains active.
8. Sign in as `customer1`. Confirm **My profile** shows the statuses of ACC001/ACC004 and offers no account-status action buttons. The closed account remains viewable with its existing balance/history.

Expected: status changes update the retained account row and append audit entries; rejected close/transaction requests do not change balances, transactions or status.
