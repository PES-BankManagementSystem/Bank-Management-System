// S-25: load the authenticated user's role-specific dashboard.
(() => {
  const home = document.querySelector("#role-home");
  const eyebrow = document.querySelector("#role-home-eyebrow");
  const heading = document.querySelector("#role-home-heading");
  const intro = document.querySelector("#role-home-intro");
  const status = document.querySelector("#dashboard-status");
  const metrics = document.querySelector("#dashboard-metrics");
  const details = document.querySelector("#dashboard-details");
  const links = document.querySelector("#role-home-links");

  const homes = {
    administrator: {
      title: "Administrator dashboard",
      intro: "Monitor customers, accounts, staff and today's bank activity.",
    },
    bank_officer: {
      title: "Bank Officer dashboard",
      intro: "Review today's work and start a customer transaction.",
    },
    customer: {
      title: "Customer dashboard",
      intro: "View your accounts, balances, history and statements.",
    },
  };

  let signedInRole = "";

  function addMetric(label, value) {
    const item = document.createElement("div");
    item.className = "dashboard-metric";
    const name = document.createElement("span");
    name.textContent = label;
    const amount = document.createElement("strong");
    amount.textContent = value;
    item.append(name, amount);
    metrics.append(item);
  }

  function addLink(parent, href, label) {
    const link = document.createElement("a");
    link.href = href;
    link.textContent = label;
    parent.append(link);
  }

  function renderDashboard(data) {
    metrics.replaceChildren();
    details.replaceChildren();
    links.replaceChildren();
    for (const link of data.quickLinks || []) {
      const item = document.createElement("li");
      addLink(item, link.href, link.label);
      links.append(item);
    }

    if (data.role === "administrator") {
      addMetric("Customers", data.counts.customers);
      addMetric("Accounts", data.counts.accounts);
      addMetric("Staff", data.counts.staff);
      addMetric("Today's transactions", data.transactionsToday.count);
      addMetric("Today's transaction value", `₹${data.transactionsToday.totalAmount}`);
      return;
    }

    if (data.role === "bank_officer") {
      addMetric("Processed today", data.transactionsToday.count);
      addMetric("Processed value", `₹${data.transactionsToday.totalAmount}`);
      const table = document.createElement("table");
      table.className = "data-table";
      table.innerHTML = "<caption>Transactions processed today</caption><thead><tr>" +
        "<th>Time</th><th>Type</th><th>Account</th><th>Amount (INR)</th></tr></thead>";
      const body = document.createElement("tbody");
      for (const transaction of data.transactionsToday.items) {
        const row = document.createElement("tr");
        for (const value of [transaction.createdAt, transaction.type,
          transaction.accountNumber, `₹${transaction.amount}`]) {
          const cell = document.createElement("td");
          cell.textContent = value;
          row.append(cell);
        }
        body.append(row);
      }
      table.append(body);
      details.append(table);
      return;
    }

    const table = document.createElement("table");
    table.className = "data-table";
    table.innerHTML = "<caption>Your accounts</caption><thead><tr>" +
      "<th>Account</th><th>Type</th><th>Status</th><th>Balance (INR)</th><th>Links</th></tr></thead>";
    const body = document.createElement("tbody");
    for (const account of data.accounts) {
      const row = document.createElement("tr");
      for (const value of [account.accountNumber, account.type, account.status,
        `₹${account.balance}`]) {
        const cell = document.createElement("td");
        cell.textContent = value;
        row.append(cell);
      }
      const actions = document.createElement("td");
      for (const link of account.links) {
        addLink(actions, link.href, link.label);
        actions.append(" ");
      }
      row.append(actions);
      body.append(row);
    }
    table.append(body);
    details.append(table);
  }

  document.addEventListener("bms:session", (event) => {
    const { accessToken, role, mustChangePassword } = event.detail;
    const page = homes[role];
    if (!accessToken || mustChangePassword || !page) {
      home.hidden = true;
      signedInRole = "";
      return;
    }

    eyebrow.textContent = role.replaceAll("_", " ");
    heading.textContent = page.title;
    intro.textContent = page.intro;
    status.textContent = "Loading dashboard...";
    status.className = "result";
    home.hidden = false;
    window.BmsApi.get("/dashboard").then((data) => {
      renderDashboard(data);
      status.textContent = "";
    }).catch((error) => {
      status.textContent = error.message || "Unable to load your dashboard.";
      status.className = "result error";
    });

    if (signedInRole !== role) {
      signedInRole = role;
      home.scrollIntoView({ block: "start" });
      heading.focus();
    }
  });
})();
