#include <cstring>

#include "TestFramework.hpp"

int main(int argc, char** argv) {
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0;
    int failedCases = 0;
    for (const auto& test : crt::test::registry()) {
        if (filter && !std::strstr(test.name, filter)) continue;
        const int before = crt::test::failureCount();
        test.body();
        ++run;
        const bool ok = crt::test::failureCount() == before;
        if (!ok) ++failedCases;
        std::printf("[%s] %s\n", ok ? " ok " : "FAIL", test.name);
    }
    std::printf("\n%d test cases, %d failed, %d failed checks\n", run, failedCases, crt::test::failureCount());
    return failedCases == 0 && run > 0 ? 0 : 1;
}
