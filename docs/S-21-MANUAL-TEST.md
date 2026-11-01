# S-21 transaction history manual test

Use a disposable local database so the seeded account and transaction history
remains unchanged.

1. Build and create a fresh database:

   ```powershell
   cmake --build build
   sqlite3 .\build\s21-manual.sqlite ".read db\schema.sql"
   sqlite3 .\build\s21-manual.sqlite ".read db\seed.sql"
   ```

2. Start the server and open `http://127.0.0.1:8080`:

   ```powershell
   .\build\bin\bms_server.exe --host 127.0.0.1 --port 8080 --db .\build\s21-manual.sqlite --frontend .\frontend
   ```

3. Sign in as `customer1` with `Password123!`, select **History**, and choose
   `ACC001`. Confirm transactions are newest first, debits have a minus sign
   and a Debit label, the displayed timestamp's title contains its ISO UTC
   value, and transfer rows show the counterpart account. Try a one-day range,
   then Clear; confirm the empty state for a date range without transactions.
   Try a `From` date after `To` and confirm the inline error receives focus.

4. Sign in as `customer2`, open **History**, and select frozen account `ACC002`.
   Confirm its transactions remain visible. Repeat as `customer3` for closed
   account `ACC003`.

5. Sign in as `officer1`. Open **History**, enter `ACC001`, apply the filters,
   and confirm the Officer can view any account's history. Use `ACC999` and
   confirm a clear not-found error.

6. To check authorization and audit behavior as `customer2`, request the other
   customer's account through DevTools Console:

   ```javascript
   const token = (await fetch("/auth/login", {
     method: "POST",
     headers: { "Content-Type": "application/json" },
     body: JSON.stringify({ username: "customer2", password: "Password123!" })
   }).then((response) => response.json())).accessToken;
   await fetch("/accounts/ACC001/transactions", {
     headers: { Authorization: `Bearer ${token}` }
   }).then(async (response) => console.log(response.status, await response.json()));
   ```

   Confirm the response is 403 and has the same body as a request for
   `/accounts/ACC999/transactions`. In the Administrator **Audit trail**, filter
   by `ACC001` and verify a `VIEW_TRANSACTIONS_DENIED` entry identifies the
   attempted account in its details. Do not include the bearer token in a
   screenshot or share it.

7. For the empty-history state, request a future date range such as
   `2030-01-01` through `2030-01-02`; confirm the page says
   “No transactions in this period”. Simulate a network failure or use an
   invalid account number to check the error state.

Expected: Customers can read only their own accounts; Officers and
Administrators can read any account; denied Customer requests are recorded
without changing the 403 response; filtering and pagination do not alter
stored transactions.
