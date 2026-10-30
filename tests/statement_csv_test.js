const assert = require("node:assert/strict");
const { toCsv } = require("../frontend/statement_csv.js");

const csv = toCsv({
  account_number: 'ACC,"001',
  from: "2026-06-01",
  to: "2026-06-01",
  opening_balance: "10.00",
  transactions: [
    {
      created_at: "2026-06-01 23:59:59",
      type: 'deposit, "special"',
      amount: "5.00",
      resulting_balance: "15.00",
    },
    {
      created_at: "=HYPERLINK(\"https://bad\")",
      type: "+cmd",
      amount: "-1.00",
      resulting_balance: "@SUM(A1:A2)",
    },
  ],
  closing_balance: "14.00",
});

assert.equal(
  csv,
  [
    '"Account","ACC,""001"',
    '"From","2026-06-01"',
    '"To","2026-06-01"',
    '"Opening balance (INR)","10.00"',
    "",
    '"Date and time","Type","Amount (INR)","Resulting balance (INR)"',
    '"2026-06-01 23:59:59","deposit, ""special""","5.00","15.00"',
    '"\'=HYPERLINK(""https://bad"")","\'+cmd","\'-1.00","\'@SUM(A1:A2)"',
    '"Closing balance (INR)","14.00"',
  ].join("\r\n"),
);

console.log("Statement CSV escaping and formula neutralization passed.");
