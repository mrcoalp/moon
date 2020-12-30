#include "tests.h"

std::vector<TestCase> Tests::TestCases;
size_t Tests::CurrentTest = 0;

int RunTests() {
    const int nrOfTests = (int)Tests::TestCases.size();
    int passed = 0;
    printf(ANSI_COLOR_CYAN "Executing %d tests..." ANSI_COLOR_RESET "\n", nrOfTests);
    for (const auto& test : Tests::TestCases) {
        printf(ANSI_COLOR_YELLOW "\n%s" ANSI_COLOR_RESET "\n", test.name.c_str());
        try {
            test.check();
            ++Tests::CurrentTest;
            for (const auto& success : test.successes) {
                printf(ANSI_COLOR_GREEN "\t'%s' succeeded" ANSI_COLOR_RESET "\n", success.c_str());
            }
            for (const auto& failure : test.failures) {
                printf(ANSI_COLOR_RED "\t'%s' failed" ANSI_COLOR_RESET "\n", failure.c_str());
            }
            if (test.failures.empty()) {
                ++passed;
                printf(ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n");
            } else {
                printf(ANSI_COLOR_RED "FAILED" ANSI_COLOR_RESET "\n");
            }
        } catch (const std::exception& e) {
            printf(ANSI_COLOR_RED "'%s' raised an exception: %s" ANSI_COLOR_RESET "\n", test.name.c_str(), e.what());
        } catch (...) {
            printf(ANSI_COLOR_RED "'%s' raised an unknown exception" ANSI_COLOR_RESET "\n", test.name.c_str());
        }
    }
    const int result = nrOfTests - passed;
    if (result == 0) {
        printf("\n" ANSI_COLOR_GREEN "ALL TESTS PASSED!" ANSI_COLOR_RESET "\n");
    } else {
        printf("\n" ANSI_COLOR_RED "%d TESTS FAILED!" ANSI_COLOR_RESET "\n", result);
    }
    return result;
}
