(() => {
  const card = document.querySelector("#account-opening-card");
  const form = document.querySelector("#account-opening-form");
  const customerId = document.querySelector("#account-opening-customer-id");
  const accountType = document.querySelector("#account-opening-type");
  const initialDeposit = document.querySelector("#account-opening-deposit");
  const reviewButton = document.querySelector("#account-opening-review");
  const confirmation = document.querySelector("#account-opening-confirmation");
  const summary = document.querySelector("#account-opening-summary");
  const confirmButton = document.querySelector("#account-opening-confirm");
  const backButton = document.querySelector("#account-opening-back");
  const status = document.querySelector("#account-opening-status");
  const minimumHint = document.querySelector("#account-opening-minimum");
  const fieldIds = {
    customerId: "account-opening-customer-id",
    accountType: "account-opening-type",
    initialDeposit: "account-opening-deposit",
  };
  let role = "";
  let types = [];

  function clearFieldErrors() {
    for (const id of Object.values(fieldIds)) {
      const input = document.getElementById(id);
      input.removeAttribute("aria-invalid");
      document.getElementById(`${id}-error`).textContent = "";
    }
  }

  function showFieldErrors(fields) {
    let firstInput = null;
    for (const error of fields) {
      const id = fieldIds[error.field];
      if (!id) continue;
      const input = document.getElementById(id);
      input.setAttribute("aria-invalid", "true");
      document.getElementById(`${id}-error`).textContent = error.message;
      if (!firstInput) firstInput = input;
    }
    firstInput?.focus();
    return Boolean(firstInput);
  }

  function selectedType() {
    return types.find((type) => String(type.typeId) === accountType.value) || null;
  }

  function updateMinimumHint() {
    const type = selectedType();
    minimumHint.textContent = type
      ? `Minimum balance for ${type.name}: INR ${type.minimumBalance}.`
      : "Select an account type to see its minimum balance.";
  }

  function appendSummary(label, value) {
    const term = document.createElement("dt");
    term.textContent = label;
    const description = document.createElement("dd");
    description.textContent = value;
    summary.append(term, description);
  }

  function showConfirmation() {
    const type = selectedType();
    summary.replaceChildren();
    appendSummary("Customer ID", customerId.value.trim());
    appendSummary("Account type", type.name);
    appendSummary("Initial deposit", `INR ${initialDeposit.value.trim()}`);
    appendSummary("Required minimum", `INR ${type.minimumBalance}`);
    form.hidden = true;
    confirmation.hidden = false;
    confirmButton.focus();
  }

  async function loadTypes() {
    status.textContent = "Loading account types…";
    status.className = "result";
    try {
      const response = await window.BmsApi.get("/account-types");
      types = response.accountTypes;
      accountType.replaceChildren(new Option("Select an account type", ""));
      for (const type of types) {
        const option = new Option(
          `${type.name} (minimum INR ${type.minimumBalance})`,
          String(type.typeId),
        );
        accountType.add(option);
      }
      updateMinimumHint();
      status.textContent = "";
    } catch (error) {
      status.textContent = error.message || "Account types could not be loaded.";
      status.className = "result error";
    }
  }

  function requestReview() {
    clearFieldErrors();
    status.textContent = "";
    status.className = "result";
    status.setAttribute("role", "status");
    const errors = [];
    if (!customerId.value.trim()) {
      errors.push({ field: "customerId", message: "Customer ID is required." });
    }
    if (!selectedType()) {
      errors.push({ field: "accountType", message: "Select an account type." });
    }
    if (!/^(0|[1-9][0-9]*)(\.[0-9]{1,2})?$/.test(initialDeposit.value.trim())) {
      errors.push({
        field: "initialDeposit",
        message: "Enter a non-negative amount with up to two decimal places.",
      });
    }
    if (showFieldErrors(errors)) return;
    showConfirmation();
  }

  async function openAccount() {
    clearFieldErrors();
    status.textContent = "Opening account…";
    status.className = "result";
    confirmButton.disabled = true;
    confirmButton.textContent = "Opening…";
    const submittedCustomerId = customerId.value.trim();
    try {
      const response = await window.BmsApi.post("/accounts", {
        customerId: submittedCustomerId,
        accountType: Number(accountType.value),
        initialDeposit: initialDeposit.value.trim(),
      });
      status.textContent =
        `Account ${response.account.accountNumber} opened successfully. ` +
        `Status: ${response.account.status}. Balance: INR ${response.account.balance}. ` +
        `Opening transaction reference: ${response.transaction.referenceNumber}.`;
      status.className = "result success";
      status.setAttribute("role", "status");
      confirmation.hidden = true;
      form.hidden = false;
      form.reset();
      updateMinimumHint();
      document.dispatchEvent(new CustomEvent("bms:account-opened", {
        detail: {
          customerId: submittedCustomerId,
          accountNumber: response.account.accountNumber,
        },
      }));
    } catch (error) {
      confirmation.hidden = true;
      form.hidden = false;
      const fields = error.fields || [];
      if (error.minimumBalance) {
        fields.push({
          field: "initialDeposit",
          message: `The initial deposit must be at least INR ${error.minimumBalance}.`,
        });
      }
      const hasFieldErrors = showFieldErrors(fields);
      if (error.code === "CUSTOMER_INACTIVE") {
        status.textContent = error.message ||
          "This customer is inactive and cannot open a new account.";
      } else if (error.status === 404 && error.message?.includes("Customer")) {
        status.textContent = error.message;
      } else if (error.minimumBalance) {
        status.textContent =
          `The deposit is below the required minimum of INR ${error.minimumBalance}.`;
      } else {
        status.textContent = error.message || "The account could not be opened.";
      }
      status.className = "result error";
      status.setAttribute("role", hasFieldErrors ? "status" : "alert");
    } finally {
      confirmButton.disabled = false;
      confirmButton.textContent = "Confirm and open account";
    }
  }

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    requestReview();
  });
  accountType.addEventListener("change", updateMinimumHint);
  confirmButton.addEventListener("click", () => { void openAccount(); });
  backButton.addEventListener("click", () => {
    confirmation.hidden = true;
    form.hidden = false;
    status.textContent = "";
    status.setAttribute("role", "status");
    reviewButton.focus();
  });

  document.addEventListener("bms:open-account-for-customer", (event) => {
    customerId.value = event.detail.customerId;
    card.hidden = false;
    card.scrollIntoView({ behavior: "smooth", block: "start" });
    customerId.focus();
  });

  document.addEventListener("bms:session", (event) => {
    role = event.detail.role;
    const staff = Boolean(event.detail.accessToken) &&
      !event.detail.mustChangePassword &&
      (role === "administrator" || role === "bank_officer");
    card.hidden = !staff;
    if (staff) {
      void loadTypes();
    } else {
      types = [];
      accountType.replaceChildren(new Option("Select an account type", ""));
      confirmation.hidden = true;
      form.hidden = false;
      form.reset();
      status.textContent = "";
    }
  });

  document.addEventListener("bms:session-expired", () => {
    card.hidden = true;
    types = [];
    confirmation.hidden = true;
    form.hidden = false;
  });
})();
