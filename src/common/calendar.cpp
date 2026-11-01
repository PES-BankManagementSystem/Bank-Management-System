#include "common/calendar.hpp"

namespace bms::calendar {

std::tm to_utc(std::time_t t) {
    std::tm result{};
#ifdef _WIN32
    gmtime_s(&result, &t);
#else
    gmtime_r(&t, &result);
#endif
    return result;
}

std::tm to_local(std::time_t t) {
    std::tm result{};
#ifdef _WIN32
    localtime_s(&result, &t);
#else
    localtime_r(&t, &result);
#endif
    return result;
}

}  // namespace bms::calendar
