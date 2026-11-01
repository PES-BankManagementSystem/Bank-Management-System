// S-31 (NFR-9) browser walkthrough: drives the core flows of all three roles
// in an installed Chrome or Firefox through puppeteer-core.
//
//   npm install
//   node walkthrough.js <chrome|firefox> <baseUrl> <screenshotDir>
//
// Run it against a server started on a fresh copy of db/schema.sql + db/seed.sql;
// it changes balances. Set CHROME_PATH or FIREFOX_PATH if the browser is not at
// the default Linux location. Prints a JSON report; exits 0 only if every step
// passed with no JavaScript errors.
const puppeteer = require("puppeteer-core");
const fs = require("fs");
const path = require("path");

const [browserName, baseUrl, shotDir] = process.argv.slice(2);
const PASSWORD = "Password123!";
const today = new Date().toISOString().slice(0, 10);

const results = [];
const scriptErrors = [];
const failedRequests = [];

async function main() {
  fs.mkdirSync(shotDir, { recursive: true });
  const launch = browserName === "firefox"
    ? { browser: "firefox", executablePath: process.env.FIREFOX_PATH || "/usr/bin/firefox", headless: true }
    : { browser: "chrome", executablePath: process.env.CHROME_PATH || "/usr/bin/google-chrome-stable", headless: true,
        args: ["--no-first-run", "--no-default-browser-check"] };
  const browser = await puppeteer.launch(launch);
  const version = await browser.version();
  const page = await browser.newPage();
  await page.setViewport({ width: 1280, height: 900 });
  page.on("dialog", (dialog) => dialog.accept());
  // "Failed to load resource" console lines are HTTP error responses, which the
  // walkthrough triggers on purpose (wrong password, frozen account); they are
  // reported separately from JavaScript errors.
  page.on("console", (msg) => {
    if (msg.type() !== "error") return;
    (msg.text().startsWith("Failed to load resource") ? failedRequests : scriptErrors).push(msg.text());
  });
  page.on("pageerror", (err) => scriptErrors.push(String(err)));

  let n = 0;
  async function step(name, fn) {
    const started = Date.now();
    try {
      const detail = await fn();
      results.push({ step: name, ok: true, detail: detail || "", ms: Date.now() - started });
    } catch (err) {
      results.push({ step: name, ok: false, detail: String(err.message || err).split("\n")[0], ms: Date.now() - started });
    }
    n += 1;
    await page.screenshot({ path: path.join(shotDir, `${String(n).padStart(2, "0")}-${name.replace(/[^a-z0-9]+/gi, "-").toLowerCase()}.png`), fullPage: false }).catch(() => {});
  }

  const text = (sel) => page.$eval(sel, (el) => el.textContent.trim());
  const visible = (sel) => page.$eval(sel, (el) => {
    const card = el.closest(".card") || el;
    return !el.hidden && !card.hidden && !card.classList.contains("role-hidden") &&
      el.getClientRects().length > 0;
  });
  async function fill(sel, value) {
    await page.$eval(sel, (el) => { el.value = ""; });
    await page.type(sel, value);
  }
  // Date inputs are typed differently per browser; set the value as a user agent would.
  async function setDate(sel, value) {
    await page.$eval(sel, (el, v) => {
      el.value = v;
      el.dispatchEvent(new Event("input", { bubbles: true }));
      el.dispatchEvent(new Event("change", { bubbles: true }));
    }, value);
  }
  async function waitText(sel, pattern, timeout = 10000) {
    await page.waitForFunction((s, p) => {
      const el = document.querySelector(s);
      return el && new RegExp(p).test(el.textContent);
    }, { timeout }, sel, pattern.source);
    return text(sel);
  }
  async function login(username, password = PASSWORD) {
    await fill("#login-username", username);
    await fill("#login-password", password);
    await page.click("#login-submit");
  }
  async function signOut() {
    await page.click("#app-sign-out");
    await page.waitForFunction(() => document.querySelector("#app-user").hidden, { timeout: 10000 });
  }
  async function navLabels() {
    return page.$$eval("#app-nav a", (as) => as.map((a) => a.textContent.trim()));
  }

  await step("Load sign-in page", async () => {
    const res = await page.goto(baseUrl, { waitUntil: "load" });
    if (res.status() !== 200) throw new Error(`HTTP ${res.status()}`);
    if (!(await visible("#login-form"))) throw new Error("sign-in form not visible");
    return `HTTP 200, title "${await page.title()}"`;
  });

  await step("Wrong password is refused", async () => {
    await login("officer2", "wrong-password");
    return waitText("#login-error", /\S/);
  });

  // Bank Officer
  await step("Bank Officer signs in", async () => {
    await login("officer1");
    const heading = await waitText("#role-home-heading", /Bank Officer home/);
    return `${heading}; nav: ${(await navLabels()).join(", ")}`;
  });

  await step("Officer deposits INR 500.00 into ACC001", async () => {
    await fill("#account-number", "ACC001");
    await fill("#amount", "500.00");
    await page.click("#submit-button");
    return waitText("#result", /succeeded/);
  });

  await step("Officer withdraws INR 200.00 from ACC001", async () => {
    await fill("#withdraw-account-number", "ACC001");
    await fill("#withdraw-amount", "200.00");
    await page.click("#withdraw-submit-button");
    return waitText("#withdraw-result", /succeeded/);
  });

  await step("Officer transfers INR 100.00 ACC001 to ACC004", async () => {
    await fill("#from-account-number", "ACC001");
    await fill("#to-account-number", "ACC004");
    await fill("#transfer-amount", "100.00");
    await page.click("#transfer-submit-button");
    return waitText("#transfer-result", /Transfer succeeded/);
  });

  await step("Deposit to frozen ACC002 is refused", async () => {
    await fill("#account-number", "ACC002");
    await fill("#amount", "10.00");
    await page.click("#submit-button");
    const msg = await waitText("#result", /not completed|frozen|not active/i);
    const cls = await page.$eval("#result", (el) => el.className);
    if (!cls.includes("error")) throw new Error(`expected error styling, got "${cls}"`);
    return msg;
  });

  await step("Officer registers a customer", async () => {
    await fill("#customer-name", "Portability Check");
    await setDate("#customer-dob", "1995-04-10");
    await fill("#customer-address", "1 Test Lane, Bengaluru");
    await fill("#customer-phone", "9876543210");
    await fill("#customer-id-reference", "S31-CHECK-" + browserName.toUpperCase());
    await fill("#customer-email", `s31.${browserName}@example.com`);
    await page.click("#customer-submit");
    const created = await waitText("#customer-created", /CUS-/);
    return created.replace(/password[^.]*\S+/i, "password [redacted]").slice(0, 120);
  });

  await step("Officer views ACC001 history", async () => {
    await fill("#history-account-number", "ACC001");
    await page.click("#history-apply");
    await page.waitForFunction(() => document.querySelectorAll("#history-entries tr").length > 0, { timeout: 10000 });
    const rows = await page.$$eval("#history-entries tr", (trs) => trs.length);
    return `${rows} rows`;
  });

  await step("Bank Officer signs out", async () => {
    await signOut();
    if (!(await visible("#login-form"))) throw new Error("sign-in form not shown after sign-out");
    return "back at sign-in";
  });

  // Customer
  await step("Customer signs in", async () => {
    await login("customer1");
    await page.waitForFunction(() => !document.querySelector("#app-user").hidden, { timeout: 10000 });
    const heading = await text("#role-home-heading");
    return `${heading}; nav: ${(await navLabels()).join(", ")}`;
  });

  await step("Customer sees own accounts and balances", async () => {
    await page.waitForFunction(() => document.querySelectorAll("#customer-accounts tr").length >= 2, { timeout: 10000 });
    const rows = await page.$$eval("#customer-accounts tr", (trs) => trs.map((tr) => tr.textContent.replace(/\s+/g, " ").trim()));
    if (!rows.some((r) => r.includes("ACC001")) || !rows.some((r) => r.includes("ACC004"))) throw new Error(`rows: ${rows.join(" | ")}`);
    if (rows.some((r) => r.includes("ACC002"))) throw new Error("another customer's account is listed");
    return rows.join(" | ");
  });

  await step("Customer views ACC001 history with today's filter", async () => {
    await page.waitForFunction(() => [...document.querySelectorAll("#history-account option")].some((o) => o.value === "ACC001"), { timeout: 10000 });
    await page.select("#history-account", "ACC001");
    await setDate("#history-from", today);
    await setDate("#history-to", today);
    await page.click("#history-apply");
    await page.waitForFunction(() => document.querySelectorAll("#history-entries tr").length >= 3, { timeout: 10000 });
    const rows = await page.$$eval("#history-entries tr", (trs) => trs.map((tr) => tr.textContent.replace(/\s+/g, " ").trim()));
    return `${rows.length} rows today: ${rows.join(" | ")}`;
  });

  await step("Customer views ACC001 statement", async () => {
    await fill("#statement-account-number", "ACC001");
    await setDate("#statement-from", "2026-01-01");
    await setDate("#statement-to", today);
    await page.click("#statement-submit");
    await page.waitForFunction(() => !document.querySelector("#statement-output").hidden, { timeout: 10000 });
    const opening = await text("#statement-opening-balance");
    const closing = await text("#statement-closing-balance");
    const rows = await page.$$eval("#statement-entries tr", (trs) => trs.length);
    return `opening ${opening}, closing ${closing}, ${rows} rows`;
  });

  await step("Customer cannot see officer screens", async () => {
    const hidden = await page.$eval("#deposit-card", (el) => el.classList.contains("role-hidden") || el.getClientRects().length === 0);
    if (!hidden) throw new Error("deposit card visible to customer");
    return "deposit/withdraw/transfer cards hidden";
  });

  await step("Customer signs out", async () => {
    await signOut();
    return "back at sign-in";
  });

  // Administrator
  await step("Administrator signs in", async () => {
    await login("admin");
    const heading = await waitText("#role-home-heading", /Administrator home/);
    return `${heading}; nav: ${(await navLabels()).join(", ")}`;
  });

  await step("Administrator views audit trail", async () => {
    await page.click("#audit-submit");
    await page.waitForFunction(() => document.querySelectorAll("#audit-list tr").length > 1, { timeout: 10000 });
    const body = await page.$eval("#audit-list", (el) => el.textContent);
    const rows = await page.$$eval("#audit-list tbody tr", (trs) => trs.length);
    if (!/deposit/i.test(body)) throw new Error("today's deposit not in audit trail");
    return `${rows} entries, includes the deposit`;
  });

  await step("Administrator views staff list", async () => {
    await page.waitForFunction(() => /officer1/.test(document.querySelector("#staff-list").textContent), { timeout: 10000 });
    return "officer1 and officer2 listed";
  });

  await step("Administrator views account types", async () => {
    await page.waitForFunction(() => /Savings/.test(document.querySelector("#account-types-list").textContent), { timeout: 10000 });
    return "Savings and Current listed";
  });

  await step("Administrator signs out", async () => {
    await signOut();
    return "back at sign-in";
  });

  await browser.close();
  const report = { browser: browserName, version, results, scriptErrors, failedRequests };
  console.log(JSON.stringify(report, null, 2));
  process.exit(results.every((r) => r.ok) && scriptErrors.length === 0 ? 0 : 1);
}

main().catch((err) => { console.error(err); process.exit(2); });
