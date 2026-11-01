#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace bms {

// What went wrong, independent of HTTP. The API layer maps each kind to a
// status code (api/errors.hpp), so core modules never deal with HTTP.
enum class ErrorKind {
    Validation,       // bad input: missing field, wrong format, negative amount
    Unauthenticated,  // no session, or session expired
    Forbidden,        // authenticated, but the role or ownership check failed
    NotFound,         // the customer, account or record does not exist
    Conflict,         // clashes with current state, e.g. a duplicate interest run
    BusinessRule,     // valid input that breaks a bank rule, e.g. below minimum balance
};

// An expected failure with a stable machine-readable code and a message that is
// safe to show to the user (SADS 3.3). Throw it from any module; the API layer
// turns it into { "code", "message", "field?", "fields?", "minimumBalance?" }. Anything that is not a
// bms::Error is treated as an unexpected internal error.
class Error : public std::runtime_error {
public:
    using FieldErrors = std::vector<std::pair<std::string, std::string>>;

    Error(ErrorKind kind, std::string code, std::string message,
          std::optional<std::string> field = std::nullopt,
          FieldErrors fields = {},
          std::optional<std::string> minimum_balance = std::nullopt);

    ErrorKind kind() const noexcept { return kind_; }
    const std::string& code() const noexcept { return code_; }
    const std::optional<std::string>& field() const noexcept { return field_; }
    const FieldErrors& fields() const noexcept { return fields_; }
    const std::optional<std::string>& minimum_balance() const noexcept {
        return minimum_balance_;
    }

private:
    ErrorKind kind_;
    std::string code_;
    std::optional<std::string> field_;
    FieldErrors fields_;
    std::optional<std::string> minimum_balance_;
};

// Shorthands for the common cases. Business-rule errors take their own code,
// e.g. business_rule("INSUFFICIENT_BALANCE", "...").
Error validation_error(std::string field, std::string message);
Error validation_errors(Error::FieldErrors fields);
Error unauthenticated(std::string message = "Please log in to continue.");
Error forbidden(std::string message = "You are not allowed to perform this action.");
Error not_found(std::string message);
Error conflict(std::string code, std::string message,
               std::optional<std::string> field = std::nullopt);
Error business_rule(std::string code, std::string message,
                    std::optional<std::string> minimum_balance = std::nullopt);

}  // namespace bms
