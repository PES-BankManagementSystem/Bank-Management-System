#pragma once

#include <functional>
#include <string>
#include <string_view>

// Minimal thread-safe logging. Never pass passwords, PINs, session tokens or
// request bodies to these functions (NFR-4, SEC-2).
namespace bms::log {

enum class Level { Info, Warning, Error };

void info(std::string_view message);
void warning(std::string_view message);
void error(std::string_view message);

// Receives every log line. The default sink writes timestamped lines to
// stdout (info) and stderr (warnings and errors).
using Sink = std::function<void(Level, std::string_view)>;

// Replaces the sink and returns the previous one; tests use it to capture
// output. Passing an empty Sink restores the default.
Sink set_sink(Sink sink);

}  // namespace bms::log
