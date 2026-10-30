(() => {
  const card = document.querySelector("#statement-card");
  const form = document.querySelector("#statement-form");
  const submit = document.querySelector("#statement-submit");
  const status = document.querySelector("#statement-result");
  const output = document.querySelector("#statement-output");
  const entries = document.querySelector("#statement-entries");
  const emptyState = document.querySelector("#statement-empty-state");
  let currentStatement = null;

  function cell(row, value) {
    const element = document.createElement("td");
    element.textContent = value;
    row.append(element);
  }

  function render(statement) {
    currentStatement = statement;
    document.querySelector("#statement-period").textContent =
      `Statement from ${statement.from} to ${statement.to}`;
    document.querySelector("#statement-account-label").textContent = statement.account_number;
    document.querySelector("#statement-opening-balance").textContent = `₹${statement.opening_balance}`;
    document.querySelector("#statement-closing-balance").textContent = `₹${statement.closing_balance}`;
    entries.replaceChildren();
    for (const transaction of statement.transactions) {
      const row = document.createElement("tr");
      cell(row, transaction.created_at);
      cell(row, transaction.type.replaceAll("_", " "));
      cell(row, `₹${transaction.amount}`);
      cell(row, `₹${transaction.resulting_balance}`);
      entries.append(row);
    }
    emptyState.hidden = statement.transactions.length !== 0;
    output.hidden = false;
  }

  function downloadCsv() {
    if (!currentStatement) return;
    const csv = window.BmsStatementCsv.toCsv(currentStatement);
    const url = URL.createObjectURL(new Blob([csv], { type: "text/csv;charset=utf-8" }));
    const link = document.createElement("a");
    link.href = url;
    link.download = `statement-${currentStatement.account_number}-${currentStatement.from}-to-${currentStatement.to}.csv`;
    document.body.append(link);
    link.click();
    link.remove();
    window.setTimeout(() => URL.revokeObjectURL(url), 1000);
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    status.textContent = "";
    status.className = "result";
    output.hidden = true;
    const account = form.elements.account_number.value.trim();
    const from = form.elements.from.value;
    const to = form.elements.to.value;
    const query = new URLSearchParams({ from, to });
    submit.disabled = true;
    try {
      const statement = await window.BmsApi.get(
        `/accounts/${encodeURIComponent(account)}/statement?${query}`);
      render(statement);
      status.textContent = statement.transactions.length
        ? "Statement ready."
        : "Statement ready. There were no transactions in this period.";
      status.className = "result success";
    } catch (error) {
      status.textContent = error.message || "The statement could not be loaded.";
      status.className = "result error";
    } finally {
      submit.disabled = false;
    }
  });

  document.querySelector("#statement-download").addEventListener("click", downloadCsv);
  document.querySelector("#statement-print").addEventListener("click", () => {
    document.body.classList.add("printing-statement");
    window.print();
  });
  window.addEventListener("afterprint", () => document.body.classList.remove("printing-statement"));

  document.addEventListener("bms:session", (event) => {
    const { accessToken, role, mustChangePassword } = event.detail;
    const visible = Boolean(accessToken) && !mustChangePassword &&
      ["customer", "bank_officer", "administrator"].includes(role);
    card.hidden = !visible;
    status.textContent = "";
    output.hidden = true;
    if (!visible) {
      form.reset();
      currentStatement = null;
    }
  });
})();
