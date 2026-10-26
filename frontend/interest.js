const interestForm = document.querySelector("#interest-form");
const interestResult = document.querySelector("#interest-result");
const interestSubmitButton = document.querySelector("#interest-submit-button");

interestForm.addEventListener("submit", async (event) => {
  event.preventDefault();
  interestResult.textContent = "";
  interestResult.className = "result";

  const period = interestForm.elements.period.value;
  const confirmed = window.confirm(
    `Credit interest for ${period} to all eligible accounts? This can only be run once per period.`
  );
  if (!confirmed) return;

  interestSubmitButton.disabled = true;
  interestSubmitButton.textContent = "Processing…";
  try {
    const body = await window.BmsApi.post("/interest/run", { period });

    interestResult.textContent =
      `Interest for ${body.period} was credited to ${body.accounts_credited} account(s). ` +
      `Total credited: ₹${body.total_interest}.`;
    interestResult.className = "result success";
    interestForm.reset();
  } catch (error) {
    interestResult.textContent = error.message || "Interest could not be run. Please try again.";
    interestResult.className = "result error";
  } finally {
    interestSubmitButton.disabled = false;
    interestSubmitButton.textContent = "Run interest";
  }
});
