#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace bms {

struct Config {
    std::string host = "127.0.0.1";  // localhost only unless asked otherwise
    int port = 8080;
    std::string db_path = "bms.db";
    std::string frontend_dir;  // defaults to the repository's frontend/ folder
};

// Returns the value of an environment variable, or nothing if it is unset.
using EnvLookup = std::function<std::optional<std::string>(const std::string& name)>;

EnvLookup system_env();

// Builds the configuration from defaults, then environment variables
// (BMS_HOST, BMS_PORT, BMS_DB_PATH, BMS_FRONTEND_DIR), then command-line options
// (--host, --port, --db, --frontend), later sources winning.
// Throws std::invalid_argument with a user-facing message on bad input.
Config load_config(const std::vector<std::string>& args, const EnvLookup& env);

std::string usage();

}  // namespace bms
