#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "common/config.hpp"
#include "support/check.hpp"

namespace {

bms::EnvLookup fake_env(std::map<std::string, std::string> vars) {
    return [vars = std::move(vars)](const std::string& name) -> std::optional<std::string> {
        auto it = vars.find(name);
        if (it == vars.end()) return std::nullopt;
        return it->second;
    };
}

bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

int main() {
    using bms::load_config;

    bms::test::run("defaults listen on localhost:8080", [] {
        const bms::Config c = load_config({}, fake_env({}));
        CHECK_EQ(c.host, std::string("127.0.0.1"));
        CHECK_EQ(c.port, 8080);
        CHECK_EQ(c.db_path, std::string("bms.db"));
        CHECK(ends_with(c.frontend_dir, "frontend"));
    });

    bms::test::run("environment variables override defaults", [] {
        const bms::Config c = load_config({}, fake_env({{"BMS_HOST", "0.0.0.0"},
                                                        {"BMS_PORT", "9000"},
                                                        {"BMS_DB_PATH", "/tmp/x.db"},
                                                        {"BMS_FRONTEND_DIR", "web"}}));
        CHECK_EQ(c.host, std::string("0.0.0.0"));
        CHECK_EQ(c.port, 9000);
        CHECK_EQ(c.db_path, std::string("/tmp/x.db"));
        CHECK_EQ(c.frontend_dir, std::string("web"));
    });

    bms::test::run("command-line options override environment variables", [] {
        const bms::Config c =
            load_config({"--port", "9100", "--db", "cli.db", "--host", "::1", "--frontend", "ui"},
                        fake_env({{"BMS_PORT", "9000"}, {"BMS_DB_PATH", "env.db"}}));
        CHECK_EQ(c.port, 9100);
        CHECK_EQ(c.db_path, std::string("cli.db"));
        CHECK_EQ(c.host, std::string("::1"));
        CHECK_EQ(c.frontend_dir, std::string("ui"));
    });

    bms::test::run("port 0 is accepted (OS picks a free port)", [] {
        CHECK_EQ(load_config({"--port", "0"}, fake_env({})).port, 0);
    });

    bms::test::run("invalid ports are rejected", [] {
        for (const char* bad : {"abc", "80x", "-1", "65536", ""}) {
            CHECK_THROWS_AS(load_config({"--port", bad}, fake_env({})), std::invalid_argument);
            CHECK_THROWS_AS(load_config({}, fake_env({{"BMS_PORT", bad}})), std::invalid_argument);
        }
    });

    bms::test::run("missing option values and unknown options are rejected", [] {
        CHECK_THROWS_AS(load_config({"--port"}, fake_env({})), std::invalid_argument);
        CHECK_THROWS_AS(load_config({"--verbose"}, fake_env({})), std::invalid_argument);
    });

    return bms::test::finish();
}
