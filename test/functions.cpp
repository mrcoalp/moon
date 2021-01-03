#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("call functions", "[functions]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf("%s\n", error.c_str()); });

    SECTION("no arguments, no return") {
        Moon::RunCode("function Test() end");
        REQUIRE(Moon::CallFunction("Test"));
    }

    SECTION("arguments, no return") {
        Moon::RunCode("function Test(a, b, c) assert(a + b == 3); assert(c == 'passed') end");
        BEGIN_STACK_GUARD
        REQUIRE(Moon::CallFunction("Test", 2, 1, "passed"));
        END_STACK_GUARD
    }

    SECTION("no arguments, return") {
        Moon::RunCode("function Test() return 'passed' end");
        std::string s;
        BEGIN_STACK_GUARD
        REQUIRE(Moon::CallFunction(s, "Test"));
        END_STACK_GUARD
        REQUIRE(s == "passed");
    }

    SECTION("arguments, return") {
        Moon::RunCode("function Test(a, b, c, d) assert(a + b == 3); assert(c); return d['first'] end");
        std::vector<std::string> vec;
        moon::LuaMap<std::vector<std::string>> map{std::make_pair("first", std::vector<std::string>{"passed"})};
        BEGIN_STACK_GUARD
        REQUIRE(Moon::CallFunction(vec, "Test", 2, 1, true, map));
        END_STACK_GUARD
        REQUIRE(vec[0] == "passed");
    }

    Moon::CloseState();
}
