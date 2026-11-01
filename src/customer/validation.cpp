#include "customer/validation.hpp"

#include "common/calendar.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <optional>
#include <regex>
#include <string>

namespace bms::customer {
namespace {

std::string trim(std::string value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string normalize_phone(const std::string& phone) {
    std::string normalized;
    normalized.reserve(phone.size());
    for (const unsigned char ch : phone) {
        if (ch != '-' && std::isspace(ch) == 0) normalized.push_back(static_cast<char>(ch));
    }
    return normalized;
}

std::optional<std::size_t> utf8_length(const std::string& value) {
    std::size_t count = 0;
    for (std::size_t i = 0; i < value.size();) {
        const unsigned char lead = static_cast<unsigned char>(value[i]);
        std::size_t width = 0;
        std::uint32_t codepoint = 0;
        if (lead <= 0x7f) {
            width = 1;
            codepoint = lead;
        } else if ((lead & 0xe0) == 0xc0) {
            width = 2;
            codepoint = lead & 0x1f;
        } else if ((lead & 0xf0) == 0xe0) {
            width = 3;
            codepoint = lead & 0x0f;
        } else if ((lead & 0xf8) == 0xf0) {
            width = 4;
            codepoint = lead & 0x07;
        } else {
            return std::nullopt;
        }
        if (i + width > value.size()) return std::nullopt;
        for (std::size_t offset = 1; offset < width; ++offset) {
            const unsigned char continuation = static_cast<unsigned char>(value[i + offset]);
            if ((continuation & 0xc0) != 0x80) return std::nullopt;
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if ((width == 2 && codepoint < 0x80) ||
            (width == 3 && codepoint < 0x800) ||
            (width == 4 && codepoint < 0x10000) ||
            (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) {
            return std::nullopt;
        }
        ++count;
        i += width;
    }
    return count;
}

bool has_error(const Error::FieldErrors& errors, const std::string& field) {
    return std::any_of(errors.begin(), errors.end(), [&field](const auto& error) {
        return error.first == field;
    });
}

bool valid_date(const std::string& date, int& year, int& month, int& day) {
    static const std::regex pattern(R"(^([0-9]{4})-([0-9]{2})-([0-9]{2})$)");
    std::smatch match;
    if (!std::regex_match(date, match, pattern)) return false;

    year = std::stoi(match[1].str());
    month = std::stoi(match[2].str());
    day = std::stoi(match[3].str());
    if (year < 1 || month < 1 || month > 12) return false;

    static constexpr int month_lengths[] = {31, 28, 31, 30, 31, 30,
                                             31, 31, 30, 31, 30, 31};
    int days_in_month = month_lengths[month - 1];
    const bool leap_year = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    if (month == 2 && leap_year) ++days_in_month;
    return day >= 1 && day <= days_in_month;
}

}  // namespace

CustomerInput validate_customer_input(CustomerInput input,
                                      Error::FieldErrors initial_errors) {
    input.name = trim(std::move(input.name));
    input.date_of_birth = trim(std::move(input.date_of_birth));
    input.address = trim(std::move(input.address));
    input.phone = normalize_phone(trim(std::move(input.phone)));
    input.identification_reference = trim(std::move(input.identification_reference));
    input.email = trim(std::move(input.email));

    auto add_if_missing_error = [&initial_errors](const std::string& field,
                                                   const std::string& message) {
        if (!has_error(initial_errors, field)) initial_errors.emplace_back(field, message);
    };

    if (input.name.empty()) {
        add_if_missing_error("name", "Name is required.");
    } else if (input.name.size() > 100 ||
               input.name.find_first_not_of(
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz -'.") != std::string::npos ||
               input.name.find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz") ==
                   std::string::npos) {
        add_if_missing_error("name", "Name must be 1-100 characters and contain a letter.");
    }

    int birth_year = 0;
    int birth_month = 0;
    int birth_day = 0;
    if (input.date_of_birth.empty()) {
        add_if_missing_error("dateOfBirth", "Date of birth is required.");
    } else if (!valid_date(input.date_of_birth, birth_year, birth_month, birth_day)) {
        add_if_missing_error("dateOfBirth", "Date of birth must be a real date in YYYY-MM-DD format.");
    } else {
        const std::tm today = bms::calendar::to_local(std::time(nullptr));
        const int current_year = today.tm_year + 1900;
        const int current_month = today.tm_mon + 1;
        const int current_day = today.tm_mday;
        const bool birth_is_today_or_future =
            birth_year > current_year ||
            (birth_year == current_year && birth_month > current_month) ||
            (birth_year == current_year && birth_month == current_month && birth_day >= current_day);
        const int age = current_year - birth_year -
                        ((current_month < birth_month ||
                          (current_month == birth_month && current_day < birth_day)) ? 1 : 0);
        if (birth_is_today_or_future) {
            add_if_missing_error("dateOfBirth", "Date of birth must be in the past.");
        } else if (age > 120) {
            add_if_missing_error("dateOfBirth", "Age must not exceed 120 years.");
        }
    }

    const auto address_length = utf8_length(input.address);
    if (input.address.empty()) {
        add_if_missing_error("address", "Address is required.");
    } else if (!address_length || *address_length > 255) {
        add_if_missing_error("address", "Address must be valid UTF-8 and not exceed 255 characters.");
    }

    if (input.phone.empty()) {
        add_if_missing_error("phone", "Phone number is required.");
    } else {
        const std::string digits = input.phone.front() == '+' ? input.phone.substr(1) : input.phone;
        const bool all_digits = !digits.empty() &&
            std::all_of(digits.begin(), digits.end(), [](unsigned char ch) {
                return std::isdigit(ch) != 0;
            });
        if (!all_digits || digits.size() < 10 || digits.size() > 15) {
            add_if_missing_error("phone", "Phone must contain 10-15 digits, optionally starting with +.");
        }
    }

    if (input.identification_reference.empty()) {
        add_if_missing_error("identificationReference", "Identification reference is required.");
    } else if (input.identification_reference.size() > 50 ||
               input.identification_reference.find_first_not_of(
                   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-") !=
                   std::string::npos) {
        add_if_missing_error("identificationReference",
                             "Identification reference must be 1-50 alphanumeric characters or hyphens.");
    }

    const auto at = input.email.find('@');
    const auto dot = input.email.find('.', at == std::string::npos ? 0 : at + 1);
    if (input.email.empty()) {
        add_if_missing_error("email", "Email is required.");
    } else if (input.email.size() > 254 || at == 0 || at == std::string::npos ||
               dot == std::string::npos || dot <= at + 1 || dot + 1 >= input.email.size() ||
               input.email.find_first_of(" \t\r\n") != std::string::npos) {
        add_if_missing_error("email", "Enter a valid email address.");
    }

    if (!initial_errors.empty()) throw validation_errors(std::move(initial_errors));
    return input;
}

}  // namespace bms::customer
