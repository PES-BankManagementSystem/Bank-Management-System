#include "common/error.hpp"

#include <utility>

namespace bms {

Error::Error(ErrorKind kind, std::string code, std::string message,
             std::optional<std::string> field, FieldErrors fields,
             std::optional<std::string> minimum_balance)
    : std::runtime_error(std::move(message)),
      kind_(kind),
      code_(std::move(code)),
      field_(std::move(field)),
      fields_(std::move(fields)),
      minimum_balance_(std::move(minimum_balance)) {}

Error validation_error(std::string field, std::string message) {
    return Error(ErrorKind::Validation, "VALIDATION_ERROR", std::move(message), std::move(field));
}

Error validation_errors(Error::FieldErrors fields) {
    return Error(ErrorKind::Validation, "VALIDATION_ERROR", "One or more fields are invalid.",
                 std::nullopt, std::move(fields));
}

Error unauthenticated(std::string message) {
    return Error(ErrorKind::Unauthenticated, "UNAUTHENTICATED", std::move(message));
}

Error forbidden(std::string message) {
    return Error(ErrorKind::Forbidden, "FORBIDDEN", std::move(message));
}

Error not_found(std::string message) {
    return Error(ErrorKind::NotFound, "NOT_FOUND", std::move(message));
}

Error conflict(std::string code, std::string message, std::optional<std::string> field) {
    return Error(ErrorKind::Conflict, std::move(code), std::move(message), std::move(field));
}

Error business_rule(std::string code, std::string message,
                    std::optional<std::string> minimum_balance) {
    return Error(ErrorKind::BusinessRule, std::move(code), std::move(message),
                 std::nullopt, {}, std::move(minimum_balance));
}

}  // namespace bms
