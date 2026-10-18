#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "common/db.hpp"

namespace bms::test {

class SeededDatabase {
public:
    SeededDatabase() {
        static std::atomic<unsigned long long> sequence{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
                ("bms_deposit_test_" + std::to_string(stamp) + "_" +
                 std::to_string(sequence.fetch_add(1)) + ".sqlite");
        database_ = std::make_unique<Database>(path_.string());
        database_->exec_script(read_file(std::filesystem::path(BMS_DB_DIR) / "schema.sql"));
        database_->exec_script(read_file(std::filesystem::path(BMS_DB_DIR) / "seed.sql"));
    }

    ~SeededDatabase() {
        database_.reset();
        std::error_code error;
        std::filesystem::remove(path_, error);
        std::filesystem::remove(path_.string() + "-journal", error);
    }

    SeededDatabase(const SeededDatabase&) = delete;
    SeededDatabase& operator=(const SeededDatabase&) = delete;

    Database& database() { return *database_; }

private:
    static std::string read_file(const std::filesystem::path& path) {
        std::ifstream input(path);
        if (!input) throw std::runtime_error("Unable to read SQL file: " + path.string());
        std::ostringstream contents;
        contents << input.rdbuf();
        return contents.str();
    }

    std::filesystem::path path_;
    std::unique_ptr<Database> database_;
};

}  // namespace bms::test
