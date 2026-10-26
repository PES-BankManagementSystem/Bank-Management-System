// S-07: after sign-in, send each role to its own home (placeholder pages
// until the S-25 dashboards exist).
(() => {
  const home = document.querySelector("#role-home");
  const eyebrow = document.querySelector("#role-home-eyebrow");
  const heading = document.querySelector("#role-home-heading");
  const intro = document.querySelector("#role-home-intro");
  const links = document.querySelector("#role-home-links");

  const homes = {
    administrator: {
      title: "Administrator home",
      intro: "Manage customers, staff and bank-wide operations.",
      links: [
        ["#customer-registration-card", "Register a customer"],
        ["#interest-heading", "Run monthly interest"],
      ],
    },
    bank_officer: {
      title: "Bank Officer home",
      intro: "Serve customers at the counter.",
      links: [
        ["#customer-registration-card", "Register a customer"],
        ["#deposit-form", "Deposit money"],
        ["#withdraw-heading", "Withdraw money"],
        ["#transfer-heading", "Transfer money"],
      ],
    },
    customer: {
      title: "Customer home",
      intro: "Your accounts, balances, history and statements will appear here.",
      links: [],
    },
  };

  let signedInRole = "";

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
    links.replaceChildren();
    for (const [target, label] of page.links) {
      if (!document.querySelector(target)) continue;
      const item = document.createElement("li");
      const link = document.createElement("a");
      link.href = target;
      link.textContent = label;
      item.append(link);
      links.append(item);
    }
    home.hidden = false;
    if (signedInRole !== role) {
      signedInRole = role;
      home.scrollIntoView({ block: "start" });
      heading.focus();
    }
  });
})();
