#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("call global lua functions", "[functions]") {
    Moon::Init();

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

TEST_CASE("register C++ functions, lambdas as global lua functions", "[functions][basic]") {
    Moon::Init();

    BEGIN_STACK_GUARD

    SECTION("register static functions") {
        Moon::RegisterFunction("TestCPPStaticFunction", TestCPPStaticFunction);
        REQUIRE(Moon::RunCode("return TestCPPStaticFunction(2, 3)"));
        auto s = Moon::Get<std::string>(-1);
        REQUIRE(s == "5");
        Moon::Pop();
    }

    SECTION("register static member functions") {
        Moon::RegisterFunction("TestCPPStaticFunction", &Test::TestCPPStaticFunction);
        REQUIRE(Moon::RunCode("return TestCPPStaticFunction(true)"));
        auto s = Moon::Get<std::string>(-1);
        REQUIRE(s == "passed");
        Moon::Pop();
    }

    SECTION("register lambda functions") {
        bool b = false;
        Moon::RegisterFunction("TestCPPStaticFunction", [&b]() { b = true; });
        REQUIRE(Moon::RunCode("TestCPPStaticFunction()"));
        REQUIRE(b);
    }

    SECTION("register lambda functions with args") {
        std::vector<int> vec;
        Moon::RegisterFunction("TestCPPStaticFunction", [&vec](std::vector<int> vec_) { vec = std::move(vec_); });
        REQUIRE(Moon::RunCode("TestCPPStaticFunction({1, 2, 3})"));
        REQUIRE(vec[1] == 2);
    }

    SECTION("register lambda functions with return") {
        Moon::RegisterFunction("TestBool", [](bool passed) { return passed; });
        REQUIRE(Moon::RunCode("local passed = TestBool(true); assert(passed)"));
        Moon::RegisterFunction("TestVec", [](std::vector<std::string> vec) { return vec; });
        REQUIRE(Moon::RunCode("local passed = TestVec({'passed', 'failed'})[1]; assert(passed == 'passed')"));
        Moon::RegisterFunction("TestMap", [](moon::LuaMap<std::string> map) { return map; });
        REQUIRE(Moon::RunCode("local passed = TestMap({first = 'passed'}); assert(passed['first'] == 'passed')"));
    }

    SECTION("register functions with optional args") {
        Moon::RegisterFunction("Optional", [](moon::Object optional) { return static_cast<bool>(optional); });
        REQUIRE(Moon::RunCode("assert(not Optional())"));
        REQUIRE(Moon::RunCode("assert(not Optional(nil))"));
        REQUIRE(Moon::RunCode("assert(Optional(1))"));
        REQUIRE(Moon::RunCode("assert(Optional('passed'))"));
    }

    END_STACK_GUARD

    Moon::CloseState();
}

SCENARIO("get lua function as C++ stl function", "[functions]") {
    Moon::Init();
    BEGIN_STACK_GUARD

    GIVEN("a set of lua functions") {
        REQUIRE(Moon::RunCode("function OnUpdate(data, delta) data.passed = true; assert(data.passed); assert(delta == 2) end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a and b == 2 and c == 'passed' end"));

        WHEN("we get them as stl functions") {
            auto onUpdate = Moon::Get<std::function<void(moon::LuaMap<moon::Object>, int)>>("OnUpdate");
            auto anonymous = Moon::Get<std::function<bool(bool, int, std::string)>>(-1);

            THEN("we should be able to call them") {
                onUpdate(moon::LuaMap<moon::Object>{}, 2);
                REQUIRE_FALSE(Moon::HasError());
                REQUIRE(anonymous(true, 2, "passed"));
            }
        }

        Moon::Pop();
    }

    REQUIRE_FALSE(Moon::HasError());
    END_STACK_GUARD
    Moon::CloseState();
}
