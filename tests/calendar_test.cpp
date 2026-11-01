// S-31 (NFR-9): the shared calendar helpers give the same answers on every
// platform, so business modules can use them instead of platform-specific calls.
#include <ctime>

#include <gtest/gtest.h>

#include "common/calendar.hpp"

TEST(Calendar, EpochIsFirstOfJanuary1970Utc) {
    const std::tm tm = bms::calendar::to_utc(0);
    EXPECT_EQ(tm.tm_year, 70);
    EXPECT_EQ(tm.tm_mon, 0);
    EXPECT_EQ(tm.tm_mday, 1);
    EXPECT_EQ(tm.tm_hour, 0);
    EXPECT_EQ(tm.tm_min, 0);
    EXPECT_EQ(tm.tm_sec, 0);
}

TEST(Calendar, LeapDayIsBrokenDownInUtc) {
    // 2024-02-29T12:34:56Z
    const std::tm tm = bms::calendar::to_utc(1709210096);
    EXPECT_EQ(tm.tm_year, 124);
    EXPECT_EQ(tm.tm_mon, 1);
    EXPECT_EQ(tm.tm_mday, 29);
    EXPECT_EQ(tm.tm_hour, 12);
    EXPECT_EQ(tm.tm_min, 34);
    EXPECT_EQ(tm.tm_sec, 56);
}

TEST(Calendar, LocalTimeRoundTripsThroughMktime) {
    const std::time_t now = std::time(nullptr);
    std::tm local = bms::calendar::to_local(now);
    EXPECT_EQ(std::mktime(&local), now);
}
