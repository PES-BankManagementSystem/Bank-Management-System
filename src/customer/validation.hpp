#pragma once

#include <string>

#include "common/error.hpp"

namespace bms::customer {

struct CustomerInput {
    std::string name;
    std::string date_of_birth;
    std::string address;
    std::string phone;
    std::string identification_reference;
    std::string email;
};

CustomerInput validate_customer_input(CustomerInput input,
                                      Error::FieldErrors initial_errors = {});

}  // namespace bms::customer
