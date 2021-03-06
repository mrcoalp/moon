#include <catch2/catch.hpp>

#include "helpers.h"

SCENARIO("get global values from Lua stack") {
    Moon::Init();
    std::string info;
    std::string warning;
    std::string error;
    LoggerSetter logs{info, warning, error};
    BEGIN_STACK_GUARD

    GIVEN("some values in Lua stack") {
        REQUIRE(Moon::RunCode(R"(
string = 'passed'
number = 3.14
boolean = true
array = { 1, 2, 3, 4 }
map = { x = { y = { z = 2 } } }
function OnUpdate(bool)
    return bool
end
local a = 1
local b = 2
local c = 3
return a, b, c
)"));
        using traverse_map = std::map<std::string, std::map<std::string, std::map<std::string, int>>>;

        WHEN("single globals are fetched by name") {
            auto s = Moon::Get<std::string>("string");
            auto d = Moon::Get<double>("number");
            auto b = Moon::Get<bool>("boolean");

            THEN("values should match with expected") {
                REQUIRE(s == "passed");
                REQUIRE(d == 3.14);
                REQUIRE(b);
            }
        }

        AND_WHEN("single globals are fetched by index") {
            auto a = Moon::Get<int>(1);
            auto b = Moon::Get<int>(2);
            auto c = Moon::Get<int>(3);

            THEN("values should match with expected") {
                REQUIRE(a == 1);
                REQUIRE(b == 2);
                REQUIRE(c == 3);
            }
        }

        AND_WHEN("multiple globals are fetched by name") {
            auto tup = Moon::Get<std::string, double, bool>("string", "number", "boolean");

            THEN("values should match with expected") {
                REQUIRE(std::get<0>(tup) == "passed");
                REQUIRE(std::get<1>(tup) == 3.14);
                REQUIRE(std::get<2>(tup));
            }
        }

        AND_WHEN("multiple globals are fetched by index") {
            auto tup = Moon::Get<int, int, int>(1, 2, 3);

            THEN("values should match with expected") {
                REQUIRE(std::get<0>(tup) == 1);
                REQUIRE(std::get<1>(tup) == 2);
                REQUIRE(std::get<2>(tup) == 3);
            }
        }

        AND_WHEN("multiple globals are fetched by name and index") {
            auto tup = Moon::Get<std::string, int, double, int, bool, int>("string", 1, "number", 2, "boolean", 3);

            THEN("values should match with expected") {
                REQUIRE(std::get<0>(tup) == "passed");
                REQUIRE(std::get<2>(tup) == 3.14);
                REQUIRE(std::get<4>(tup));

                REQUIRE(std::get<1>(tup) == 1);
                REQUIRE(std::get<3>(tup) == 2);
                REQUIRE(std::get<5>(tup) == 3);
            }
        }

        AND_WHEN("multiple, more complex, globals are fetched by name") {
            auto tup = Moon::Get<std::vector<int>, traverse_map, std::function<bool(bool)>>("array", "map", "OnUpdate");

            THEN("values should match with expected") {
                REQUIRE(std::get<0>(tup)[1] == 2);
                REQUIRE(std::get<1>(tup).at("x").at("y").at("z") == 2);
                REQUIRE(std::get<2>(tup)(true));
            }
        }

        Moon::Pop(3);
    }

    END_STACK_GUARD
    REQUIRE(logs.NoErrors());
    Moon::CloseState();
}
