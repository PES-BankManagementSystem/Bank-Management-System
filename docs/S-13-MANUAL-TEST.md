# S-13 customer deactivation manual test

Use a disposable local database so the seeded customer remains available for other tests.

1. Build the server, then create and seed a fresh database:

   ```powershell
   cmake --build build
   sqlite3 .\build\s13-manual.sqlite ".read db/schema.sql"
   sqlite3 .\build\s13-manual.sqlite ".read db/seed.sql"
   ```

2. Record the seeded customer's history counts:

   ```powershell
   sqlite3 .\build\s13-manual.sqlite "SELECT COUNT(*) FROM accounts WHERE customer_id = 1;"
   sqlite3 .\build\s13-manual.sqlite "SELECT COUNT(*) FROM transactions WHERE account_id IN (SELECT account_id FROM accounts WHERE customer_id = 1);"
   ```

3. Start the server and open `http://127.0.0.1:8080`:

   ```powershell
   .\build\bin\bms_server.exe --host 127.0.0.1 --port 8080 --db .\build\s13-manual.sqlite --frontend .\frontend
   ```

4. Sign in as `admin` with the demo password `Password123!`. Open **Customers**, find `CUS-00000001`, and view the customer. Confirm the detail view shows an **Active** badge and the **Deactivate customer** action.
5. Select **Deactivate customer** and confirm the dialog. Confirm the detail view and refreshed search result both show **Inactive**, and that the accounts remain listed.
6. Sign out and try to sign in as `customer1` with `Password123!`. Sign-in should fail with the generic invalid-credentials message.
7. Sign in as `officer1` with `Password123!`. Search for the same customer and open the record. Confirm **Inactive** is shown, **Deactivate customer** is not present, and **Edit customer** remains available.
8. Recheck the two history counts from step 2 and verify the customer and login rows remain:

   ```powershell
   sqlite3 .\build\s13-manual.sqlite "SELECT customer_code, status FROM customers WHERE customer_id = 1;"
   sqlite3 .\build\s13-manual.sqlite "SELECT username, status FROM users WHERE user_id = 4;"
   sqlite3 .\build\s13-manual.sqlite "SELECT COUNT(*) FROM accounts WHERE customer_id = 1;"
   sqlite3 .\build\s13-manual.sqlite "SELECT COUNT(*) FROM transactions WHERE account_id IN (SELECT account_id FROM accounts WHERE customer_id = 1);"
   ```

Expected: customer and user rows remain with status `inactive`; both account and transaction counts match their values from step 2. Account opening is not implemented in this repository; the backend guard is covered by unit tests.
