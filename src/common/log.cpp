#include "common/log.hpp"

#include <chrono>
#include <ctime>
#include <iostream>
#include <mutex>
#include <utility>

namespace bms::log {
namespace {

std::mutex& sink_mutex() {
    static std::mutex m;
    return m;
}

std::string utc_timestamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &now);
#else
    gmtime_r(&now, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", &tm);
    return buf;
}

void default_sink(Level level, std::string_view message) {
    static constexpr const char* kNames[] = {"INFO", "WARN", "ERROR"};
    std::ostream& out = level == Level::Info ? std::cout : std::cerr;
    out << utc_timestamp() << ' ' << kNames[static_cast<int>(level)] << ' ' << message
        << std::endl;
}

Sink& current_sink() {
    static Sink sink = default_sink;
    return sink;
}

void write(Level level, std::string_view message) {
    std::lock_guard<std::mutex> lock(sink_mutex());
    current_sink()(level, message);
}

}  // namespace

void info(std::string_view message) { write(Level::Info, message); }
void warning(std::string_view message) { write(Level::Warning, message); }
void error(std::string_view message) { write(Level::Error, message); }

Sink set_sink(Sink sink) {
    std::lock_guard<std::mutex> lock(sink_mutex());
    Sink previous = std::exchange(current_sink(), sink ? std::move(sink) : Sink(default_sink));
    return previous;
}

}  // namespace bms::log
