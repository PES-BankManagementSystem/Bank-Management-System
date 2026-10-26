// S-09: Administrator view of locked users with an Unlock button per user.
(() => {
  const card = document.querySelector("#locked-users-card");
  const list = document.querySelector("#locked-users-list");
  const refresh = document.querySelector("#locked-users-refresh");
  const result = document.querySelector("#locked-users-result");
  let token = "";

  const roleLabel = (role) => role.replaceAll("_", " ");

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
    if (!response.ok) throw new Error(body.message || "The request failed.");
    return body;
  }

  function render(users) {
    list.replaceChildren();
    if (users.length === 0) {
      const empty = document.createElement("p");
      empty.className = "empty-state";
      empty.textContent = "No users are locked.";
      list.append(empty);
      return;
    }
    const table = document.createElement("table");
    table.className = "data-table";
    const head = table.createTHead().insertRow();
    for (const title of ["Username", "Role", "Locked at", ""]) {
      const th = document.createElement("th");
      th.scope = "col";
      th.textContent = title;
      head.append(th);
    }
    const body = table.createTBody();
    for (const user of users) {
      const row = body.insertRow();
      row.insertCell().textContent = user.username;
      row.insertCell().textContent = roleLabel(user.role);
      row.insertCell().textContent = user.lockedAt;
      const button = document.createElement("button");
      button.type = "button";
      button.textContent = "Unlock";
      button.setAttribute("aria-label", `Unlock ${user.username}`);
      button.addEventListener("click", () => unlock(user, button));
      row.insertCell().append(button);
    }
    list.append(table);
  }

  async function load() {
    result.textContent = "";
    result.className = "result";
    try {
      const body = await api("/users/locked");
      render(body.users);
    } catch (error) {
      result.textContent = error.message;
      result.className = "result error";
    }
  }

  async function unlock(user, button) {
    if (!window.confirm(`Unlock ${user.username} (${roleLabel(user.role)})?`)) return;
    button.disabled = true;
    try {
      await api(`/users/${user.userId}/unlock`, { method: "PATCH" });
      await load();
      result.textContent = `${user.username} is unlocked and can sign in again.`;
      result.className = "result success";
    } catch (error) {
      button.disabled = false;
      result.textContent = error.message;
      result.className = "result error";
    }
  }

  refresh.addEventListener("click", load);

  document.addEventListener("bms:session", (event) => {
    const { accessToken, role, mustChangePassword } = event.detail;
    token = accessToken;
    const visible = Boolean(accessToken) && role === "administrator" && !mustChangePassword;
    card.hidden = !visible;
    list.replaceChildren();
    result.textContent = "";
    if (visible) load();
  });
})();
