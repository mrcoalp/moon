#pragma once

#ifndef TESTS_H
#define TESTS_H

#include <functional>
#include <utility>
#include <vector>

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

#define TEST(_name)                                                                    \
    struct _name {                                                                     \
        _name() { Tests::TestCases.emplace_back(TestCase(#_name, &_name::TestBody)); } \
        static void TestBody();                                                        \
    };                                                                                 \
    _name test_##_name;                                                                \
    void _name::TestBody()

#define EXPECT(_condition)                                                        \
    if (!(_condition)) {                                                          \
        Tests::TestCases[Tests::CurrentTest].failures.emplace_back(#_condition);  \
    } else {                                                                      \
        Tests::TestCases[Tests::CurrentTest].successes.emplace_back(#_condition); \
    }

struct TestCase {
    std::vector<std::string> failures;
    std::vector<std::string> successes;
    std::string name;
    std::function<void()> check;
    TestCase(std::string name_, std::function<void()> check_) : name(std::move(name_)), check(std::move(check_)) {}
};

struct Tests {
    static std::vector<TestCase> TestCases;
    static size_t CurrentTest;
};

int RunTests();

#endif
