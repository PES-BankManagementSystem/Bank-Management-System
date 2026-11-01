// S-05: shared API client. Every screen calls the back end through BmsApi so
// the session token is attached in one place and errors look the same
// everywhere ({ code, message, field?, fields? } from the server).
window.BmsApi = (() => {
  let token = "";

  // customer.js owns sign-in and publishes the session on every change.
  document.addEventListener("bms:session", (event) => {
    token = event.detail.accessToken || "";
  });

  class ApiError extends Error {
    constructor(message, {
      status = 0, code = "", field = null, fields = [], minimumBalance = null,
    } = {}) {
      super(message);
      this.name = "ApiError";
      this.status = status;
      this.code = code;
      this.field = field;
      this.fields = fields;
      this.minimumBalance = minimumBalance;
    }
  }

  async function request(path, { method = "GET", body, headers = {} } = {}) {
    const init = { method, headers: { ...headers } };
    if (body !== undefined) {
      init.headers["Content-Type"] = "application/json";
      init.body = JSON.stringify(body);
    }
    if (token) init.headers.Authorization = `Bearer ${token}`;

    let response;
    try {
      response = await fetch(path, init);
    } catch {
      throw new ApiError("Cannot reach the server. Check your connection and try again.");
    }

    const text = await response.text();
    let data = {};
    if (text) {
      try {
        data = JSON.parse(text);
      } catch {
        data = {};
      }
    }

    if (response.status === 401 && token) {
      // Expired, unknown or logged-out token: return to the sign-in form.
      document.dispatchEvent(new CustomEvent("bms:session-expired"));
    }
    if (!response.ok) {
      const fields = Array.isArray(data.fields)
        ? data.fields
        : data.field ? [{ field: data.field, message: data.message }] : [];
      throw new ApiError(data.message || "The request failed. Please try again.", {
        status: response.status,
        code: data.code || "",
        field: data.field || null,
        fields,
        minimumBalance: data.minimumBalance || null,
      });
    }
    return data;
  }

  return {
    ApiError,
    request,
    get: (path) => request(path),
    post: (path, body) => request(path, { method: "POST", body }),
    patch: (path, body) => request(path, { method: "PATCH", body }),
    del: (path) => request(path, { method: "DELETE" }),
    isSignedIn: () => Boolean(token),
  };
})();
