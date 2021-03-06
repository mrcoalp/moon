#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("call global lua functions", "[functions]") {
    Moon::Init();
    std::string log;
    std::string error;
    Moon::SetLogger([&log, &error](moon::Logger::Level level, const std::string& msg) {
        switch (level) {
            case moon::Logger::Level::Info:
            case moon::Logger::Level::Warning:
                log = msg;
                break;
            case moon::Logger::Level::Error:
                error = msg;
                break;
        }
    });

    SECTION("no arguments, no return") {
        REQUIRE(Moon::RunCode("function Test() assert(true) end"));
        Moon::Call<void>("Test");
    }

    SECTION("arguments, no return") {
        REQUIRE(Moon::RunCode("function Test(a, b, c) return a + b == 3 and c == 'passed' end"));
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call<bool>("Test", 2, 1, "passed"));
        END_STACK_GUARD
    }

    SECTION("no arguments, return") {
        Moon::RunCode("function Test() return 'passed' end");
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call<std::string>("Test") == "passed");
        END_STACK_GUARD
    }

    SECTION("arguments, return") {
        REQUIRE(Moon::RunCode("function Test(a, b, c, d) assert(a + b == 3); assert(c); return d['first'] end"));
        std::vector<std::string> vec;
        moon::LuaMap<std::vector<std::string>> map{std::make_pair("first", std::vector<std::string>{"passed"})};
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call<std::vector<std::string>>("Test", 2, 1, true, map)[0] == "passed");
        END_STACK_GUARD
    }

    SECTION("call function errors") {
        BEGIN_STACK_GUARD
        Moon::Call<void>("dummy");
        REQUIRE_FALSE(error.empty());
        error.clear();
        Moon::Call<int>("dummy");
        REQUIRE_FALSE(error.empty());
        error.clear();
        Moon::Call<std::string>("dummy");
        REQUIRE_FALSE(error.empty());
        error.clear();
        END_STACK_GUARD
    }

    REQUIRE(error.empty());
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
        bool b = false;
        Moon::RegisterFunction("TestTuple", [&b](std::tuple<int, bool, std::string> tup) { b = std::get<1>(tup); });
        REQUIRE(Moon::RunCode("TestTuple(1, true, 'passed')"));
        REQUIRE(b);
    }

    SECTION("register lambda functions with return") {
        Moon::RegisterFunction("TestBool", [](bool passed) { return passed; });
        REQUIRE(Moon::RunCode("local passed = TestBool(true); assert(passed)"));
        Moon::RegisterFunction("TestVec", [](std::vector<std::string> vec) { return vec; });
        REQUIRE(Moon::RunCode("local passed = TestVec({'passed', 'failed'})[1]; assert(passed == 'passed')"));
        Moon::RegisterFunction("TestMap", [](moon::LuaMap<std::string> map) { return map; });
        REQUIRE(Moon::RunCode("local passed = TestMap({first = 'passed'}); assert(passed['first'] == 'passed')"));
        Moon::RegisterFunction("TestTuple", []() { return std::make_tuple(1, true, "passed"); });
        REQUIRE(Moon::RunCode("local a, b, c = TestTuple(); assert(b)"));
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
    std::string log;
    std::string error;
    Moon::SetLogger([&log, &error](moon::Logger::Level level, const std::string& msg) {
        switch (level) {
            case moon::Logger::Level::Info:
            case moon::Logger::Level::Warning:
                log = msg;
                break;
            case moon::Logger::Level::Error:
                error = msg;
                break;
        }
    });
    BEGIN_STACK_GUARD

    GIVEN("a set of lua functions") {
        REQUIRE(Moon::RunCode("function OnUpdate(data, delta) data.passed = true; assert(data.passed); assert(delta == 2) end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a and b == 2 and c == 'passed' end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a, b, c end"));

        WHEN("we get them as stl functions") {
            auto onUpdate = Moon::Get<std::function<void(moon::LuaMap<moon::Object>, int)>>("OnUpdate");
            auto anonymous = Moon::Get<std::function<bool(bool, int, std::string)>>(-2);
            auto tupleReturn = Moon::Get<std::function<std::tuple<int, std::string, bool>(std::tuple<int, std::string, bool>)>>(-1);

            THEN("we should be able to call them") {
                onUpdate(moon::LuaMap<moon::Object>{}, 2);
                REQUIRE(error.empty());
                REQUIRE(anonymous(true, 2, "passed"));
                auto t = tupleReturn(std::make_tuple(1, "passed", true));
                REQUIRE(std::get<0>(t) == 1);
                REQUIRE(std::get<1>(t) == "passed");
                REQUIRE(std::get<2>(t));
            }
        }

        Moon::Pop(2);
    }

    REQUIRE(error.empty());
    END_STACK_GUARD
    Moon::CloseState();
}
