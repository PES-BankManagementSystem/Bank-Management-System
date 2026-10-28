// S-24: Administrator page to create, rename, deactivate and reactivate
// Bank Officer accounts.
(() => {
  const card = document.querySelector("#staff-card");
  const form = document.querySelector("#staff-form");
  const usernameInput = document.querySelector("#staff-username");
  const usernameError = document.querySelector("#staff-username-error");
  const submit = document.querySelector("#staff-submit");
  const created = document.querySelector("#staff-created");
  const list = document.querySelector("#staff-list");
  const result = document.querySelector("#staff-result");
  const usernamePattern = /^[a-z][a-z0-9._-]{2,31}$/;
  let token = "";

  async function api(path, options = {}) {
    const response = await fetch(path, {
      ...options,
      headers: { "Content-Type": "application/json", Authorization: `Bearer ${token}` },
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      throw new Error("Your session has ended. Please sign in again.");
    }
    const body = response.status === 204 ? {} : await response.json();
    if (!response.ok) {
      const error = new Error(body.message || "The request failed.");
      error.field = body.field;
      throw error;
    }
    return body;
  }

  function showResult(message, kind) {
    result.textContent = message;
    result.className = `result ${kind}`;
  }

  function setUsernameError(message) {
    usernameError.textContent = message;
    if (message) usernameInput.setAttribute("aria-invalid", "true");
    else usernameInput.removeAttribute("aria-invalid");
  }

  function actionButton(label, ariaLabel, onClick) {
    const button = document.createElement("button");
    button.type = "button";
    button.textContent = label;
    button.setAttribute("aria-label", ariaLabel);
    button.addEventListener("click", () => onClick(button));
    return button;
  }

  function render(officers) {
    list.replaceChildren();
    if (officers.length === 0) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = "No Bank Officers yet.";
      list.append(empty);
      return;
    }
    const table = document.createElement("table");
    table.className = "data-table";
    const head = table.createTHead().insertRow();
    for (const title of ["Username", "Status", ""]) {
      const th = document.createElement("th");
      th.scope = "col";
      th.textContent = title;
      head.append(th);
    }
    const body = table.createTBody();
    for (const officer of officers) {
      const row = body.insertRow();
      row.insertCell().textContent = officer.username;
      row.insertCell().textContent = officer.status;
      const actions = row.insertCell();
      actions.append(actionButton("Rename", `Rename ${officer.username}`,
        (button) => rename(officer, button)));
      actions.append(" ");
      if (officer.status === "inactive") {
        actions.append(actionButton("Reactivate", `Reactivate ${officer.username}`,
          (button) => setStatus(officer, "active", button)));
      } else {
        actions.append(actionButton("Deactivate", `Deactivate ${officer.username}`,
          (button) => setStatus(officer, "inactive", button)));
      }
    }
    list.append(table);
  }

  async function load() {
    try {
      render((await api("/staff")).officers);
    } catch (error) {
      showResult(error.message, "error");
    }
  }

  async function update(officer, changes, button, successMessage) {
    button.disabled = true;
    try {
      await api(`/staff/${officer.userId}`, { method: "PATCH", body: JSON.stringify(changes) });
      await load();
      showResult(successMessage, "success");
    } catch (error) {
      button.disabled = false;
      showResult(error.message, "error");
    }
  }

  function setStatus(officer, status, button) {
    const verb = status === "inactive" ? "Deactivate" : "Reactivate";
    const note = status === "inactive" ? " They will be signed out and cannot sign in." : "";
    if (!window.confirm(`${verb} ${officer.username}?${note}`)) return;
    update(officer, { status }, button,
      `${officer.username} is now ${status === "inactive" ? "deactivated" : "active"}.`);
  }

  function rename(officer, button) {
    const username = window.prompt(`New username for ${officer.username}`, officer.username);
    if (username === null || username.trim() === officer.username) return;
    if (!usernamePattern.test(username.trim())) {
      showResult("Use 3-32 lower-case letters, digits, \".\", \"_\" or \"-\", starting with a letter.", "error");
      return;
    }
    update(officer, { username: username.trim() }, button,
      `${officer.username} is now ${username.trim()}.`);
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    setUsernameError("");
    created.textContent = "";
    result.textContent = "";
    const username = usernameInput.value.trim();
    if (!usernamePattern.test(username)) {
      setUsernameError("Use 3-32 lower-case letters, digits, \".\", \"_\" or \"-\", starting with a letter.");
      usernameInput.focus();
      return;
    }
    submit.disabled = true;
    try {
      const body = await api("/staff", { method: "POST", body: JSON.stringify({ username }) });
      form.reset();
      created.textContent =
        `Bank Officer created. Username: ${body.login.username} ` +
        `Temporary password: ${body.login.temporaryPassword} ` +
        "Shown only once. Hand it to the officer directly; they must change it at first sign in.";
      await load();
    } catch (error) {
      if (error.field === "username") {
        setUsernameError(error.message);
        usernameInput.focus();
      } else {
        showResult(error.message, "error");
      }
    } finally {
      submit.disabled = false;
    }
  });

  document.addEventListener("bms:session", (event) => {
    const { accessToken, role, mustChangePassword } = event.detail;
    token = accessToken;
    const visible = Boolean(accessToken) && role === "administrator" && !mustChangePassword;
    card.hidden = !visible;
    list.replaceChildren();
    created.textContent = "";
    result.textContent = "";
    setUsernameError("");
    form.reset();
    if (visible) load();
  });
})();
