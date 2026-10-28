const loginForm = document.querySelector("#login-form");
const loginError = document.querySelector("#login-error");
const sessionStatus = document.querySelector("#session-status");
const logoutButton = document.querySelector("#logout-button");
const passwordChangeCard = document.querySelector("#password-change-card");
const passwordChangeForm = document.querySelector("#password-change-form");
const passwordChangeError = document.querySelector("#password-change-error");
const registrationCard = document.querySelector("#customer-registration-card");
const customerForm = document.querySelector("#customer-form");
const customerSubmit = document.querySelector("#customer-submit");
const customerFormError = document.querySelector("#customer-form-error");
const customerCreated = document.querySelector("#customer-created");
const customerManagementCard = document.querySelector("#customer-management-card");
const customerSearchForm = document.querySelector("#customer-search-form");
const customerSearchQuery = document.querySelector("#customer-search-query");
const customerListStatus = document.querySelector("#customer-list-status");
const customerResults = document.querySelector("#customer-results");
const customerPreviousPage = document.querySelector("#customer-previous-page");
const customerNextPage = document.querySelector("#customer-next-page");
const customerPageStatus = document.querySelector("#customer-page-status");
const customerDetailCard = document.querySelector("#customer-detail-card");
const customerDetailFields = document.querySelector("#customer-detail-fields");
const customerDetailAccounts = document.querySelector("#customer-detail-accounts");
const customerDetailStatus = document.querySelector("#customer-detail-status");
const customerEditButton = document.querySelector("#customer-edit-button");
const customerEditForm = document.querySelector("#customer-edit-form");
const customerEditSubmit = document.querySelector("#customer-edit-submit");
const customerProfileCard = document.querySelector("#customer-profile-card");
const customerProfileFields = document.querySelector("#customer-profile-fields");
const customerProfileAccounts = document.querySelector("#customer-profile-accounts");
const customerProfileStatus = document.querySelector("#customer-profile-status");
const staffTransactionCards = [
  document.querySelector("#deposit-card"),
  document.querySelector("#withdraw-card"),
  document.querySelector("#transfer-card"),
  document.querySelector("#interest-card"),
];

let accessToken = "";
let currentRole = "";
let mustChangePassword = false;
let customerOffset = 0;
let customerTotal = 0;
const customerPageSize = 20;
let selectedCustomerId = "";

const fieldMap = {
  name: "customer-name",
  dateOfBirth: "customer-dob",
  address: "customer-address",
  phone: "customer-phone",
  identificationReference: "customer-id-reference",
  email: "customer-email",
};

const editFieldMap = {
  name: "customer-edit-name",
  dateOfBirth: "customer-edit-dob",
  address: "customer-edit-address",
  phone: "customer-edit-phone",
  identificationReference: "customer-edit-id-reference",
  email: "customer-edit-email",
};

const today = new Date();
document.querySelector("#customer-dob").max =
  `${today.getFullYear()}-${String(today.getMonth() + 1).padStart(2, "0")}-${String(today.getDate()).padStart(2, "0")}`;

function setFieldError(field, message) {
  const inputId = fieldMap[field];
  if (!inputId) return;
  const input = document.getElementById(inputId);
  const error = document.getElementById(`${inputId}-error`);
  input.setAttribute("aria-invalid", "true");
  error.textContent = message;
}

function clearFieldErrors() {
  for (const inputId of Object.values(fieldMap)) {
    document.getElementById(inputId).removeAttribute("aria-invalid");
    document.getElementById(`${inputId}-error`).textContent = "";
  }
}

function clearEditFieldErrors() {
  for (const inputId of Object.values(editFieldMap)) {
    document.getElementById(inputId).removeAttribute("aria-invalid");
    document.getElementById(`${inputId}-error`).textContent = "";
  }
}

function displayEditFieldErrors(body) {
  const errors = Array.isArray(body.fields)
    ? body.fields
    : body.field ? [{ field: body.field, message: body.message }] : [];
  for (const error of errors) {
    const inputId = editFieldMap[error.field];
    if (!inputId) continue;
    const input = document.getElementById(inputId);
    input.setAttribute("aria-invalid", "true");
    document.getElementById(`${inputId}-error`).textContent = error.message;
  }
  const first = errors.find((error) => editFieldMap[error.field]);
  if (first) document.getElementById(editFieldMap[first.field]).focus();
  return errors.length > 0;
}

function appendDetail(details, label, value) {
  const term = document.createElement("dt");
  term.textContent = label;
  const description = document.createElement("dd");
  description.textContent = value == null ? "" : String(value);
  details.append(term, description);
}

function renderCustomerDetails(detailsElement, accountsElement, customer) {
  detailsElement.replaceChildren();
  for (const [label, value] of [
    ["Customer ID", customer.customerId],
    ["Name", customer.name],
    ["Date of birth", customer.dateOfBirth],
    ["Address", customer.address],
    ["Phone", customer.phone],
    ["Email", customer.email],
    ["Identification reference", customer.identificationReference],
    ["Status", customer.status],
    ["Created", customer.createdAt],
    ["Updated", customer.updatedAt],
  ]) {
    appendDetail(detailsElement, label, value);
  }

  accountsElement.replaceChildren();
  for (const account of customer.accounts || []) {
    const row = document.createElement("tr");
    for (const value of [
      account.accountNumber, account.type, account.status, account.balance,
    ]) {
      const cell = document.createElement("td");
      cell.textContent = value == null ? "" : String(value);
      row.append(cell);
    }
    accountsElement.append(row);
  }
  if (!accountsElement.childElementCount) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.colSpan = 4;
    cell.textContent = "No accounts are linked to this customer.";
    row.append(cell);
    accountsElement.append(row);
  }
}

async function loadCustomerPage() {
  customerListStatus.textContent = "Loading customers...";
  customerResults.replaceChildren();
  customerPreviousPage.disabled = true;
  customerNextPage.disabled = true;
  document.querySelector("#customer-search-submit").disabled = true;
  try {
    const params = new URLSearchParams({
      query: customerSearchQuery.value,
      limit: String(customerPageSize),
      offset: String(customerOffset),
    });
    const response = await fetch(`/customers?${params}`, {
      headers: { Authorization: `Bearer ${accessToken}` },
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      return;
    }
    const body = await response.json();
    if (!response.ok) throw new Error(body.message || "Unable to load customers.");

    customerTotal = body.total;
    for (const customer of body.items) {
      const row = document.createElement("tr");
      for (const value of [
        customer.customerId, customer.name, customer.phone, customer.email,
      ]) {
        const cell = document.createElement("td");
        cell.textContent = value;
        row.append(cell);
      }
      const actionCell = document.createElement("td");
      const openButton = document.createElement("button");
      openButton.type = "button";
      openButton.textContent = "View";
      openButton.dataset.customerId = customer.customerId;
      openButton.addEventListener("click", () => openCustomer(openButton.dataset.customerId));
      actionCell.append(openButton);
      row.append(actionCell);
      customerResults.append(row);
    }
    customerListStatus.textContent = customerTotal === 0 ? "No customers found." : "";
    const firstResult = customerTotal === 0 ? 0 : customerOffset + 1;
    const lastResult = Math.min(customerOffset + customerPageSize, customerTotal);
    customerPageStatus.textContent =
      `${firstResult}-${lastResult} of ${customerTotal}`;
    customerPreviousPage.disabled = customerOffset === 0;
    customerNextPage.disabled = customerOffset + customerPageSize >= customerTotal;
  } catch (error) {
    customerListStatus.textContent = error.message || "Unable to load customers.";
  } finally {
    document.querySelector("#customer-search-submit").disabled = false;
  }
}

async function openCustomer(customerId) {
  customerDetailCard.hidden = false;
  customerDetailStatus.textContent = "Loading customer details…";
  customerEditForm.hidden = true;
  customerEditButton.hidden = true;
  selectedCustomerId = customerId;
  try {
    const response = await fetch(`/customers/${encodeURIComponent(customerId)}`, {
      headers: { Authorization: `Bearer ${accessToken}` },
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      return;
    }
    const body = await response.json();
    if (!response.ok) throw new Error(body.message || "Unable to load customer details.");
    renderCustomerDetails(customerDetailFields, customerDetailAccounts, body);
    for (const [field, value] of [
      ["name", body.name],
      ["dateOfBirth", body.dateOfBirth],
      ["address", body.address],
      ["phone", body.phone],
      ["identificationReference", body.identificationReference],
      ["email", body.email],
    ]) {
      customerEditForm.elements[field].value = value;
    }
    customerDetailStatus.textContent = "";
    customerEditButton.hidden = false;
    customerDetailCard.scrollIntoView({ behavior: "smooth", block: "start" });
  } catch (error) {
    customerDetailStatus.textContent = error.message || "Unable to load customer details.";
  }
}

async function loadMyProfile() {
  customerProfileStatus.textContent = "Loading your profile...";
  try {
    const response = await fetch("/customers/me", {
      headers: { Authorization: `Bearer ${accessToken}` },
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      return;
    }
    const body = await response.json();
    if (!response.ok) throw new Error(body.message || "Unable to load your profile.");
    renderCustomerDetails(customerProfileFields, customerProfileAccounts, body);
    customerProfileStatus.textContent = "";
  } catch (error) {
    customerProfileStatus.textContent = error.message || "Unable to load your profile.";
  }
}

function displayApiFieldErrors(body) {
  const errors = Array.isArray(body.fields)
    ? body.fields
    : body.field ? [{ field: body.field, message: body.message }] : [];
  for (const error of errors) setFieldError(error.field, error.message);
  const first = errors.find((error) => fieldMap[error.field]);
  if (first) document.getElementById(fieldMap[first.field]).focus();
  return errors.length > 0;
}

function validateCustomerForm() {
  const values = Object.fromEntries(new FormData(customerForm).entries());
  const errors = [];
  const add = (field, message) => errors.push({ field, message });
  const name = values.name.trim();
  const address = values.address.trim();
  const phone = values.phone.replace(/[\s-]/g, "");
  const idReference = values.identificationReference.trim();
  const email = values.email.trim();

  if (!name || name.length > 100 || !/[A-Za-z]/.test(name) || /[^A-Za-z .'-]/.test(name)) {
    add("name", "Enter a name of 1-100 letters, spaces, hyphens, apostrophes or dots.");
  }
  if (!values.dateOfBirth) {
    add("dateOfBirth", "Date of birth is required.");
  } else {
    const birth = new Date(`${values.dateOfBirth}T00:00:00`);
    const todayLocal = new Date();
    todayLocal.setHours(0, 0, 0, 0);
    const age = today.getFullYear() - birth.getFullYear() -
      ((todayLocal.getMonth() < birth.getMonth() ||
        (todayLocal.getMonth() === birth.getMonth() && todayLocal.getDate() < birth.getDate())) ? 1 : 0);
    if (Number.isNaN(birth.getTime()) || birth >= todayLocal || age > 120) {
      add("dateOfBirth", "Enter a real date in the past; age must not exceed 120 years.");
    }
  }
  if (!address || Array.from(address).length > 255) {
    add("address", "Address is required and must not exceed 255 characters.");
  }
  if (!/^\+?\d{10,15}$/.test(phone)) {
    add("phone", "Phone must contain 10-15 digits, optionally starting with +.");
  }
  if (!/^[A-Za-z0-9-]{1,50}$/.test(idReference)) {
    add("identificationReference", "Use 1-50 letters, numbers or hyphens.");
  }
  if (!/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email) || email.length > 254) {
    add("email", "Enter a valid email address.");
  }
  for (const error of errors) setFieldError(error.field, error.message);
  if (errors.length) document.getElementById(fieldMap[errors[0].field]).focus();
  return errors.length === 0;
}

function updateSessionView() {
  const authorizedRole = currentRole === "administrator" || currentRole === "bank_officer";
  loginForm.hidden = Boolean(accessToken);
  logoutButton.hidden = !accessToken;
  registrationCard.hidden = !accessToken || mustChangePassword || !authorizedRole;
  customerManagementCard.hidden = !accessToken || mustChangePassword || !authorizedRole;
  customerProfileCard.hidden = !accessToken || mustChangePassword || currentRole !== "customer";
  for (const card of staffTransactionCards) {
    card.hidden = Boolean(accessToken) && currentRole === "customer";
  }
  passwordChangeCard.hidden = !accessToken || !mustChangePassword;
  sessionStatus.textContent = accessToken
    ? `Signed in as ${currentRole.replaceAll("_", " ")}.`
    : "You are signed out.";
  customerDetailCard.hidden = true;
  document.dispatchEvent(new CustomEvent("bms:session", {
    detail: { accessToken, role: currentRole, mustChangePassword },
  }));
  if (accessToken && !mustChangePassword) {
    if (authorizedRole) void loadCustomerPage();
    if (currentRole === "customer") void loadMyProfile();
  }
}

loginForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  loginError.textContent = "";
  sessionStatus.textContent = "";
  const submit = loginForm.querySelector("button[type=submit]");
  submit.disabled = true;
  try {
    const response = await fetch("/auth/login", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        username: loginForm.elements.username.value.trim(),
        password: loginForm.elements.password.value,
      }),
    });
    const body = await response.json();
    if (!response.ok) throw new Error(body.message || "Sign in failed.");
    accessToken = body.accessToken;
    currentRole = body.role;
    mustChangePassword = Boolean(body.mustChangePassword);
    loginForm.reset();
    updateSessionView();
  } catch (error) {
    loginError.textContent = error.message || "Sign in failed.";
  } finally {
    submit.disabled = false;
  }
});

logoutButton.addEventListener("click", async () => {
  try {
    if (accessToken) {
      await fetch("/auth/logout", {
        method: "POST",
        headers: { Authorization: `Bearer ${accessToken}` },
      });
    }
  } finally {
    accessToken = "";
    currentRole = "";
    mustChangePassword = false;
    customerCreated.replaceChildren();
    customerForm.reset();
    clearFieldErrors();
    updateSessionView();
  }
});

passwordChangeForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  passwordChangeError.textContent = "";
  const password = passwordChangeForm.elements.newPassword.value;
  if (password.length < 12) {
    passwordChangeError.textContent = "Password must be at least 12 characters.";
    passwordChangeForm.elements.newPassword.focus();
    return;
  }
  try {
    const response = await fetch("/auth/change-password", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${accessToken}`,
      },
      body: JSON.stringify({ newPassword: password }),
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      return;
    }
    const body = await response.json();
    if (!response.ok) throw new Error(body.message || "Password change failed.");
    mustChangePassword = false;
    passwordChangeForm.reset();
    updateSessionView();
  } catch (error) {
    passwordChangeError.textContent = error.message || "Password change failed.";
  }
});

customerForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  clearFieldErrors();
  customerFormError.textContent = "";
  customerCreated.replaceChildren();
  if (!validateCustomerForm()) return;

  const values = Object.fromEntries(new FormData(customerForm).entries());
  values.name = values.name.trim();
  values.address = values.address.trim();
  values.phone = values.phone.replace(/[\s-]/g, "");
  values.identificationReference = values.identificationReference.trim();
  values.email = values.email.trim();
  customerSubmit.disabled = true;
  try {
    const response = await fetch("/customers", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${accessToken}`,
      },
      body: JSON.stringify(values),
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      return;
    }
    const body = await response.json();
    if (!response.ok) {
      if (!displayApiFieldErrors(body)) throw new Error(body.message || "Registration failed.");
      return;
    }

    customerForm.reset();
    const lines = [
      "Customer registered successfully.",
      `Customer ID: ${body.customerId}`,
      `Username: ${body.login.username}`,
      `Temporary password: ${body.login.temporaryPassword}`,
      "Shown only once. Hand these credentials directly to the customer; do not record or disclose them to anyone else.",
    ];
    customerCreated.textContent = lines.join(" ");
  } catch (error) {
    customerFormError.textContent = error.message || "Registration failed. Please try again.";
  } finally {
    customerSubmit.disabled = false;
  }
});

customerSearchForm.addEventListener("submit", (event) => {
  event.preventDefault();
  customerOffset = 0;
  void loadCustomerPage();
});

customerPreviousPage.addEventListener("click", () => {
  customerOffset = Math.max(0, customerOffset - customerPageSize);
  void loadCustomerPage();
});

customerNextPage.addEventListener("click", () => {
  if (customerOffset + customerPageSize < customerTotal) {
    customerOffset += customerPageSize;
    void loadCustomerPage();
  }
});

customerEditButton.addEventListener("click", () => {
  clearEditFieldErrors();
  customerDetailStatus.textContent = "";
  customerEditForm.hidden = false;
  customerEditButton.hidden = true;
  document.querySelector("#customer-edit-name").focus();
});

document.querySelector("#customer-edit-cancel").addEventListener("click", () => {
  clearEditFieldErrors();
  customerEditForm.hidden = true;
  customerEditButton.hidden = false;
  customerDetailStatus.textContent = "";
});

customerEditForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  clearEditFieldErrors();
  customerDetailStatus.textContent = "";
  const values = Object.fromEntries(new FormData(customerEditForm).entries());
  values.name = values.name.trim();
  values.dateOfBirth = values.dateOfBirth.trim();
  values.address = values.address.trim();
  values.phone = values.phone.replace(/[\s-]/g, "");
  values.identificationReference = values.identificationReference.trim();
  values.email = values.email.trim();

  customerEditSubmit.disabled = true;
  try {
    const response = await fetch(`/customers/${encodeURIComponent(selectedCustomerId)}`, {
      method: "PUT",
      headers: {
        "Content-Type": "application/json",
        Authorization: `Bearer ${accessToken}`,
      },
      body: JSON.stringify(values),
    });
    if (response.status === 401) {
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
      return;
    }
    const body = await response.json();
    if (!response.ok) {
      const message = body.message || "Unable to update customer.";
      if (response.status === 404 || response.status === 409) {
        customerDetailStatus.textContent = message;
      } else if (displayEditFieldErrors(body)) {
        customerDetailStatus.textContent = message;
      } else {
        throw new Error(message);
      }
      return;
    }

    renderCustomerDetails(customerDetailFields, customerDetailAccounts, body);
    for (const [field, value] of [
      ["name", body.name],
      ["dateOfBirth", body.dateOfBirth],
      ["address", body.address],
      ["phone", body.phone],
      ["identificationReference", body.identificationReference],
      ["email", body.email],
    ]) {
      customerEditForm.elements[field].value = value;
    }
    customerEditForm.hidden = true;
    customerEditButton.hidden = false;
    customerDetailStatus.textContent = "Customer details updated successfully.";
    void loadCustomerPage();
  } catch (error) {
    customerDetailStatus.textContent = error.message || "Unable to update customer.";
  } finally {
    customerEditSubmit.disabled = false;
  }
});

// Shared session hook for the other front-end modules: they listen for
// "bms:session" and raise "bms:session-expired" when the server answers 401,
// which returns the page to the sign-in form.
document.addEventListener("bms:session-expired", () => {
  accessToken = "";
  currentRole = "";
  mustChangePassword = false;
  updateSessionView();
  loginError.textContent = "Your session has ended. Please sign in again.";
});

updateSessionView();
