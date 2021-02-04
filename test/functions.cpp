#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("call functions", "[functions]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf("%s\n", error.c_str()); });

    SECTION("no arguments, no return") {
        Moon::RunCode("function Test() end");
        REQUIRE(Moon::Call("Test"));
    }

    SECTION("arguments, no return") {
        Moon::RunCode("function Test(a, b, c) assert(a + b == 3); assert(c == 'passed') end");
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call("Test", 2, 1, "passed"));
        END_STACK_GUARD
    }

    SECTION("no arguments, return") {
        Moon::RunCode("function Test() return 'passed' end");
        std::string s;
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call(s, "Test"));
        END_STACK_GUARD
        REQUIRE(s == "passed");
    }

    SECTION("arguments, return") {
        Moon::RunCode("function Test(a, b, c, d) assert(a + b == 3); assert(c); return d['first'] end");
        std::vector<std::string> vec;
        moon::LuaMap<std::vector<std::string>> map{std::make_pair("first", std::vector<std::string>{"passed"})};
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call(vec, "Test", 2, 1, true, map));
        END_STACK_GUARD
        REQUIRE(vec[0] == "passed");
    }

    SECTION("directly from stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return function(passed) assert(passed) end");
        REQUIRE(Moon::Call(true));
        Moon::RunCode("return function() assert(false) end");
        REQUIRE_FALSE(Moon::Call());
        END_STACK_GUARD
    }

    SECTION("call function errors") {
        BEGIN_STACK_GUARD
        REQUIRE_FALSE(Moon::Call());
        REQUIRE_FALSE(Moon::Call(false, 2));
        REQUIRE_FALSE(Moon::Call("failed"));
        REQUIRE_FALSE(Moon::Call("failed", 1, 2, 3));
        bool a = false;
        REQUIRE_FALSE(Moon::Call(a, "failed"));
        REQUIRE_FALSE(Moon::Call(a, "failed", 1, 2, 3));
        REQUIRE(Moon::RunCode("function failed() assert(false) end"));
        REQUIRE_FALSE(Moon::Call(a, "failed"));
        REQUIRE_FALSE(Moon::Call(a, "failed", 1, 2, 3));
        REQUIRE_FALSE(a);
        END_STACK_GUARD
    }

    Moon::CloseState();
}

std::string TestCPPStaticFunction(int a, int b) { return std::to_string(a + b); }

struct Test {
    static std::string TestCPPStaticFunction(bool passed = false) { return passed ? "passed" : "failed"; }
};

TEST_CASE("register C++ functions, lambdas", "[functions][basic]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf("%s\n", error.c_str()); });

    SECTION("register static functions") {
        BEGIN_STACK_GUARD
        Moon::RegisterFunction("TestCPPStaticFunction", TestCPPStaticFunction);
        REQUIRE(Moon::RunCode("return TestCPPStaticFunction(2, 3)"));
        auto s = Moon::Get<std::string>(-1);
        REQUIRE(s == "5");
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("register static member functions") {
        BEGIN_STACK_GUARD
        Moon::RegisterFunction("TestCPPStaticFunction", &Test::TestCPPStaticFunction);
        REQUIRE(Moon::RunCode("return TestCPPStaticFunction(true)"));
        auto s = Moon::Get<std::string>(-1);
        REQUIRE(s == "passed");
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("register lambda functions") {
        BEGIN_STACK_GUARD
        bool b = false;
        Moon::RegisterFunction("TestCPPStaticFunction", [&b]() { b = true; });
        REQUIRE(Moon::RunCode("TestCPPStaticFunction()"));
        REQUIRE(b);
        END_STACK_GUARD
    }

    SECTION("register lambda functions with args") {
        BEGIN_STACK_GUARD
        std::vector<int> vec;
        Moon::RegisterFunction("TestCPPStaticFunction", [&vec](std::vector<int> vec_) { vec = vec_; });
        REQUIRE(Moon::RunCode("TestCPPStaticFunction({1, 2, 3})"));
        REQUIRE(vec[1] == 2);
        END_STACK_GUARD
    }

    SECTION("register lambda functions with return") {
        BEGIN_STACK_GUARD
        Moon::RegisterFunction("TestBool", [](bool passed) { return passed; });
        REQUIRE(Moon::RunCode("local passed = TestBool(true); assert(passed)"));
        Moon::RegisterFunction("TestVec", [](std::vector<std::string> vec) { return vec; });
        REQUIRE(Moon::RunCode("local passed = TestVec({'passed', 'failed'})[1]; assert(passed == 'passed')"));
        Moon::RegisterFunction("TestMap", [](moon::LuaMap<std::string> map) { return map; });
        REQUIRE(Moon::RunCode("local passed = TestMap({first = 'passed'}); assert(passed['first'] == 'passed')"));
        END_STACK_GUARD
    }

    Moon::CloseState();
}
