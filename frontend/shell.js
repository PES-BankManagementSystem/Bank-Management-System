// S-05: base layout and navigation by role. Shows only the screens the
// signed-in role may use (SRS 2.3, 5.5) and lists them in the header.
(() => {
  const ADMIN = "administrator";
  const OFFICER = "bank_officer";

  // Screens that are on the page today. Later screens can join the
  // navigation by adding data-nav-label="..." data-roles="role other_role"
  // to their card; those cards keep controlling their own visibility.
  const views = [
    { target: "#customer-registration-card", label: "Register customer", roles: [ADMIN, OFFICER] },
    { target: "#deposit-form", label: "Deposit", roles: [ADMIN, OFFICER] },
    { target: "#withdraw-form", label: "Withdraw", roles: [ADMIN, OFFICER] },
    { target: "#transfer-form", label: "Transfer", roles: [ADMIN, OFFICER] },
    { target: "#interest-form", label: "Run interest", roles: [ADMIN] },
  ];

  const nav = document.querySelector("#app-nav");
  const user = document.querySelector("#app-user");
  const userLabel = document.querySelector("#app-user-label");
  const signOut = document.querySelector("#app-sign-out");
  const logoutButton = document.querySelector("#logout-button");

  const cards = views
    .map((view) => {
      const card = document.querySelector(view.target)?.closest(".card");
      if (!card) return null;
      if (!card.id) card.id = `${view.target.slice(1)}-card`;
      return { ...view, card };
    })
    .filter(Boolean);

  const roleLabel = (role) =>
    ({ administrator: "Administrator", bank_officer: "Bank Officer", customer: "Customer" })[role] ||
    role;

  function render({ accessToken, role, mustChangePassword }) {
    const signedIn = Boolean(accessToken) && !mustChangePassword;
    for (const view of cards) {
      view.card.classList.toggle("role-hidden", !signedIn || !view.roles.includes(role));
    }

    const links = signedIn ? cards.filter((view) => view.roles.includes(role)) : [];
    if (signedIn) {
      for (const card of document.querySelectorAll("[data-nav-label][data-roles]")) {
        if (card.dataset.roles.split(/\s+/).includes(role) && card.id) {
          links.push({ label: card.dataset.navLabel, card });
        }
      }
    }
    nav.replaceChildren(...links.map(({ label, card }) => {
      const link = document.createElement("a");
      link.href = `#${card.id}`;
      link.textContent = label;
      return link;
    }));
    nav.hidden = links.length === 0;
    user.hidden = !accessToken;
    userLabel.textContent = accessToken ? `Signed in as ${roleLabel(role)}` : "";
  }

  signOut.addEventListener("click", () => logoutButton?.click());
  document.addEventListener("bms:session", (event) => render(event.detail));
  render({ accessToken: "", role: "", mustChangePassword: false });
})();
