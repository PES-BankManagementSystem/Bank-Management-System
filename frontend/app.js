const form = document.querySelector("#deposit-form");
const result = document.querySelector("#result");
const submitButton = document.querySelector("#submit-button");

form.addEventListener("submit", async (event) => {
  event.preventDefault();
  result.textContent = "";
  result.className = "result";

  const accountNumber = form.elements.account_number.value.trim();
  const amount = form.elements.amount.value.trim();
  const confirmed = window.confirm(
    `Confirm deposit of ₹${amount} into account ${accountNumber}?\n\nNo changes are made until you confirm.`
  );
  if (!confirmed) return;

  submitButton.disabled = true;
  submitButton.textContent = "Processing…";
  try {
    const body = await window.BmsApi.post("/transactions", {
      type: "deposit",
      account_number: accountNumber,
      amount,
    });

    result.textContent =
      `Deposit of ₹${body.transaction.amount} into ${body.transaction.account_number} succeeded. ` +
      `New balance: ₹${body.balance}.`;
    result.className = "result success";
    form.reset();
  } catch (error) {
    result.textContent = error.code === "ACCOUNT_NOT_ACTIVE"
      ? `The deposit was not completed. ${error.message} No balance or transaction history was changed.`
      : error.message || "The deposit could not be completed. Please try again.";
    result.className = "result error";
  } finally {
    submitButton.disabled = false;
    submitButton.textContent = "Review deposit";
  }
});
