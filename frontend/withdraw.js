const withdrawForm = document.querySelector("#withdraw-form");
const withdrawResult = document.querySelector("#withdraw-result");
const withdrawSubmitButton = document.querySelector("#withdraw-submit-button");

withdrawForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  withdrawResult.textContent = "";
  withdrawResult.className = "result";

  const accountNumber = withdrawForm.elements.account_number.value.trim();
  const amount = withdrawForm.elements.amount.value.trim();
  const confirmed = window.confirm(
    `Confirm withdrawal of ₹${amount} from account ${accountNumber}?\n\nNo changes are made until you confirm.`
  );
  if (!confirmed) return;

  withdrawSubmitButton.disabled = true;
  withdrawSubmitButton.textContent = "Processing…";
  try {
    const body = await window.BmsApi.post("/transactions", {
      type: "withdraw",
      account_number: accountNumber,
      amount,
    });

    withdrawResult.textContent =
      `Withdrawal of ₹${body.transaction.amount} from ${body.transaction.account_number} succeeded. ` +
      `New balance: ₹${body.balance}.`;
    withdrawResult.className = "result success";
    withdrawForm.reset();
  } catch (error) {
    withdrawResult.textContent = error.message || "The withdrawal could not be completed. Please try again.";
    withdrawResult.className = "result error";
  } finally {
    withdrawSubmitButton.disabled = false;
    withdrawSubmitButton.textContent = "Review withdrawal";
  }
});
