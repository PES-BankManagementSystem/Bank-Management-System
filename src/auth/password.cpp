#include "auth/password.hpp"

#include <stdexcept>

#include <sodium.h>

namespace bms::auth {
namespace {

void initialize_sodium() {
    if (sodium_init() < 0) {
        throw std::runtime_error("Unable to initialize secure password services.");
    }
}

}  // namespace

std::string hash_password(const std::string& password) {
    initialize_sodium();
    char hash[crypto_pwhash_STRBYTES]{};
    if (crypto_pwhash_str(hash, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("Unable to hash the password.");
    }
    return hash;
}

bool verify_password(const std::string& password, const std::string& hash) {
    initialize_sodium();
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

}  // namespace bms::auth
