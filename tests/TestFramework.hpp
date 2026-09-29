#pragma once

// A deliberately tiny unit-test harness (no external dependencies, works with every compiler
// the project supports). Usage:
//
//   TEST_CASE("vectors add component-wise") { CHECK(Vec2{1, 2} + Vec2{3, 4} == Vec2{4, 6}); }
//
// The runner executes every registered case, reports failures with file:line and returns a
// non-zero exit code if anything failed. An optional command-line argument filters test names.

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace crt::test {

struct TestCase {
    const char* name;
    std::function<void()> body;
};

inline std::vector<TestCase>& registry() {
    static std::vector<TestCase> cases;
    return cases;
}

inline int& failureCount() {
    static int count = 0;
    return count;
}

struct Registrar {
    Registrar(const char* name, std::function<void()> body) { registry().push_back({name, std::move(body)}); }
};

inline void reportFailure(const char* file, int line, const std::string& message) {
    std::printf("    FAILED %s:%d: %s\n", file, line, message.c_str());
    ++failureCount();
}

template <class A, class B>
std::string describe(const A& a, const B& b) {
    std::ostringstream ss;
    ss << a << " vs " << b;
    return ss.str();
}

}  // namespace crt::test

#define CRT_CONCAT_INNER(a, b) a##b
#define CRT_CONCAT(a, b) CRT_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                              \
    static void CRT_CONCAT(crtTest_, __LINE__)();                                                    \
    static const ::crt::test::Registrar CRT_CONCAT(crtRegistrar_, __LINE__)(name, CRT_CONCAT(crtTest_, __LINE__)); \
    static void CRT_CONCAT(crtTest_, __LINE__)()

#define CHECK(condition)                                                                   \
    do {                                                                                   \
        if (!(condition)) ::crt::test::reportFailure(__FILE__, __LINE__, #condition);      \
    } while (false)

#define CHECK_NEAR(actual, expected, tolerance)                                                          \
    do {                                                                                                 \
        const double crtA = static_cast<double>(actual);                                                 \
        const double crtE = static_cast<double>(expected);                                               \
        if (!(std::abs(crtA - crtE) <= static_cast<double>(tolerance))) {                               \
            ::crt::test::reportFailure(__FILE__, __LINE__,                                               \
                                       std::string(#actual " ~ " #expected ": ") +                       \
                                           ::crt::test::describe(crtA, crtE));                           \
        }                                                                                                \
    } while (false)

#define REQUIRE(condition)                                                                 \
    do {                                                                                   \
        if (!(condition)) {                                                                \
            ::crt::test::reportFailure(__FILE__, __LINE__, #condition);                    \
            return;                                                                        \
        }                                                                                  \
    } while (false)
