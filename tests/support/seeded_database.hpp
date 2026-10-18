#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "common/db.hpp"

namespace bms::test {

class SeededDatabaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        db_path_ = std::filesystem::temp_directory_path() /
                   ("bms_test_" + std::to_string(stamp) + ".sqlite");
        db_ = std::make_unique<Database>(db_path_.string());
        db_->exec_script(read_file(std::filesystem::path(BMS_DB_DIR) / "schema.sql"));
        db_->exec_script(read_file(std::filesystem::path(BMS_DB_DIR) / "seed.sql"));
    }

    void TearDown() override {
        db_.reset();
        std::error_code error;
        std::filesystem::remove(db_path_, error);
        std::filesystem::remove(db_path_.string() + "-journal", error);
    }

    Database& db() { return *db_; }

private:
    static std::string read_file(const std::filesystem::path& path) {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Unable to read SQL file: " + path.string());
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

    std::filesystem::path db_path_;
    std::unique_ptr<Database> db_;
};

}  // namespace bms::test
