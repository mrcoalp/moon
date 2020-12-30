#include <map>

#include "scripting.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

int RunTests() {
    const int nrOfTests = (int)s_tests.size();
    int passed = 0;
    printf(ANSI_COLOR_CYAN "Executing %d tests..." ANSI_COLOR_RESET "\n", nrOfTests);
    for (const auto& test : s_tests) {
        printf(ANSI_COLOR_YELLOW "\n%s" ANSI_COLOR_RESET "\n", test.second.name.c_str());
        try {
            s_currentTest = test.second.name;
            test.second.check();
            for (const auto& success : test.second.successes) {
                printf(ANSI_COLOR_GREEN "\t'%s' succeeded" ANSI_COLOR_RESET "\n", success.c_str());
            }
            for (const auto& failure : test.second.failures) {
                printf(ANSI_COLOR_RED "\t'%s' failed" ANSI_COLOR_RESET "\n", failure.c_str());
            }
            if (test.second.failures.empty()) {
                ++passed;
                printf(ANSI_COLOR_GREEN "PASSED" ANSI_COLOR_RESET "\n");
            } else {
                printf(ANSI_COLOR_RED "FAILED" ANSI_COLOR_RESET "\n");
            }
        } catch (const std::exception& e) {
            printf(ANSI_COLOR_RED "'%s' raised an exception: %s" ANSI_COLOR_RESET "\n", test.second.name.c_str(), e.what());
        } catch (...) {
            printf(ANSI_COLOR_RED "'%s' raised an unknown exception" ANSI_COLOR_RESET "\n", test.second.name.c_str());
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

int main() {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", error.c_str()); });
    Moon::RegisterClass<Script>();
    auto failed = RunTests();
    Moon::CloseState();
    return failed;
}
