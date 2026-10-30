#pragma once

#include <string>
#include <cstddef>

#include "common/error.hpp"

namespace bms {

// Account numbers are opaque identifiers, but only a short alphanumeric token
// (optionally containing '_' or '-') is accepted at API and service boundaries.
inline void validate_account_number(const std::string& value, const std::string& field) {
    if (value.empty() || value.size() > 32) {
        throw validation_error(field, "Account number must contain 1-32 letters, digits, '-' or '_'.");
    }
    for (const unsigned char character : value) {
        const bool allowed = (character >= 'A' && character <= 'Z') ||
                             (character >= 'a' && character <= 'z') ||
                             (character >= '0' && character <= '9') ||
                             character == '-' || character == '_';
        if (!allowed) {
            throw validation_error(field, "Account number must contain 1-32 letters, digits, '-' or '_'.");
        }
    }
}

inline void validate_maximum_length(const std::string& value, const std::string& field,
                                    std::size_t maximum_bytes) {
    if (value.size() > maximum_bytes) {
        throw validation_error(field, "This field is too long.");
    }
}

}  // namespace bms
