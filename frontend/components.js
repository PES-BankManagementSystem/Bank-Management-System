// S-05: reusable UI components (SRS 3.1). Plain DOM, no framework, works in
// current Chrome and Firefox.
window.BmsUI = (() => {
  let nextId = 0;
  const uniqueId = (prefix) => `${prefix}-${++nextId}`;

  // Form field with label, optional hint and an inline error message.
  // type "password" gives a masked field.
  function formField({ name, label, type = "text", id = uniqueId(name),
                       required = false, hint = "", attributes = {} }) {
    const wrapper = document.createElement("div");
    wrapper.className = "form-field";

    const labelElement = document.createElement("label");
    labelElement.htmlFor = id;
    labelElement.textContent = label;

    const input = document.createElement("input");
    input.id = id;
    input.name = name;
    input.type = type;
    input.required = required;
    for (const [key, value] of Object.entries(attributes)) input.setAttribute(key, value);

    const describedBy = [];
    wrapper.append(labelElement, input);
    if (hint) {
      const hintElement = document.createElement("p");
      hintElement.className = "hint";
      hintElement.id = `${id}-hint`;
      hintElement.textContent = hint;
      wrapper.append(hintElement);
      describedBy.push(hintElement.id);
    }
    const error = document.createElement("p");
    error.className = "field-error";
    error.id = `${id}-error`;
    wrapper.append(error);
    describedBy.push(error.id);
    input.setAttribute("aria-describedby", describedBy.join(" "));

    return {
      element: wrapper,
      input,
      setError(message) {
        error.textContent = message;
        if (message) input.setAttribute("aria-invalid", "true");
        else input.removeAttribute("aria-invalid");
      },
      clearError() {
        this.setError("");
      },
    };
  }

  // Shows server field errors (ApiError.fields) next to the matching inputs
  // of a form. Returns true if at least one field error was shown.
  function showFieldErrors(form, error) {
    let first = null;
    for (const { field, message } of error.fields || []) {
      const input = form.elements.namedItem(field);
      if (!input) continue;
      const target = document.getElementById(`${input.id}-error`);
      if (target) target.textContent = message;
      input.setAttribute("aria-invalid", "true");
      first ??= input;
    }
    if (first) first.focus();
    return first !== null;
  }

  function clearFieldErrors(form) {
    for (const input of form.querySelectorAll("[aria-invalid]")) {
      input.removeAttribute("aria-invalid");
      const target = document.getElementById(`${input.id}-error`);
      if (target) target.textContent = "";
    }
  }

  // Data table. columns: [{ key, label, format?(value, row) }]
  function dataTable({ columns, rows, caption = "", emptyMessage = "Nothing to show." }) {
    if (rows.length === 0) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = emptyMessage;
      return empty;
    }
    const table = document.createElement("table");
    table.className = "data-table";
    if (caption) table.createCaption().textContent = caption;
    const head = table.createTHead().insertRow();
    for (const column of columns) {
      const th = document.createElement("th");
      th.scope = "col";
      th.textContent = column.label;
      head.append(th);
    }
    const body = table.createTBody();
    for (const row of rows) {
      const tr = body.insertRow();
      for (const column of columns) {
        const value = column.format ? column.format(row[column.key], row) : row[column.key];
        const cell = tr.insertCell();
        if (value instanceof Node) cell.append(value);
        else cell.textContent = value ?? "";
      }
    }
    return table;
  }

  // Modal confirmation step. rows: [[label, value], ...]. Resolves true on
  // confirm, false on cancel or Escape.
  function confirmDialog({ title, rows = [], message = "",
                           confirmLabel = "Confirm", cancelLabel = "Cancel" }) {
    if (typeof HTMLDialogElement !== "function") {
      const text = [title, ...rows.map(([label, value]) => `${label}: ${value}`), message];
      return Promise.resolve(window.confirm(text.filter(Boolean).join("\n")));
    }
    return new Promise((resolve) => {
      const dialog = document.createElement("dialog");
      dialog.className = "confirm-dialog";
      const headingId = uniqueId("confirm-title");
      dialog.setAttribute("aria-labelledby", headingId);

      const heading = document.createElement("h2");
      heading.id = headingId;
      heading.textContent = title;
      dialog.append(heading);

      if (rows.length) {
        const list = document.createElement("dl");
        for (const [label, value] of rows) {
          const term = document.createElement("dt");
          term.textContent = label;
          const detail = document.createElement("dd");
          detail.textContent = value;
          list.append(term, detail);
        }
        dialog.append(list);
      }
      if (message) {
        const note = document.createElement("p");
        note.className = "hint";
        note.textContent = message;
        dialog.append(note);
      }

      const actions = document.createElement("div");
      actions.className = "dialog-actions";
      const cancel = document.createElement("button");
      cancel.type = "button";
      cancel.className = "secondary";
      cancel.textContent = cancelLabel;
      const confirm = document.createElement("button");
      confirm.type = "button";
      confirm.textContent = confirmLabel;
      actions.append(cancel, confirm);
      dialog.append(actions);

      let confirmed = false;
      cancel.addEventListener("click", () => dialog.close());
      confirm.addEventListener("click", () => {
        confirmed = true;
        dialog.close();
      });
      dialog.addEventListener("close", () => {
        dialog.remove();
        resolve(confirmed);
      });
      document.body.append(dialog);
      dialog.showModal();
      cancel.focus();
    });
  }

  // The confirmation every balance-changing operation shows before it is
  // committed: account, amount and resulting balance (SRS 3.1).
  function confirmTransaction({ action, account, amount, resultingBalance }) {
    return confirmDialog({
      title: `Confirm ${action}`,
      rows: [
        ["Account", account],
        ["Amount", `₹${amount}`],
        ["Resulting balance", `₹${resultingBalance}`],
      ],
      message: "No changes are made until you confirm.",
      confirmLabel: `Confirm ${action}`,
    });
  }

  // Success / failure message in a status element (role="status").
  function showMessage(element, text, kind = "") {
    element.textContent = text;
    element.className = kind ? `result ${kind}` : "result";
  }

  // Short-lived message in the corner, for results that are not tied to one form.
  function toast(text, kind = "success") {
    let region = document.querySelector("#toast-region");
    if (!region) {
      region = document.createElement("div");
      region.id = "toast-region";
      region.className = "toast-region";
      region.setAttribute("role", "status");
      region.setAttribute("aria-live", "polite");
      document.body.append(region);
    }
    const item = document.createElement("p");
    item.className = `toast ${kind}`;
    item.textContent = text;
    region.append(item);
    setTimeout(() => item.remove(), 5000);
  }

  return {
    formField, showFieldErrors, clearFieldErrors, dataTable,
    confirmDialog, confirmTransaction, showMessage, toast,
  };
})();
