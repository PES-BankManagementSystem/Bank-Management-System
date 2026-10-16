#pragma once

// Minimal assertions so tests can run under ctest before the test framework
// lands (S-06). Port tests that use this header to GoogleTest when it does.

#include <exception>
#include <functional>
#include <iostream>

namespace bms::test {

inline int& failure_count() {
    static int count = 0;
    return count;
}

inline void run(const char* name, const std::function<void()>& test) {
    const int before = failure_count();
    try {
        test();
    } catch (const std::exception& e) {
        ++failure_count();
        std::cerr << "  unexpected exception: " << e.what() << '\n';
    }
    std::cout << (failure_count() == before ? "[PASS] " : "[FAIL] ") << name << '\n';
}

inline int finish() {
    if (failure_count() > 0) std::cerr << failure_count() << " check(s) failed\n";
    return failure_count() == 0 ? 0 : 1;
}

}  // namespace bms::test

#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            ++::bms::test::failure_count();                                                 \
            std::cerr << "  " << __FILE__ << ':' << __LINE__ << ": CHECK(" #cond ") failed\n"; \
        }                                                                                   \
    } while (0)

#define CHECK_EQ(actual, expected)                                                       \
    do {                                                                                 \
        const auto actual_ = (actual);                                                   \
        const auto expected_ = (expected);                                               \
        if (!(actual_ == expected_)) {                                                   \
            ++::bms::test::failure_count();                                              \
            std::cerr << "  " << __FILE__ << ':' << __LINE__ << ": " #actual " == " #expected \
                      << " failed: got " << actual_ << ", expected " << expected_ << '\n'; \
        }                                                                                \
    } while (0)

#define CHECK_THROWS_AS(expr, ExceptionType)                                              \
    do {                                                                                  \
        bool thrown_ = false;                                                             \
        try {                                                                             \
            (void)(expr);                                                                 \
        } catch (const ExceptionType&) {                                                  \
            thrown_ = true;                                                               \
        }                                                                                 \
        if (!thrown_) {                                                                   \
            ++::bms::test::failure_count();                                               \
            std::cerr << "  " << __FILE__ << ':' << __LINE__ << ": " #expr " did not throw " \
                      << #ExceptionType << '\n';                                          \
        }                                                                                 \
    } while (0)
