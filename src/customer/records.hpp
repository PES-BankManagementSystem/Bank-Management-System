#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "customer/validation.hpp"

namespace bms {
class Database;
}

namespace bms::customer {

struct CustomerSummary {
    std::string customer_id;
    std::string name;
    std::string phone;
    std::string email;
    std::string status;
};

struct CustomerAccount {
    std::string account_number;
    std::string type;
    std::string status;
    std::int64_t balance_paise;
};

struct CustomerDetails {
    std::string customer_id;
    std::string name;
    std::string date_of_birth;
    std::string address;
    std::string phone;
    std::string email;
    std::string identification_reference;
    std::string status;
    std::string created_at;
    std::string updated_at;
    std::vector<CustomerAccount> accounts;
};

struct CustomerPage {
    std::vector<CustomerSummary> items;
    std::int64_t total;
};

CustomerPage search_customers(Database& database, const std::string& query,
                              std::int64_t limit, std::int64_t offset);
CustomerDetails get_customer(Database& database, const std::string& customer_id);
CustomerDetails get_customer_for_user(Database& database, std::int64_t user_id);
CustomerDetails update_customer(Database& database, const std::string& customer_id,
                                const CustomerInput& input, std::int64_t operator_user_id);
CustomerDetails deactivate_customer(Database& database, const std::string& customer_id,
                                    std::int64_t operator_user_id);
void require_active_customer(Database& database, const std::string& customer_id);

}  // namespace bms::customer
