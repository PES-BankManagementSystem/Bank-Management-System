// S-27: Administrator audit trail viewer. Read-only: filter by date and
// account, newest first, with a "Load more" button for older entries.
(() => {
  const PAGE_SIZE = 50;
  const card = document.querySelector("#audit-card");
  const form = document.querySelector("#audit-form");
  const list = document.querySelector("#audit-list");
  const more = document.querySelector("#audit-more");
  const result = document.querySelector("#audit-result");
  const submit = document.querySelector("#audit-submit");
  let offset = 0;

  function query() {
    const params = new URLSearchParams({ limit: PAGE_SIZE, offset });
    for (const name of ["from", "to", "account"]) {
      const value = form.elements[name].value.trim();
      if (value) params.set(name, value);
    }
    return params.toString();
  }

  function table() {
    let existing = list.querySelector("table");
    if (existing) return existing;
    existing = document.createElement("table");
    existing.className = "data-table";
    const head = existing.createTHead().insertRow();
    for (const title of ["Time (UTC)", "Operator", "Action", "Account", "Details"]) {
      const th = document.createElement("th");
      th.scope = "col";
      th.textContent = title;
      head.append(th);
    }
    existing.createTBody();
    list.append(existing);
    return existing;
  }

  function append(entries) {
    const body = table().tBodies[0];
    for (const entry of entries) {
      const row = body.insertRow();
      row.insertCell().textContent = entry.timestamp;
      row.insertCell().textContent = entry.operator;
      row.insertCell().textContent = entry.action;
      row.insertCell().textContent = entry.accountNumber || `${entry.entity} #${entry.entityId}`;
      row.insertCell().textContent = entry.details;
    }
  }

  async function load(reset) {
    if (reset) {
      offset = 0;
      list.replaceChildren();
    }
    result.textContent = "";
    result.className = "result";
    submit.disabled = true;
    try {
      const body = await window.BmsApi.get(`/audit?${query()}`);
      append(body.entries);
      offset += body.entries.length;
      more.hidden = body.entries.length < PAGE_SIZE;
      if (offset === 0) {
        const empty = document.createElement("p");
        empty.className = "empty-state";
        empty.textContent = "No audit entries match these filters.";
        list.append(empty);
      }
    } catch (error) {
      more.hidden = true;
      result.textContent = error.message;
      result.className = "result error";
    } finally {
      submit.disabled = false;
    }
  }

  form.addEventListener("submit", (event) => {
    event.preventDefault();
    load(true);
  });
  more.addEventListener("click", () => load(false));

  document.addEventListener("bms:session", (event) => {
    const { accessToken, role, mustChangePassword } = event.detail;
    const visible = Boolean(accessToken) && role === "administrator" && !mustChangePassword;
    card.hidden = !visible;
    list.replaceChildren();
    more.hidden = true;
    result.textContent = "";
    if (visible) load(true);
  });
})();
