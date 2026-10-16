#include "common/config.hpp"

#include <cstdlib>
#include <stdexcept>

namespace bms {
namespace {

int parse_port(const std::string& text, const std::string& source) {
    std::size_t used = 0;
    int port = -1;
    try {
        port = std::stoi(text, &used);
    } catch (const std::exception&) {
        used = 0;
    }
    // Port 0 lets the OS pick a free port (used by the tests).
    if (used != text.size() || port < 0 || port > 65535) {
        throw std::invalid_argument(source + " must be a port number between 0 and 65535, got '" +
                                    text + "'");
    }
    return port;
}

}  // namespace

EnvLookup system_env() {
    return [](const std::string& name) -> std::optional<std::string> {
        const char* value = std::getenv(name.c_str());
        if (value == nullptr || *value == '\0') return std::nullopt;
        return std::string(value);
    };
}

Config load_config(const std::vector<std::string>& args, const EnvLookup& env) {
    Config config;
#ifdef BMS_DEFAULT_FRONTEND_DIR
    config.frontend_dir = BMS_DEFAULT_FRONTEND_DIR;
#endif

    if (auto v = env("BMS_HOST")) config.host = *v;
    if (auto v = env("BMS_PORT")) config.port = parse_port(*v, "BMS_PORT");
    if (auto v = env("BMS_DB_PATH")) config.db_path = *v;
    if (auto v = env("BMS_FRONTEND_DIR")) config.frontend_dir = *v;

    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& option = args[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) throw std::invalid_argument(option + " needs a value");
            return args[++i];
        };

        if (option == "--host") {
            config.host = value();
        } else if (option == "--port") {
            config.port = parse_port(value(), "--port");
        } else if (option == "--db") {
            config.db_path = value();
        } else if (option == "--frontend") {
            config.frontend_dir = value();
        } else {
            throw std::invalid_argument("Unknown option: " + option);
        }
    }
    return config;
}

std::string usage() {
    return "Usage: bms_server [options]\n"
           "\n"
           "Options (environment variable in brackets; options override it):\n"
           "  --host <address>   address to listen on   [BMS_HOST]  default 127.0.0.1\n"
           "  --port <number>    port to listen on      [BMS_PORT]  default 8080\n"
           "  --db <path>        SQLite database file   [BMS_DB_PATH]  default bms.db\n"
           "  --frontend <dir>   folder served at /     [BMS_FRONTEND_DIR]\n"
           "                     default: the frontend/ folder of the repository\n"
           "  --version          print the version and exit\n"
           "  --help             print this help and exit\n";
}

}  // namespace bms
