# S-15 account opening manual test

Use a disposable local database so the seeded account and transaction history remains available to other tests.

1. Build and create a fresh seeded database:

   ```powershell
   cmake --build build
   sqlite3 .\build\s15-manual.sqlite ".read db\schema.sql"
   sqlite3 .\build\s15-manual.sqlite ".read db\seed.sql"
   ```

2. Start the server and open `http://127.0.0.1:8080`:

   ```powershell
   .\build\bin\bms_server.exe --host 127.0.0.1 --port 8080 --db .\build\s15-manual.sqlite --frontend .\frontend
   ```

3. Sign in as `officer1` with `Password123!`. Confirm **Open account** is in the staff navigation. Open it and verify the account-type list shows each type's minimum balance. Select Savings and enter customer `CUS-00000001` with a deposit of `INR 1,000.00`.
4. Choose **Review account**. Confirm customer ID, account type, entered deposit and required minimum are shown. Choose **Back** and verify the original values remain. Review again and choose **Confirm and open account**.
5. Verify the success message shows a new account number, Active status, INR 1,000.00 balance and opening transaction reference. Open the customer's details and verify the new account is in the account table.
6. Attempt to open Savings for `CUS-00000001` with `INR 999.99`. Confirm the request is rejected and the displayed error states the required minimum of INR 1,000.00.
7. Enter an unknown customer code and submit a valid deposit. Confirm a clear not-found error appears without clearing the entered values. Then use an inactive customer code (deactivate a test customer first) and confirm the inactive status is clearly reported.
8. Sign in as `customer1`. Confirm **Open account** is absent from navigation and no account-opening action is available on customer details.
9. With SQLite, verify the newly created account has one opening `deposit` transaction with the same amount and resulting balance, and one `ACCOUNT_OPENED` audit entry. Failed attempts must not add account or transaction rows.

Expected: only Officers and Administrators can open an account. Account creation, opening deposit and audit entry are atomic.
