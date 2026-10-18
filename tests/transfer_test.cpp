#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include "api/server.hpp"
#include "common/config.hpp"
#include "common/db.hpp"
#include "common/error.hpp"
#include "support/check.hpp"
#include "support/seeded_db.hpp"
#include "transaction/routes.hpp"
#include "transaction/transfer.hpp"

namespace {

struct ErrorSnapshot {
    std::string code;
    std::string message;
    std::optional<std::string> field;
};

ErrorSnapshot capture_error(const std::function<void()>& operation) {
    try {
        operation();
    } catch (const bms::Error& error) {
        return {error.code(), error.what(), error.field()};
    }
    return {};
}

std::int64_t balance(bms::Database& database, const std::string& number) {
    auto query = database.prepare("SELECT balance FROM accounts WHERE account_number = ?");
    query->bind(1, number);
    query->step();
    return query->column_int(0);
}

std::int64_t row_count(bms::Database& database, const std::string& table) {
    auto query = database.prepare("SELECT COUNT(*) FROM " + table);
    query->step();
    return query->column_int(0);
}

void test_success_updates_both_balances_and_links_records_and_audits() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const std::int64_t total_before = balance(database, "ACC001") + balance(database, "ACC004");
    const auto result = bms::transaction::transfer(database, "ACC001", "ACC004", "125.50", 2);

    CHECK_EQ(result.amount_paise, 12550);
    CHECK_EQ(result.source_balance_paise, 4987450);
    CHECK_EQ(result.destination_balance_paise, 10012550);
    CHECK_EQ(balance(database, "ACC001"), result.source_balance_paise);
    CHECK_EQ(balance(database, "ACC004"), result.destination_balance_paise);
    CHECK_EQ(balance(database, "ACC001") + balance(database, "ACC004"), total_before);
    CHECK(result.transfer_reference.find("TRF-") == 0);

    auto linked = database.prepare(
        "SELECT transaction_id, account_id, operator_id, type, amount, resulting_balance, "
        "created_at, transfer_ref FROM transactions WHERE transfer_ref = ? ORDER BY type");
    linked->bind(1, result.transfer_reference);
    CHECK(linked->step());
    CHECK_EQ(linked->column_int(0), result.destination_transaction_id);
    CHECK_EQ(linked->column_int(1), result.destination_account_id);
    CHECK_EQ(linked->column_int(2), 2);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(linked->column_text(3))), std::string("transfer_credit"));
    CHECK_EQ(linked->column_int(4), 12550);
    CHECK_EQ(linked->column_int(5), result.destination_balance_paise);
    CHECK(linked->column_text(6) != nullptr);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(linked->column_text(7))), result.transfer_reference);
    CHECK(linked->step());
    CHECK_EQ(linked->column_int(0), result.source_transaction_id);
    CHECK_EQ(linked->column_int(1), result.source_account_id);
    CHECK_EQ(linked->column_int(2), 2);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(linked->column_text(3))), std::string("transfer_debit"));
    CHECK_EQ(linked->column_int(4), 12550);
    CHECK_EQ(linked->column_int(5), result.source_balance_paise);
    CHECK(linked->column_text(6) != nullptr);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(linked->column_text(7))), result.transfer_reference);
    CHECK(!linked->step());

    auto audits = database.prepare(
        "SELECT user_id, action, entity, entity_id, details FROM audit_log "
        "WHERE action IN ('TRANSFER_OUT', 'TRANSFER_IN') ORDER BY action");
    CHECK(audits->step());
    CHECK_EQ(audits->column_int(0), 2);
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audits->column_text(1))), std::string("TRANSFER_IN"));
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audits->column_text(2))), std::string("accounts"));
    CHECK_EQ(audits->column_int(3), result.destination_account_id);
    CHECK(std::string(reinterpret_cast<const char*>(audits->column_text(4))).find(result.transfer_reference) != std::string::npos);
    CHECK(audits->step());
    CHECK_EQ(std::string(reinterpret_cast<const char*>(audits->column_text(1))), std::string("TRANSFER_OUT"));
    CHECK_EQ(audits->column_int(3), result.source_account_id);
    CHECK(!audits->step());
}

void test_same_account_and_invalid_amounts_are_rejected() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto same = capture_error([&] {
        bms::transaction::transfer(database, "ACC001", "ACC001", "10.00", 2);
    });
    CHECK_EQ(same.code, std::string("VALIDATION_ERROR"));
    CHECK_EQ(same.field.value_or(""), std::string("to_account_number"));

    for (const std::string amount : {"0", "-1.00", "1.001", "abc"}) {
        const auto error = capture_error([&] {
            bms::transaction::transfer(database, "ACC001", "ACC004", amount, 2);
        });
        CHECK_EQ(error.code, std::string("VALIDATION_ERROR"));
        CHECK_EQ(error.field.value_or(""), std::string("amount"));
    }
}

void test_account_status_and_unknown_accounts_are_rejected_without_changes() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    for (const auto& [from, to, expected_text] : {
             std::tuple<std::string, std::string, std::string>{"ACC002", "ACC001", "Source account ACC002 is frozen"},
             {"ACC001", "ACC002", "Destination account ACC002 is frozen"},
             {"ACC001", "ACC003", "Destination account ACC003 is closed"}}) {
        const auto source_before = balance(database, from);
        const auto destination_before = balance(database, to);
        const auto error = capture_error([&, from_number = std::string(from), to_number = std::string(to)] {
            bms::transaction::transfer(database, from_number, to_number, "10.00", 2);
        });
        CHECK_EQ(error.code, std::string("ACCOUNT_NOT_ACTIVE"));
        CHECK(error.message.find(expected_text) != std::string::npos);
        CHECK_EQ(balance(database, from), source_before);
        CHECK_EQ(balance(database, to), destination_before);
    }

    for (const auto& [from, to] : {std::pair<std::string, std::string>{"UNKNOWN", "ACC001"},
                                   {"ACC001", "UNKNOWN"}}) {
        const auto error = capture_error([&, from_number = std::string(from), to_number = std::string(to)] {
            bms::transaction::transfer(database, from_number, to_number, "10.00", 2);
        });
        CHECK_EQ(error.code, std::string("NOT_FOUND"));
        CHECK(error.message.find("UNKNOWN") != std::string::npos);
    }
}

void test_source_minimum_boundary_is_enforced() {
    bms::test::SeededDatabase allowed_fixture;
    auto& allowed_db = allowed_fixture.database();
    const auto allowed = bms::transaction::transfer(allowed_db, "ACC001", "ACC004", "49000.00", 2);
    CHECK_EQ(allowed.source_balance_paise, 100000);

    bms::test::SeededDatabase rejected_fixture;
    auto& rejected_db = rejected_fixture.database();
    const auto before = balance(rejected_db, "ACC001");
    const auto error = capture_error([&] {
        bms::transaction::transfer(rejected_db, "ACC001", "ACC004", "49000.01", 2);
    });
    CHECK_EQ(error.code, std::string("BELOW_MINIMUM_BALANCE"));
    CHECK(error.message.find("1000.00") != std::string::npos);
    CHECK(error.message.find("49000.00") != std::string::npos);
    CHECK_EQ(balance(rejected_db, "ACC001"), before);
}

void test_generated_references_are_unique_across_transfers() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto first = bms::transaction::transfer(database, "ACC001", "ACC004", "10.00", 2);
    const auto second = bms::transaction::transfer(database, "ACC004", "ACC001", "10.00", 2);
    CHECK(first.transfer_reference != second.transfer_reference);
    CHECK_EQ(first.transfer_reference,
             bms::transaction::make_transfer_reference(first.created_at, first.source_transaction_id));
    CHECK_EQ(second.transfer_reference,
             bms::transaction::make_transfer_reference(second.created_at, second.source_transaction_id));
}

void test_failure_after_debit_before_credit_rolls_back_everything() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto source_before = balance(database, "ACC001");
    const auto destination_before = balance(database, "ACC004");
    const auto transactions_before = row_count(database, "transactions");
    const auto audits_before = row_count(database, "audit_log");
    database.exec(R"(
        CREATE TRIGGER fail_transfer_credit
        BEFORE UPDATE ON accounts WHEN OLD.account_id = 4 AND NEW.balance <> OLD.balance
        BEGIN SELECT RAISE(ABORT, 'forced credit failure'); END;
    )");

    CHECK_THROWS_AS(bms::transaction::transfer(database, "ACC001", "ACC004", "10.00", 2), bms::DbError);
    CHECK_EQ(balance(database, "ACC001"), source_before);
    CHECK_EQ(balance(database, "ACC004"), destination_before);
    CHECK_EQ(row_count(database, "transactions"), transactions_before);
    CHECK_EQ(row_count(database, "audit_log"), audits_before);
}

void test_failure_after_credit_before_audit_rolls_back_everything() {
    bms::test::SeededDatabase fixture;
    auto& database = fixture.database();
    const auto source_before = balance(database, "ACC001");
    const auto destination_before = balance(database, "ACC004");
    const auto transactions_before = row_count(database, "transactions");
    const auto audits_before = row_count(database, "audit_log");
    database.exec(R"(
        CREATE TRIGGER fail_transfer_audit
        BEFORE INSERT ON audit_log WHEN NEW.action = 'TRANSFER_OUT'
        BEGIN SELECT RAISE(ABORT, 'forced audit failure'); END;
    )");

    CHECK_THROWS_AS(bms::transaction::transfer(database, "ACC001", "ACC004", "10.00", 2), bms::DbError);
    CHECK_EQ(balance(database, "ACC001"), source_before);
    CHECK_EQ(balance(database, "ACC004"), destination_before);
    CHECK_EQ(row_count(database, "transactions"), transactions_before);
    CHECK_EQ(row_count(database, "audit_log"), audits_before);
}

void test_transfer_api_success_and_expected_errors() {
    bms::test::SeededDatabase fixture;
    bms::Config config;
    config.host = "127.0.0.1";
    config.port = 0;
    bms::api::Server server(config);
    bms::transaction::register_routes(server.router(), fixture.database(), 2);
    const int port = server.bind();
    std::thread server_thread([&server] { server.run(); });
    server.wait_until_ready();
    httplib::Client client("127.0.0.1", port);

    const auto success = client.Post("/transfers",
        R"({"from_account_number":"ACC001","to_account_number":"ACC004","amount":"10.00"})",
        "application/json");
    CHECK(success);
    if (success) {
        const auto body = nlohmann::json::parse(success->body);
        CHECK_EQ(success->status, 201);
        CHECK_EQ(body["from_balance"].get<std::string>(), std::string("49990.00"));
        CHECK_EQ(body["to_balance"].get<std::string>(), std::string("100010.00"));
        CHECK(body["transfer_reference"].get<std::string>().find("TRF-") == 0);
        CHECK_EQ(body["transactions"]["debit"]["type"].get<std::string>(), std::string("transfer_debit"));
        CHECK_EQ(body["transactions"]["credit"]["type"].get<std::string>(), std::string("transfer_credit"));
        CHECK_EQ(body["transactions"]["debit"]["transfer_ref"], body["transactions"]["credit"]["transfer_ref"]);
    }

    const auto same = client.Post("/transfers",
        R"({"from_account_number":"ACC001","to_account_number":"ACC001","amount":"10.00"})",
        "application/json");
    CHECK(same);
    if (same) {
        const auto body = nlohmann::json::parse(same->body);
        CHECK_EQ(same->status, 400);
        CHECK_EQ(body["code"].get<std::string>(), std::string("VALIDATION_ERROR"));
        CHECK_EQ(body["field"].get<std::string>(), std::string("to_account_number"));
    }

    const auto frozen = client.Post("/transfers",
        R"({"from_account_number":"ACC001","to_account_number":"ACC002","amount":"10.00"})",
        "application/json");
    CHECK(frozen);
    if (frozen) {
        const auto body = nlohmann::json::parse(frozen->body);
        CHECK_EQ(frozen->status, 422);
        CHECK_EQ(body["code"].get<std::string>(), std::string("ACCOUNT_NOT_ACTIVE"));
        CHECK(body["message"].get<std::string>().find("ACC002 is frozen") != std::string::npos);
    }

    const auto unknown = client.Post("/transfers",
        R"({"from_account_number":"ACC001","to_account_number":"UNKNOWN","amount":"10.00"})",
        "application/json");
    CHECK(unknown);
    if (unknown) CHECK_EQ(unknown->status, 404);

    server.stop();
    server_thread.join();
}

}  // namespace

int main() {
    bms::test::run("transfer updates both accounts, linked transactions and audit", test_success_updates_both_balances_and_links_records_and_audits);
    bms::test::run("same-account transfers and invalid amounts are rejected", test_same_account_and_invalid_amounts_are_rejected);
    bms::test::run("account status and missing-account errors are handled", test_account_status_and_unknown_accounts_are_rejected_without_changes);
    bms::test::run("source minimum balance boundary is enforced", test_source_minimum_boundary_is_enforced);
    bms::test::run("transfer references are unique", test_generated_references_are_unique_across_transfers);
    bms::test::run("failure before credit rolls back transfer", test_failure_after_debit_before_credit_rolls_back_everything);
    bms::test::run("failure before audit rolls back transfer", test_failure_after_credit_before_audit_rolls_back_everything);
    bms::test::run("transfer API returns success and validation errors", test_transfer_api_success_and_expected_errors);
    return bms::test::finish();
}
