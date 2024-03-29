#include <catch2/catch.hpp>

#include "helpers.h"

SCENARIO("get global values from Lua stack", "[basic][global]") {
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
map2 = { x = { y = { z = { 1, 2 } } } }
function OnUpdate(bool)
    return bool
end
func_nested = { f = function(bool) return bool, 2 end, x = { y = { f = function(bool) assert(bool) end } } }
local a = 1
local b = 2
local c = 3
return a, b, c, function(a, b, c) return a, b, c end
)"));
        using traverse_map = std::map<std::string, std::map<std::string, std::map<std::string, int>>>;

        WHEN("single globals are fetched by name") {
            auto s = Moon::Get<std::string>("string");
            auto d = Moon::Get<double>("number");
            auto b = Moon::Get<bool>("boolean");
            std::string s2 = Moon::At("string");
            double d2 = Moon::At("number");
            bool b2 = Moon::At("boolean");

            THEN("values should match with expected") {
                REQUIRE(Moon::GetType("map", "x", "y", "z") == moon::LuaType::Number);
                REQUIRE(Moon::Check<int>("map", "x", "y", "z"));
                REQUIRE(s == "passed");
                REQUIRE(d == 3.14);
                REQUIRE(b);
                REQUIRE(s2 == "passed");
                REQUIRE(d2 == 3.14);
                REQUIRE(b2);
            }
        }

        AND_WHEN("single globals are fetched by index") {
            auto a = Moon::Get<int>(1);
            auto b = Moon::Get<int>(2);
            auto c = Moon::Get<int>(3);
            auto f = Moon::At(4).Get<std::function<std::tuple<int, int, int>(int, int, int)>>();

            THEN("values should match with expected") {
                REQUIRE(a == 1);
                REQUIRE(b == 2);
                REQUIRE(c == 3);
                auto tup = f(1, 2, 3);
                REQUIRE(std::get<0>(tup) == 1);
                REQUIRE(std::get<1>(tup) == 2);
                REQUIRE(std::get<2>(tup) == 3);
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

        AND_WHEN("multiple globals are fetched by index in stack") {
            auto tup = Moon::Get<int, int, int>(1, 2, 3);

            THEN("values should match with expected") {
                REQUIRE(std::get<0>(tup) == 1);
                REQUIRE(std::get<1>(tup) == 2);
                REQUIRE(std::get<2>(tup) == 3);
            }
        }

        AND_WHEN("multiple globals are fetched by name and index in stack") {
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

        AND_WHEN("nested getter and setter are used") {
            auto i = Moon::GetNested<int>("map2", "x", "y", "z", 2);
            auto b = Moon::GetNested<bool>("boolean");
            Moon::SetNested("map2", "x", "y", "z", 2, 1);
            auto i2 = Moon::GetNested<int>("map2", "x", "y", "z", 2);
            int i3 = Moon::GetNested<int>("array", 1);
            Moon::SetNested("array", 2, 6);
            int i4 = Moon::GetNested<int>("array", 2);
            Moon::SetNested("array2", 1, true);
            auto b2 = Moon::GetNested<bool>("array2", 1);
            // New keys
            Moon::SetNested("map2", "x", "y", "w", 1, 2);
            int i5 = Moon::GetNested<int>("map2", "x", "y", "w", 1);

            THEN("values should be valid and updated") {
                REQUIRE(i == 2);
                REQUIRE(i2 == 1);
                REQUIRE(i3 == 1);
                REQUIRE(i4 == 6);
                REQUIRE(i5 == 2);
                REQUIRE(b);
                REQUIRE(b2);
            }

            AND_THEN("errors should be caught but not to throw") {
                Moon::GetNested<int>("map2", "x");
                REQUIRE(logs.ErrorCheck());
            }
        }

        AND_WHEN("variables are cleared from stack") {
            Moon::At("string").Clean();
            Moon::At("number").Clean();
            Moon::At("boolean").Clean();
            REQUIRE(Moon::At("map")["x"]["y"].GetType() == moon::LuaType::Table);
            Moon::At("map")["x"]["y"].Clean();

            THEN("those values should be null") {
                REQUIRE(Moon::GetType("string") == moon::LuaType::Null);
                REQUIRE(Moon::At("number").GetType() == moon::LuaType::Null);
                REQUIRE(Moon::At("boolean").GetType() == moon::LuaType::Null);
                REQUIRE(Moon::At("map")["x"]["y"].GetType() == moon::LuaType::Null);
            }
        }

        AND_WHEN("nested get and set of functions is used") {
            auto f = Moon::At("func_nested")["f"].Get<std::function<bool(bool)>>();
            auto f2 = Moon::At("func_nested")["x"]["y"]["f"].Get<std::function<void(bool)>>();
            bool b = false;
            Moon::At("func_nested")["x"]["f"] = [&b](bool b_) { b = b_; };

            THEN("all functions should be valid") {
                REQUIRE(f);
                REQUIRE(f2);
                REQUIRE(f(true));
                f2(false);
                REQUIRE(logs.ErrorCheck());
                Moon::At("func_nested")["x"]["f"](true);
                REQUIRE(b);
            }
        }

        AND_WHEN("get or set is used in null values") {
            THEN("getters should log errors") {
                Moon::Get<int>("asd");
                REQUIRE(logs.ErrorCheck());
                Moon::Get<double>(56);
                REQUIRE(logs.ErrorCheck());
                Moon::GetNested<bool>(6, "c", 5, 8, "z");
                REQUIRE(logs.ErrorCheck());
                Moon::At(1)["a"].Get<int>();
                REQUIRE(logs.ErrorCheck());
                Moon::At(1)["a"][2]["z"](true);
                REQUIRE(logs.ErrorCheck());
            }
        }

        Moon::Pop(4);
    }

    END_STACK_GUARD
    INFO(logs.GetError())
    REQUIRE(logs.NoErrors());
    Moon::CloseState();
}

std::string Foo(int a, int b) { return std::to_string(a + b); }

struct Bar {
    static std::string Foo(bool passed = false) { return passed ? "passed" : "failed"; }
};

SCENARIO("push global values to Lua stack", "[basic][global]") {
    Moon::Init();
    std::string info, warning, error;
    LoggerSetter logs{info, warning, error};
    BEGIN_STACK_GUARD

    GIVEN("an empty lua stack") {
        REQUIRE(Moon::GetTop() == 0);

        WHEN("some globals are pushed") {
            Moon::At("int") = 2;
            Moon::At("string") = "passed";
            Moon::At("number") = 2.0;
            Moon::At("boolean") = true;
            Moon::At("array") = std::vector<int>{1, 2, 3};
            Moon::At("map") = std::map<std::string, int>{{"x", 1}, {"y", 2}};
            bool b = false;
            Moon::At("f") = [&b](bool b_) { b = b_; };
            Moon::At("Foo") = Foo;
            Moon::At("BarFoo") = Bar::Foo;
            Moon::At("nested_map")[1][5]["a"] = std::vector<int>{1, 2, 3};  // Must create complete path and needed tables

            THEN("globals should be available in Lua") {
                REQUIRE(Moon::RunCode("assert(int == 2)"));
                REQUIRE(Moon::RunCode("assert(string == 'passed')"));
                REQUIRE(Moon::RunCode("assert(number == 2.0)"));
                REQUIRE(Moon::RunCode("assert(boolean)"));
                REQUIRE(Moon::RunCode("f(true)"));
                REQUIRE(b);
                REQUIRE(Moon::RunCode("assert(Foo(1, 2) == '3')"));
                REQUIRE(Moon::RunCode("assert(BarFoo(true) == 'passed')"));
                REQUIRE(Moon::RunCode("assert(nested_map[1][5].a[1] == 1)"));
            }

            AND_THEN("we should be able to get globals") {
                int i = Moon::At("int");
                REQUIRE(i == 2);
                std::string s = Moon::At("string");
                REQUIRE(s == "passed");
                double d = Moon::At("number");
                REQUIRE(d == 2.0);
                bool b2 = Moon::At("boolean");
                REQUIRE(b2);
                std::vector<int> vec = Moon::At("array");
                REQUIRE(vec[1] == 2);
                std::map<std::string, int> map = Moon::At("map");
                REQUIRE(map.at("x") == 1);
                Moon::At("f")(true);
                REQUIRE(b);
                REQUIRE(Moon::At("Foo").Call<std::string>(1, 2) == "3");
                REQUIRE(Moon::At("BarFoo").Call<std::string>(true) == "passed");
            }

            AND_THEN("we can get trivial and container global values from keys") {
                REQUIRE(Moon::Get<int>("int") == 2);
                REQUIRE(Moon::Get<std::string>("string") == "passed");
                REQUIRE(Moon::Get<double>("number") == 2.0);
                REQUIRE(Moon::Get<bool>("boolean"));
                REQUIRE(Moon::Get<std::vector<int>>("array")[1] == 2);
                REQUIRE(Moon::Get<std::map<std::string, int>>("map").at("y") == 2);
            }

            AND_THEN("we should not be able to get Lua global registered C functions back as function, for memory safety reasons") {
                auto f = Moon::Get<std::function<void(bool)>>("f");
                REQUIRE(logs.ErrorCheck());
                auto f2 = Moon::Get<std::function<std::string(int, int)>>("Foo");
                REQUIRE(logs.ErrorCheck());
                auto f3 = Moon::Get<std::function<std::string(bool)>>("BarFoo");
                REQUIRE(logs.ErrorCheck());
                REQUIRE_FALSE(f);
                REQUIRE_FALSE(f2);
                REQUIRE_FALSE(f3);
            }
        }

        AND_WHEN("view is used to interact with global scope") {
            auto& view = Moon::View();
            view["int"] = 2;
            view["floating"] = 2.f;
            view["string"] = "passed";
            view["bool"] = true;
            view["f"] = [](const std::string& test) { return test; };
            view["map"]["x"][1] = 2;

            THEN("globals should be accessible") {
                REQUIRE(view["int"].Check<int>());
                REQUIRE(view["int"] == 2);
                REQUIRE((3 != view["int"]));
                REQUIRE(view["int"] != 2.0);  // Integrals and floating should be distinguishable
                REQUIRE(logs.ErrorCheck());
                REQUIRE(view["floating"] == 2.f);
                REQUIRE(("passed" == view["string"]));
                REQUIRE(view["bool"]);
                REQUIRE(view["f"].GetType() == moon::LuaType::UserData);
                REQUIRE(view["f"].Call<std::string>("passed") == "passed");
            }

            AND_THEN("nested fields should be accessible") {
                REQUIRE(view["map"]["x"][1].Check<int>());
                REQUIRE(view["map"]["x"][1].GetType() == moon::LuaType::Number);
                int i = view["map"]["x"][1];
                REQUIRE(i == 2);
            }
        }
    }

    END_STACK_GUARD
    INFO(logs.GetError())
    REQUIRE(logs.NoErrors());
    Moon::CloseState();
}
