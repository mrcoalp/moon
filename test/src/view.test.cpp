#include <catch2/catch.hpp>

#include "helpers.h"

SCENARIO("use Moon view to interact with global scope", "[global][view][basic][functions]") {
    Moon::Init();
    std::string info, warning, error;
    LoggerSetter logs{info, warning, error};
    BEGIN_STACK_GUARD

    GIVEN("an empty Lua stack and Moon view") {
        REQUIRE(Moon::GetTop() == 0);
        auto& view = Moon::View();

        WHEN("some values are set through view") {
            view["int"] = 1;
            view["double"] = 1.0;
            view["bool"] = true;
            view["string"] = "passed";
            view["map"] = moon::LuaMap<int>{{"x", 1}, {"y", 2}, {"z", 3}};
            view["array"] = std::vector<int>{1, 2, 3};
            view["f"] = [](const std::string& test) { return test; };

            THEN("values should be available in Lua land") {
                REQUIRE(Moon::RunCode("assert(int == 1)"));
                REQUIRE(Moon::RunCode("assert(double == 1.0)"));
                REQUIRE(Moon::RunCode("assert(bool)"));
                REQUIRE(Moon::RunCode("assert(string == 'passed')"));
                REQUIRE(Moon::RunCode("assert(map.x == 1)"));
                REQUIRE(Moon::RunCode("assert(array[1] == 1)"));
                REQUIRE(Moon::RunCode("assert(f('passed') == 'passed')"));
            }

            view["int"] = 2;
            view["map"]["x"] = 2;
            view["map"]["w"]["f"] = [](bool test) { return test; };
            view["array"][1] = 2;
            view["array"][4]["f"] = [](int a, int b, int c) { return std::make_tuple(a, b, c); };

            AND_THEN("values can be updated, even nested tables") {
                REQUIRE(Moon::RunCode("assert(int == 2)"));
                REQUIRE(Moon::RunCode("assert(double == 1.0)"));
                REQUIRE(Moon::RunCode("assert(bool)"));
                REQUIRE(Moon::RunCode("assert(string == 'passed')"));
                REQUIRE(Moon::RunCode("assert(map.x == 2)"));
                REQUIRE(Moon::RunCode("assert(map.w.f(true))"));
                REQUIRE(Moon::RunCode("assert(array[1] == 2)"));
                REQUIRE(Moon::RunCode("local a, b, c = array[4]['f'](1, 2, 3); assert(b == 2)"));
                REQUIRE(Moon::RunCode("assert(f('passed') == 'passed')"));
            }

            AND_THEN("errors should be caught and stack preserved") {
                view["int"]["x"] = 2;  // not possible to set a field in a number number value
                REQUIRE(logs.ErrorCheck());
                view["double"][1]["w"][2][3]["z"] = 2;  // not possible to set a field in a number number value
                REQUIRE(logs.ErrorCheck());
                CHECK_STACK_GUARD
            }
        }
    }

    END_STACK_GUARD
    INFO(logs.GetError())
    REQUIRE(logs.NoErrors());
    Moon::CloseState();
}
