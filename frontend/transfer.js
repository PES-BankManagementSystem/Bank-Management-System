const transferForm = document.querySelector("#transfer-form");
const transferResult = document.querySelector("#transfer-result");
const transferSubmitButton = document.querySelector("#transfer-submit-button");

transferForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  transferResult.textContent = "";
  transferResult.className = "result";

  const fromAccount = transferForm.elements.from_account_number.value.trim();
  const toAccount = transferForm.elements.to_account_number.value.trim();
  const amount = transferForm.elements.amount.value.trim();
  const confirmed = window.confirm(
    `Confirm transfer of ₹${amount} from ${fromAccount} to ${toAccount}?\n\nNo changes are made until you confirm.`
  );
  if (!confirmed) return;

  transferSubmitButton.disabled = true;
  transferSubmitButton.textContent = "Processing…";
  try {
    const body = await window.BmsApi.post("/transfers", {
      from_account_number: fromAccount,
      to_account_number: toAccount,
      amount,
    });

    transferResult.textContent =
      `Transfer succeeded. Reference: ${body.transfer_reference}. ` +
      `${fromAccount} balance: ₹${body.from_balance}; ${toAccount} balance: ₹${body.to_balance}.`;
    transferResult.className = "result success";
    transferForm.reset();
  } catch (error) {
    transferResult.textContent = error.code === "ACCOUNT_NOT_ACTIVE"
      ? `The transfer was not completed. ${error.message} Neither balance nor transaction history was changed.`
      : error.message || "The transfer could not be completed. Please try again.";
    transferResult.className = "result error";
  } finally {
    transferSubmitButton.disabled = false;
    transferSubmitButton.textContent = "Review transfer";
  }
});
