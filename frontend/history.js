(() => {
  const card = document.querySelector("#history-card");
  const form = document.querySelector("#history-form");
  const accountSelect = document.querySelector("#history-account");
  const accountInput = document.querySelector("#history-account-number");
  const fromInput = document.querySelector("#history-from");
  const toInput = document.querySelector("#history-to");
  const applyButton = document.querySelector("#history-apply");
  const result = document.querySelector("#history-result");
  const entries = document.querySelector("#history-entries");
  const empty = document.querySelector("#history-empty");
  const newer = document.querySelector("#history-newer");
  const older = document.querySelector("#history-older");
  const pageStatus = document.querySelector("#history-page-status");
  const pageSize = 20;

  let token = "";
  let role = "";
  let offset = 0;
  let total = 0;
  let snapshot = null;
  let loading = false;

  function accountNumber() {
    return role === "customer" ? accountSelect.value.trim() : accountInput.value.trim();
  }

  function setFieldError(field, message) {
    const input = {
      accountNumber: accountInput,
      account: accountSelect,
      from: fromInput,
      to: toInput,
    }[field];
    if (!input) return;
    input.setAttribute("aria-invalid", "true");
    document.querySelector(`#history-${field === "accountNumber" || field === "account" ? "account" : field}-error`)
      .textContent = message;
  }

  function clearFieldErrors() {
    for (const [field, input] of Object.entries({
      account: accountSelect, accountNumber: accountInput, from: fromInput, to: toInput,
    })) {
      input.removeAttribute("aria-invalid");
      document.querySelector(`#history-${field === "accountNumber" ? "account" : field}-error`)
        .textContent = "";
    }
  }

  function showFieldErrors(errors) {
    clearFieldErrors();
    for (const error of errors) setFieldError(error.field, error.message);
    const first = errors.find((error) =>
      ["accountNumber", "account", "from", "to"].includes(error.field));
    if (first) {
      const target = first.field === "accountNumber" ? accountInput :
        first.field === "account" ? accountSelect :
          first.field === "from" ? fromInput : toInput;
      target.focus();
    }
  }

  function displayType(type) {
    if (type === "transfer_credit") return "Transfer in";
    if (type === "transfer_debit") return "Transfer out";
    return type.charAt(0).toUpperCase() + type.slice(1);
  }

  function appendCell(row, value) {
    const cell = document.createElement("td");
    cell.textContent = value == null ? "" : String(value);
    row.append(cell);
    return cell;
  }

  function renderRows(transactions) {
    entries.replaceChildren();
    for (const item of transactions) {
      const row = document.createElement("tr");
      const time = document.createElement("time");
      time.dateTime = item.dateTime;
      time.title = item.dateTime;
      const parsed = new Date(item.dateTime);
      time.textContent = Number.isNaN(parsed.getTime())
        ? item.dateTime
        : parsed.toLocaleString();
      const timeCell = document.createElement("td");
      timeCell.append(time);
      row.append(timeCell);

      const type = displayType(item.type);
      appendCell(row, item.counterpartAccount
        ? `${type} (${item.counterpartAccount})`
        : type);
      const direction = item.amount.startsWith("-") ? "Debit" : "Credit";
      appendCell(row, `${direction}: ${item.amount}`);
      appendCell(row, item.balanceAfter);
      entries.append(row);
    }
  }

  function updatePager() {
    const first = total === 0 ? 0 : offset + 1;
    const last = Math.min(offset + pageSize, total);
    pageStatus.textContent = `${first}-${last} of ${total}`;
    newer.disabled = loading || offset === 0;
    older.disabled = loading || offset + pageSize >= total;
  }

  async function loadHistory() {
    const account = accountNumber();
    if (!account) {
      result.classList.add("error");
      result.textContent = role === "customer"
        ? "Choose an account to view its history."
        : "Enter an account number to view its history.";
      entries.replaceChildren();
      empty.hidden = true;
      total = 0;
      updatePager();
      return;
    }

    loading = true;
    applyButton.disabled = true;
    result.classList.remove("error");
    result.textContent = "Loading transactions...";
    empty.hidden = true;
    entries.replaceChildren();
    updatePager();

    const params = new URLSearchParams({ limit: String(pageSize), offset: String(offset) });
    if (snapshot !== null) params.set("snapshot", String(snapshot));
    if (fromInput.value) params.set("from", fromInput.value);
    if (toInput.value) params.set("to", toInput.value);
    try {
      const response = await window.BmsApi.get(
        `/accounts/${encodeURIComponent(account)}/transactions?${params.toString()}`);
      total = response.total;
      snapshot = response.snapshot;
      renderRows(response.transactions);
      result.textContent = "";
      empty.hidden = response.transactions.length !== 0;
    } catch (error) {
      result.classList.add("error");
      result.textContent = error.message || "Unable to load transaction history.";
      entries.replaceChildren();
      total = 0;
      empty.hidden = true;
      if (error.fields?.length) showFieldErrors(error.fields);
      else if (error.field) showFieldErrors([{ field: error.field, message: error.message }]);
    } finally {
      loading = false;
      applyButton.disabled = false;
      updatePager();
    }
  }

  async function loadCustomerAccounts() {
    accountSelect.replaceChildren(new Option("Choose an account", ""));
    if (!token || role !== "customer") return;
    try {
      const response = await window.BmsApi.get("/customers/me/accounts");
      for (const account of response.accounts) {
        const option = new Option(
          `${account.accountNumber} - ${account.accountType} (${account.status})`,
          account.accountNumber);
        accountSelect.add(option);
      }
    } catch (error) {
      result.classList.add("error");
      result.textContent = error.message || "Unable to load your accounts.";
    }
  }

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    clearFieldErrors();
    result.classList.remove("error");
    if (fromInput.value && toInput.value && fromInput.value > toInput.value) {
      const message = "From must not be after To.";
      setFieldError("from", message);
      fromInput.focus();
      return;
    }
    offset = 0;
    snapshot = null;
    void loadHistory();
  });

  document.querySelector("#history-clear").addEventListener("click", () => {
    fromInput.value = "";
    toInput.value = "";
    clearFieldErrors();
    offset = 0;
    snapshot = null;
    void loadHistory();
  });

  newer.addEventListener("click", () => {
    offset = Math.max(0, offset - pageSize);
    void loadHistory();
  });
  older.addEventListener("click", () => {
    if (offset + pageSize < total) {
      offset += pageSize;
      void loadHistory();
    }
  });
  accountSelect.addEventListener("change", () => {
    offset = 0;
    snapshot = null;
    void loadHistory();
  });

  document.addEventListener("bms:session", (event) => {
    token = event.detail.accessToken || "";
    role = event.detail.role || "";
    const customer = token && role === "customer" && !event.detail.mustChangePassword;
    const staff = token && ["administrator", "bank_officer"].includes(role) &&
      !event.detail.mustChangePassword;
    card.hidden = !customer && !staff;
    accountSelect.hidden = !customer;
    accountInput.hidden = !staff;
    document.querySelector("#history-account-label").hidden = !customer && !staff;
    if (customer) {
      void loadCustomerAccounts();
    } else {
      accountSelect.replaceChildren(new Option("Choose an account", ""));
      accountInput.value = "";
    }
    entries.replaceChildren();
    result.textContent = "";
    total = 0;
    offset = 0;
    snapshot = null;
    updatePager();
  });
})();
