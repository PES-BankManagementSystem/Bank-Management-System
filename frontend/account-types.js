(() => {
  const card = document.querySelector("#account-types-card");
  const form = document.querySelector("#account-type-form");
  const formHeading = document.querySelector("#account-type-form-heading");
  const submit = document.querySelector("#account-type-submit");
  const cancel = document.querySelector("#account-type-cancel");
  const formStatus = document.querySelector("#account-type-form-status");
  const listStatus = document.querySelector("#account-types-status");
  const list = document.querySelector("#account-types-list");
  const fieldIds = {
    name: "account-type-name",
    minimumBalance: "account-type-minimum",
    interestRate: "account-type-interest",
    dailyWithdrawalLimit: "account-type-limit",
  };
  let role = "";
  let accessToken = "";
  let editingTypeId = null;
  let loading = false;

  function decimalValue(field) {
    const value = form.elements[field].value.trim();
    return /^(0|[1-9][0-9]*)(\.[0-9]{1,2})?$/.test(value) ? Number(value) : NaN;
  }

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

  function resetForm() {
    form.reset();
    editingTypeId = null;
    formHeading.textContent = "Add account type";
    submit.textContent = "Add account type";
    cancel.hidden = true;
    clearFieldErrors();
  }

  function startEditing(type) {
    editingTypeId = type.typeId;
    form.elements.name.value = type.name;
    form.elements.minimumBalance.value = type.minimumBalance;
    form.elements.interestRate.value = type.interestRate;
    form.elements.dailyWithdrawalLimit.value = type.dailyWithdrawalLimit;
    formHeading.textContent = `Edit ${type.name}`;
    submit.textContent = "Save changes";
    cancel.hidden = false;
    clearFieldErrors();
    formStatus.textContent = "";
    document.querySelector("#account-type-name").focus();
  }

  function renderTypes(types) {
    list.replaceChildren();
    if (types.length === 0) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = "No account types have been configured yet.";
      list.append(empty);
      return;
    }

    const scroll = document.createElement("div");
    scroll.className = "table-scroll";
    const table = document.createElement("table");
    table.className = "data-table";
    const caption = document.createElement("caption");
    caption.textContent = "Configured account types";
    table.append(caption);
    const head = document.createElement("thead");
    const headerRow = document.createElement("tr");
    for (const label of ["Name", "Minimum balance (INR)", "Interest rate (% p.a.)",
      "Daily withdrawal limit (INR)", "Actions"]) {
      const th = document.createElement("th");
      th.scope = "col";
      th.textContent = label;
      headerRow.append(th);
    }
    head.append(headerRow);
    table.append(head);

    const body = document.createElement("tbody");
    for (const type of types) {
      const row = document.createElement("tr");
      for (const value of [type.name, type.minimumBalance, type.interestRate,
        type.dailyWithdrawalLimit]) {
        const cell = document.createElement("td");
        cell.textContent = value;
        row.append(cell);
      }
      const actionCell = document.createElement("td");
      const edit = document.createElement("button");
      edit.type = "button";
      edit.textContent = "Edit";
      edit.addEventListener("click", () => startEditing(type));
      actionCell.append(edit);
      row.append(actionCell);
      body.append(row);
    }
    table.append(body);
    scroll.append(table);
    list.append(scroll);
  }

  async function loadTypes() {
    if (!accessToken || role !== "administrator" || loading) return;
    loading = true;
    listStatus.textContent = "Loading account types…";
    listStatus.className = "result";
    try {
      const response = await window.BmsApi.get("/account-types");
      renderTypes(response.accountTypes);
      listStatus.textContent = "";
    } catch (error) {
      listStatus.textContent = error.message || "Account types could not be loaded.";
      listStatus.className = "result error";
    } finally {
      loading = false;
    }
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    clearFieldErrors();
    formStatus.textContent = "";
    formStatus.className = "result";
    formStatus.setAttribute("role", "status");

    const body = {
      name: form.elements.name.value,
      minimumBalance: decimalValue("minimumBalance"),
      interestRate: decimalValue("interestRate"),
      dailyWithdrawalLimit: decimalValue("dailyWithdrawalLimit"),
    };
    submit.disabled = true;
    submit.textContent = "Saving…";
    try {
      if (editingTypeId === null) {
        await window.BmsApi.post("/account-types", body);
        formStatus.textContent = "Account type created.";
      } else {
        await window.BmsApi.request(`/account-types/${editingTypeId}`, {
          method: "PUT",
          body,
        });
        formStatus.textContent = "Account type updated.";
      }
      formStatus.className = "result success";
      resetForm();
      await loadTypes();
    } catch (error) {
      const fields = error.fields || [];
      const hasFieldErrors = showFieldErrors(fields);
      formStatus.textContent = error.message ||
        (error.status === 404 ? "That account type no longer exists." :
          error.status === 409 ? "An account type with that name already exists." :
            "The account type could not be saved.");
      formStatus.className = "result error";
      if (!hasFieldErrors) formStatus.setAttribute("role", "alert");
    } finally {
      submit.disabled = false;
      submit.textContent = editingTypeId === null ? "Add account type" : "Save changes";
    }
  });

  cancel.addEventListener("click", () => {
    resetForm();
    formStatus.textContent = "";
  });

  document.addEventListener("bms:session", (event) => {
    const session = event.detail;
    const wasAdministrator = role === "administrator" && Boolean(accessToken);
    role = session.role;
    accessToken = session.accessToken;
    const isAdministrator = Boolean(accessToken) && !session.mustChangePassword &&
      role === "administrator";
    card.hidden = !isAdministrator;
    if (isAdministrator) {
      void loadTypes();
    } else {
      list.replaceChildren();
      listStatus.textContent = "";
      if (wasAdministrator) resetForm();
    }
  });

  document.addEventListener("bms:session-expired", () => {
    accessToken = "";
    role = "";
    card.hidden = true;
    list.replaceChildren();
  });
})();
