#pragma once

#include <cstdint>
#include <string>

#include "customer/validation.hpp"

namespace bms {
class Database;
}

namespace bms::customer {

struct RegistrationResult {
    std::int64_t customer_id;
    std::string customer_code;
    std::string created_at;
    std::string name;
    std::string date_of_birth;
    std::string address;
    std::string phone;
    std::string identification_reference;
    std::string email;
    std::string username;
    std::string temporary_password;
};

RegistrationResult register_customer(Database& database, const CustomerInput& input,
                                      std::int64_t operator_user_id);

}  // namespace bms::customer
