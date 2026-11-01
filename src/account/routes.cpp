#include "account/routes.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <map>
#include <limits>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <httplib.h>

#include "account/account_types.hpp"
#include "account/account_status.hpp"
#include "account/account_opening.hpp"
#include "account/account_queries.hpp"
#include "api/router.hpp"
#include "auth/rbac.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "common/input_validation.hpp"

namespace bms::account {
namespace {

using RawFields = std::map<std::string, std::string>;

RawFields raw_fields(const std::string& body) {
    RawFields fields;
    std::size_t position = 0;
    const auto skip_space = [&body](std::size_t& offset) {
        while (offset < body.size() &&
               (body[offset] == ' ' || body[offset] == '\n' ||
                body[offset] == '\r' || body[offset] == '\t')) {
            ++offset;
        }
    };
    const auto skip_string = [&body](std::size_t& offset) {
        ++offset;
        while (offset < body.size()) {
            if (body[offset] == '\\') {
                offset += 2;
            } else if (body[offset++] == '"') {
                break;
            }
        }
    };
    const auto skip_value = [&body, &skip_string](std::size_t& offset) {
        if (offset >= body.size()) return;
        if (body[offset] == '"') {
            skip_string(offset);
            return;
        }
        if (body[offset] != '{' && body[offset] != '[') {
            while (offset < body.size() && body[offset] != ',' && body[offset] != '}' &&
                   body[offset] != ']' && body[offset] != ' ' && body[offset] != '\n' &&
                   body[offset] != '\r' && body[offset] != '\t') {
                ++offset;
            }
            return;
        }
        std::vector<char> closings;
        closings.push_back(body[offset++] == '{' ? '}' : ']');
        while (offset < body.size() && !closings.empty()) {
            if (body[offset] == '"') {
                skip_string(offset);
            } else if (body[offset] == '{') {
                closings.push_back('}');
                ++offset;
            } else if (body[offset] == '[') {
                closings.push_back(']');
                ++offset;
            } else if (body[offset] == closings.back()) {
                closings.pop_back();
                ++offset;
            } else {
                ++offset;
            }
        }
    };

    skip_space(position);
    if (position >= body.size() || body[position++] != '{') return fields;
    while (position < body.size()) {
        skip_space(position);
        if (position >= body.size() || body[position] == '}') break;
        const std::size_t key_start = position;
        if (body[position] != '"') break;
        skip_string(position);
        const std::string key_token = body.substr(key_start, position - key_start);
        skip_space(position);
        if (position >= body.size() || body[position++] != ':') break;
        skip_space(position);
        const std::size_t value_start = position;
        skip_value(position);
        const std::string key = key_token.size() >= 2
            ? key_token.substr(1, key_token.size() - 2)
            : std::string{};
        if (!fields.emplace(key, body.substr(value_start, position - value_start)).second) {
            fields[key].clear();
        }
        skip_space(position);
        if (position < body.size() && body[position] == ',') ++position;
    }
    return fields;
}

std::int64_t parse_decimal_units(const std::string& token, std::int64_t multiplier,
                                 std::int64_t maximum) {
    static const std::regex decimal_pattern(R"(^(0|[1-9][0-9]*)(?:\.([0-9]{1,2}))?$)");
    std::smatch match;
    if (!std::regex_match(token, match, decimal_pattern)) {
        throw std::invalid_argument("not a supported decimal");
    }

    std::int64_t whole = 0;
    const std::string whole_text = match[1].str();
    const auto parsed = std::from_chars(whole_text.data(), whole_text.data() + whole_text.size(), whole);
    if (parsed.ec != std::errc{} || whole > maximum / multiplier) {
        throw std::out_of_range("decimal is above its maximum");
    }
    std::int64_t units = whole * multiplier;
    const std::string fraction_text = match[2].matched ? match[2].str() : std::string{};
    if (!fraction_text.empty()) {
        std::int64_t fraction = 0;
        const auto fraction_result =
            std::from_chars(fraction_text.data(), fraction_text.data() + fraction_text.size(), fraction);
        if (fraction_result.ec != std::errc{}) throw std::out_of_range("decimal fraction is invalid");
        if (fraction_text.size() == 1) fraction *= 10;
        units += fraction;
    }
    if (units > maximum) throw std::out_of_range("decimal is above its maximum");
    return units;
}

std::string json_string(const api::Json& body, const std::string& key,
                        Error::FieldErrors& errors) {
    if (!body.contains(key)) {
        errors.emplace_back(key, "This field is required.");
        return {};
    }
    if (!body[key].is_string()) {
        errors.emplace_back(key, "This field must be a string.");
        return {};
    }
    return body[key].get<std::string>();
}

std::int64_t json_decimal(const api::Json& body, const RawFields& raw,
                          const std::string& key, std::int64_t multiplier,
                          std::int64_t maximum, Error::FieldErrors& errors) {
    if (!body.contains(key)) {
        errors.emplace_back(key, "This field is required.");
        return 0;
    }
    if (!body[key].is_number() || raw.find(key) == raw.end() || raw.at(key).empty()) {
        errors.emplace_back(key, "This field must be a decimal number with up to two decimal places.");
        return 0;
    }
    try {
        return parse_decimal_units(raw.at(key), multiplier, maximum);
    } catch (const std::exception&) {
        const bool interest_rate = maximum == kMaximumInterestRateBasisPoints;
        const std::string max_value = interest_rate ? "20.00" : "10,000,000.00";
        const std::string unit = interest_rate ? "percent per annum" : "INR";
        errors.emplace_back(key, "Enter a non-negative value with up to two decimal places, not above " +
                                     max_value + " " + unit + ".");
        return 0;
    }
}

AccountTypeInput read_input(const api::Json& body, const std::string& raw_body,
                            bool update) {
    const std::set<std::string> editable = {
        "name", "minimumBalance", "interestRate", "dailyWithdrawalLimit"};
    const std::set<std::string> immutable = {"typeId", "id", "createdAt"};
    Error::FieldErrors errors;
    for (auto it = body.begin(); it != body.end(); ++it) {
        if (immutable.count(it.key()) != 0) {
            errors.emplace_back(it.key(), "This field cannot be changed.");
        } else if (editable.count(it.key()) == 0) {
            errors.emplace_back(it.key(), "Unknown field.");
        }
    }
    const RawFields raw = raw_fields(raw_body);
    for (const auto& entry : raw) {
        if (entry.second.empty()) errors.emplace_back(entry.first, "Duplicate fields are not allowed.");
    }
    if (!update) {
        for (const std::string& field : immutable) {
            if (body.contains(field)) errors.emplace_back(field, "This field cannot be supplied.");
        }
    }

    AccountTypeInput input{
        json_string(body, "name", errors),
        json_decimal(body, raw, "minimumBalance", 100, kMaximumAccountTypeMoneyPaise, errors),
        json_decimal(body, raw, "interestRate", 100, kMaximumInterestRateBasisPoints, errors),
        json_decimal(body, raw, "dailyWithdrawalLimit", 100, kMaximumAccountTypeMoneyPaise, errors),
    };
    if (!errors.empty()) throw validation_errors(std::move(errors));
    return input;
}

OpenAccountInput read_open_account_input(const api::Json& body,
                                         const std::string& raw_body) {
    const std::set<std::string> accepted = {"customerId", "accountType", "initialDeposit"};
    Error::FieldErrors errors;
    for (auto it = body.begin(); it != body.end(); ++it) {
        if (accepted.count(it.key()) == 0) {
            errors.emplace_back(it.key(), "Unknown field.");
        }
    }

    const RawFields raw = raw_fields(raw_body);
    for (const auto& entry : raw) {
        if (entry.second.empty()) {
            errors.emplace_back(entry.first, "Duplicate fields are not allowed.");
        }
    }

    std::string customer_id;
    if (!body.contains("customerId")) {
        errors.emplace_back("customerId", "This field is required.");
    } else if (!body["customerId"].is_string()) {
        errors.emplace_back("customerId", "This field must be a string.");
    } else {
        customer_id = body["customerId"].get<std::string>();
        const auto first = std::find_if_not(customer_id.begin(), customer_id.end(),
                                            [](unsigned char ch) {
                                                return std::isspace(ch) != 0;
                                            });
        const auto last = std::find_if_not(customer_id.rbegin(), customer_id.rend(),
                                           [](unsigned char ch) {
                                               return std::isspace(ch) != 0;
                                           }).base();
        customer_id = first < last ? std::string(first, last) : std::string{};
        if (customer_id.empty()) {
            errors.emplace_back("customerId", "Customer ID cannot be blank.");
        }
    }

    std::int64_t account_type_id = 0;
    if (!body.contains("accountType")) {
        errors.emplace_back("accountType", "This field is required.");
    } else if (!body["accountType"].is_number_integer() ||
               raw.find("accountType") == raw.end()) {
        errors.emplace_back("accountType", "Select an account type ID from the account-type list.");
    } else {
        const std::string token = raw.at("accountType");
        const auto parsed = std::from_chars(token.data(), token.data() + token.size(),
                                            account_type_id);
        if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size() ||
            account_type_id <= 0) {
            errors.emplace_back("accountType", "Select a valid account type ID.");
        }
    }

    std::int64_t initial_deposit_paise = 0;
    if (!body.contains("initialDeposit")) {
        errors.emplace_back("initialDeposit", "This field is required.");
    } else {
        std::string token;
        if (body["initialDeposit"].is_number() &&
            raw.find("initialDeposit") != raw.end()) {
            token = raw.at("initialDeposit");
        } else if (body["initialDeposit"].is_string()) {
            token = body["initialDeposit"].get<std::string>();
        } else {
            errors.emplace_back(
                "initialDeposit",
                "Enter a decimal number with up to two decimal places, without exponent notation.");
        }

        if (!token.empty()) {
            static const std::regex amount_pattern(
                R"(^(0|[1-9][0-9]*)(?:\.([0-9]{1,2}))?$)");
            std::smatch match;
            if (!std::regex_match(token, match, amount_pattern)) {
                errors.emplace_back(
                    "initialDeposit",
                    "Enter a non-negative decimal amount with up to two decimal places and no exponent notation.");
            } else {
                std::int64_t whole = 0;
                const std::string whole_text = match[1].str();
                const auto parsed =
                    std::from_chars(whole_text.data(), whole_text.data() + whole_text.size(), whole);
                std::int64_t fraction = 0;
                const std::string fraction_text =
                    match[2].matched ? match[2].str() : std::string{};
                if (!fraction_text.empty()) {
                    const auto fraction_parsed = std::from_chars(
                        fraction_text.data(), fraction_text.data() + fraction_text.size(), fraction);
                    if (fraction_parsed.ec != std::errc{}) {
                        errors.emplace_back("initialDeposit", "Enter a valid decimal amount.");
                    }
                    if (fraction_text.size() == 1) fraction *= 10;
                }
                if (parsed.ec != std::errc{} ||
                    whole > (std::numeric_limits<std::int64_t>::max() - fraction) / 100) {
                    errors.emplace_back("initialDeposit", "The initial deposit is too large.");
                } else {
                    initial_deposit_paise = whole * 100 + fraction;
                }
            }
        }
    }

    if (!errors.empty()) throw validation_errors(std::move(errors));
    return {std::move(customer_id), account_type_id, initial_deposit_paise};
}

std::int64_t parse_type_id(const std::string& value) {
    std::int64_t type_id = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const auto result = std::from_chars(first, last, type_id);
    if (value.empty() || result.ec != std::errc{} || result.ptr != last || type_id <= 0) {
        throw not_found("Account type was not found.");
    }
    return type_id;
}

std::string format_money(std::int64_t paise) {
    const std::int64_t rupees = paise / 100;
    const std::int64_t remainder = paise % 100;
    return std::to_string(rupees) + "." +
           (remainder < 10 ? "0" : "") + std::to_string(remainder);
}

std::string format_rate(std::int64_t basis_points) {
    const std::int64_t percent = basis_points / 100;
    const std::int64_t fraction = basis_points % 100;
    return std::to_string(percent) + "." +
           (fraction < 10 ? "0" : "") + std::to_string(fraction);
}

api::Json account_type_json(const AccountType& type) {
    return {
        {"typeId", type.type_id},
        {"name", type.name},
        {"minimumBalance", format_money(type.minimum_balance_paise)},
        {"interestRate", format_rate(type.interest_rate_basis_points)},
        {"dailyWithdrawalLimit", format_money(type.daily_withdrawal_limit_paise)},
        {"createdAt", type.created_at},
    };
}

api::Json account_status_json(const AccountStatus& account) {
    const std::int64_t rupees = account.balance_paise / 100;
    const std::int64_t paise = account.balance_paise % 100;
    return {
        {"accountNumber", account.account_number},
        {"type", account.type},
        {"status", account.status},
        {"balance", std::to_string(rupees) + "." +
                        (paise < 10 ? "0" : "") + std::to_string(paise)},
    };
}

api::Json account_summary_json(const AccountSummary& account) {
    return {
        {"accountNumber", account.account_number},
        {"accountType", account.account_type},
        {"status", account.status},
        {"balance", format_money(account.balance_paise)},
    };
}

api::Json account_balance_json(const AccountBalance& account) {
    return {
        {"accountNumber", account.account_number},
        {"accountType", account.account_type},
        {"status", account.status},
        {"balance", format_money(account.balance_paise)},
        {"currency", "INR"},
        {"asOf", account.as_of},
    };
}

}  // namespace

void register_routes(api::Router& router, Database& database) {
    router.post("/accounts", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::OpenAccount);
        const OpenAccountInput input =
            read_open_account_input(request.json(), request.raw().body);
        const OpenAccountResult opened =
            open_account(database, input, principal.user_id);
        AccountStatus account{opened.account_number, opened.account_type,
                              opened.status, opened.balance_paise};
        api::Response response = api::Response::created({
            {"account", account_status_json(account)},
            {"transaction",
             {{"id", opened.transaction_id},
              {"referenceNumber", opened.transaction_reference},
              {"type", "deposit"},
              {"amount", format_money(opened.balance_paise)},
              {"resultingBalance", format_money(opened.balance_paise)},
              {"operatorId", principal.user_id},
              {"createdAt", opened.transaction_created_at}}},
        });
        response.headers.emplace("Location", "/accounts/" + opened.account_number);
        return response;
    });

    router.get("/customers/me/accounts", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::ViewOwnAccounts);

        api::Json accounts = api::Json::array();
        for (const AccountSummary& account :
             list_customer_accounts(database, principal.user_id)) {
            accounts.push_back(account_summary_json(account));
        }
        const std::size_t count = accounts.size();
        return api::Response::ok({
            {"accounts", std::move(accounts)},
            {"count", count},
        });
    });

    router.get("/accounts/:acc/balance", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::ViewAccount);
        const std::string& account_number = request.param("acc");
        validate_account_number(account_number, "account_number");
        auth::rbac::require_account_access(database, principal, account_number);
        return api::Response::ok(
            account_balance_json(get_account_balance(database, account_number)));
    });

    router.patch("/accounts/:acc/status", [&database](const api::Request& request) {
        const auth::Principal principal = auth::authenticate(request, database);
        const api::Json& body = request.json();
        Error::FieldErrors errors;
        for (auto it = body.begin(); it != body.end(); ++it) {
            if (it.key() != "action") errors.emplace_back(it.key(), "Unknown field.");
        }
        std::string action;
        if (!body.contains("action")) {
            errors.emplace_back("action", "This field is required.");
        } else if (!body["action"].is_string()) {
            errors.emplace_back("action", "This field must be a string.");
        } else {
            action = body["action"].get<std::string>();
            if (action != "freeze" && action != "unfreeze" && action != "close") {
                errors.emplace_back("action", "Action must be freeze, unfreeze, or close.");
            }
        }
        if (!errors.empty()) throw validation_errors(std::move(errors));

        auth::rbac::require_permission(
            request, database, principal,
            action == "close" ? auth::rbac::Operation::CloseAccount
                              : auth::rbac::Operation::FreezeAccount);
        const AccountStatus account =
            change_status(database, request.param("acc"), action, principal.user_id);
        return api::Response::ok({{"account", account_status_json(account)}});
    });

    router.get("/account-types", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::ViewAccountTypes);
        api::Json types = api::Json::array();
        for (const AccountType& type : list_account_types(database, principal.user_id)) {
            types.push_back(account_type_json(type));
        }
        return api::Response::ok({{"accountTypes", std::move(types)}});
    });

    router.post("/account-types", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::ManageAccountTypes);
        const AccountType type =
            create_account_type(database, read_input(request.json(), request.raw().body, false),
                                principal.user_id);
        api::Response response = api::Response::created({{"accountType", account_type_json(type)}});
        response.headers.emplace("Location", "/account-types/" + std::to_string(type.type_id));
        return response;
    });

    router.put("/account-types/:id", [&database](const api::Request& request) {
        const auth::Principal principal =
            auth::rbac::require(request, database, auth::rbac::Operation::ManageAccountTypes);
        const std::int64_t type_id = parse_type_id(request.param("id"));
        const AccountType type = update_account_type(
            database, type_id, read_input(request.json(), request.raw().body, true), principal.user_id);
        return api::Response::ok({{"accountType", account_type_json(type)}});
    });
}

}  // namespace bms::account
