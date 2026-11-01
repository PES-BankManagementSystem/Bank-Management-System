#pragma once

#include <ctime>

// Thread-safe calendar conversions (NFR-9). The C library spells these
// differently on Windows and POSIX; this is the only place that knows, so
// business modules stay free of platform-specific code paths.
namespace bms::calendar {

// Breaks `t` down as UTC.
std::tm to_utc(std::time_t t);

// Breaks `t` down in the server's local time zone.
std::tm to_local(std::time_t t);

}  // namespace bms::calendar
