((root, factory) => {
  const api = factory();
  root.BmsStatementCsv = api;
  if (typeof module !== "undefined" && module.exports) module.exports = api;
})(typeof globalThis !== "undefined" ? globalThis : this, () => {
  function csvField(value) {
    const text = String(value);
    const safe = /^[=+\-@]/.test(text) ? `'${text}` : text;
    return `"${safe.replaceAll('"', '""')}"`;
  }

  function toCsv(statement) {
    const rows = [
      ["Account", statement.account_number],
      ["From", statement.from],
      ["To", statement.to],
      ["Opening balance (INR)", statement.opening_balance],
      [],
      ["Date and time", "Type", "Amount (INR)", "Resulting balance (INR)"],
      ...statement.transactions.map((transaction) => [
        transaction.created_at,
        transaction.type,
        transaction.amount,
        transaction.resulting_balance,
      ]),
      ["Closing balance (INR)", statement.closing_balance],
    ];
    return rows.map((row) => row.map(csvField).join(",")).join("\r\n");
  }

  return { toCsv };
});
