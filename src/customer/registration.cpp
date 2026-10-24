#include "customer/registration.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <sodium.h>

#include "auth/password.hpp"
#include "common/db.hpp"
#include "common/error.hpp"

namespace bms::customer {
namespace {

constexpr char kCustomerAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
constexpr char kPasswordAlphabet[] =
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789!@#$%*-_";

std::string column_string(const Statement& row, int index) {
    const auto* value = row.column_text(index);
    return value == nullptr ? std::string{} : reinterpret_cast<const char*>(value);
}

void ensure_sodium() {
    if (sodium_init() < 0) {
        throw std::runtime_error("Unable to initialize secure random number generation.");
    }
}

std::string generate_customer_code() {
    std::string code = "CUS-";
    for (int i = 0; i < 8; ++i) {
        code.push_back(kCustomerAlphabet[randombytes_uniform(sizeof(kCustomerAlphabet) - 1)]);
    }
    return code;
}

std::string generate_temporary_password() {
    std::string password;
    password.reserve(20);
    for (int i = 0; i < 20; ++i) {
        password.push_back(kPasswordAlphabet[randombytes_uniform(sizeof(kPasswordAlphabet) - 1)]);
    }
    return password;
}

bool code_exists(Database& database, const std::string& code) {
    auto query = database.prepare(
        "SELECT 1 FROM customers WHERE customer_code = ? "
        "UNION ALL SELECT 1 FROM users WHERE username = ? LIMIT 1");
    query->bind(1, code);
    query->bind(2, code);
    return query->step();
}

void reject_duplicate(Database& database, const char* sql, const std::string& value,
                      const std::string& field, const std::string& message) {
    auto query = database.prepare(sql);
    query->bind(1, value);
    if (query->step()) {
        throw conflict("DUPLICATE_CUSTOMER_FIELD", message + ".", field);
    }
}

void reject_duplicate_customer_fields(Database& database, const CustomerInput& input) {
    reject_duplicate(database, "SELECT 1 FROM customers WHERE email = ?", input.email,
                     "email", "Email address is already registered");
    reject_duplicate(database, "SELECT 1 FROM customers WHERE phone = ?", input.phone,
                     "phone", "Phone number is already registered");
    reject_duplicate(database,
                     "SELECT 1 FROM customers WHERE identification_reference = ?",
                     input.identification_reference, "identificationReference",
                     "Identification reference is already registered");
}

}  // namespace

RegistrationResult register_customer(Database& database, const CustomerInput& raw_input,
                                      std::int64_t operator_user_id) {
    const CustomerInput input = validate_customer_input(raw_input);
    ensure_sodium();
    reject_duplicate_customer_fields(database, input);
    const std::string temporary_password = generate_temporary_password();
    const std::string password_hash = auth::hash_password(temporary_password);

    Database::Transaction transaction(database, true);
    reject_duplicate_customer_fields(database, input);

    std::string customer_code;
    bool user_created = false;
    std::int64_t user_id = 0;
    for (int attempt = 0; attempt < 8 && !user_created; ++attempt) {
        customer_code = generate_customer_code();
        if (code_exists(database, customer_code)) continue;

        auto create_user = database.prepare(
            "INSERT INTO users (username, role, status, failed_attempts, password_hash, "
            "must_change_password) VALUES (?, 'customer', 'active', 0, ?, 1)");
        create_user->bind(1, customer_code);
        create_user->bind(2, password_hash);
        try {
            create_user->step();
        } catch (const DbError& error) {
            if ((error.sqlite_rc() & 0xff) != SQLITE_CONSTRAINT) throw;
            continue;
        }

        auto last_id = database.prepare("SELECT last_insert_rowid()");
        last_id->step();
        user_id = last_id->column_int(0);
        user_created = true;
    }
    if (!user_created) {
        throw conflict("CUSTOMER_ID_EXHAUSTED", "Could not allocate a unique customer identifier.");
    }

    auto create_customer = database.prepare(
        "INSERT INTO customers (customer_code, user_id, full_name, email, phone, "
        "identification_reference, address, date_of_birth) VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    create_customer->bind(1, customer_code);
    create_customer->bind(2, user_id);
    create_customer->bind(3, input.name);
    create_customer->bind(4, input.email);
    create_customer->bind(5, input.phone);
    create_customer->bind(6, input.identification_reference);
    create_customer->bind(7, input.address);
    create_customer->bind(8, input.date_of_birth);
    create_customer->step();

    auto last_customer = database.prepare("SELECT last_insert_rowid()");
    last_customer->step();
    const std::int64_t customer_id = last_customer->column_int(0);

    auto audit = database.prepare(
        "INSERT INTO audit_log (user_id, action, entity, entity_id, details) "
        "VALUES (?, 'CUSTOMER_REGISTERED', 'customers', ?, ?)");
    audit->bind(1, operator_user_id);
    audit->bind(2, customer_id);
    audit->bind(3, "customerId=" + customer_code);
    audit->step();

    auto created_at_query =
        database.prepare("SELECT created_at FROM customers WHERE customer_id = ?");
    created_at_query->bind(1, customer_id);
    created_at_query->step();
    const std::string created_at = column_string(*created_at_query, 0);

    transaction.commit();
    return {customer_id, customer_code, created_at, input.name, input.date_of_birth,
            input.address, input.phone, input.identification_reference, input.email,
            customer_code, temporary_password};
}

}  // namespace bms::customer
