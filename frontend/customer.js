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

let accessToken = "";
let currentRole = "";
let mustChangePassword = false;

const fieldMap = {
  name: "customer-name",
  dateOfBirth: "customer-dob",
  address: "customer-address",
  phone: "customer-phone",
  identificationReference: "customer-id-reference",
  email: "customer-email",
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
  passwordChangeCard.hidden = !accessToken || !mustChangePassword;
  sessionStatus.textContent = accessToken
    ? `Signed in as ${currentRole.replaceAll("_", " ")}.`
    : "You are signed out.";
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

updateSessionView();
