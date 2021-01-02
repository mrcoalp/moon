#include <catch2/catch.hpp>

#include "stackguard.h"

TEST_CASE("initialize and close state", "[basic]") {
    Moon::Init();
    REQUIRE(Moon::GetState() != nullptr);
    Moon::CloseState();
    REQUIRE(Moon::GetState() == nullptr);
    Moon::SetState(luaL_newstate());
    REQUIRE(Moon::GetState() != nullptr);
    Moon::CloseState();
    REQUIRE(Moon::GetState() == nullptr);
}

TEST_CASE("index operations", "[basic]") {
    Moon::Init();

    SECTION("check for valid index") {
        BEGIN_STACK_GUARD
        Moon::PushValues(2, 3, 4, 5);
        REQUIRE(Moon::IsValidIndex(3));
        REQUIRE_FALSE(Moon::IsValidIndex(-5));
        REQUIRE(Moon::IsValidIndex(-3));
        REQUIRE_FALSE(Moon::IsValidIndex(0));
        REQUIRE_FALSE(Moon::IsValidIndex(5));
        Moon::Pop(4);
        END_STACK_GUARD
    }

    SECTION("convert negative index") {
        BEGIN_STACK_GUARD
        Moon::PushValues(2, 3, 4, 5);
        REQUIRE(Moon::ConvertNegativeIndex(-1) == 4);
        REQUIRE(Moon::ConvertNegativeIndex(-1) == Moon::GetTop());
        REQUIRE(Moon::ConvertNegativeIndex(-3) == 2);
        REQUIRE(Moon::ConvertNegativeIndex(3) == 3);
        Moon::Pop(4);
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("print stack elements", "[basic]") {
    Moon::Init();

    SECTION("print a boolean") {
        Moon::PushValue(true);
        REQUIRE(Moon::StackElementToStringDump(-1) == "true");
        Moon::Pop();
    }

    SECTION("print a number") {
        double d = 2;
        Moon::PushValue(d);
        REQUIRE(Moon::StackElementToStringDump(-1) == std::to_string(d));
        Moon::Pop();
    }

    SECTION("print a string") {
        Moon::PushValue("passed");
        REQUIRE(Moon::StackElementToStringDump(-1) == R"("passed")");
        Moon::Pop();
    }

    SECTION("print a vector") {
        Moon::PushValue(std::vector<std::string>{"passed", "passed_again"});
        REQUIRE(Moon::StackElementToStringDump(-1) == R"(["passed", "passed_again"])");
        Moon::Pop();
    }

    Moon::CloseState();
}

TEST_CASE("loading files", "[basic]") {
    Moon::Init();
    std::string error;
    Moon::SetLogger([&error](const std::string& error_) { error = error_; });
    REQUIRE_FALSE(Moon::LoadFile("failed.lua"));
    REQUIRE_FALSE(Moon::LoadFile("scripts/failed.lua"));
    REQUIRE(!error.empty());
    REQUIRE(Moon::LoadFile("scripts/luavariables.lua"));
    REQUIRE(Moon::GetGlobalVariable<bool>("bool"));
    Moon::CloseState();
}

CATCH_REGISTER_ENUM(moon::LuaType, moon::LuaType::Null, moon::LuaType::Boolean, moon::LuaType::LightUserData, moon::LuaType::Number,
                    moon::LuaType::String, moon::LuaType::Table, moon::LuaType::Function, moon::LuaType::UserData, moon::LuaType::Thread);

TEST_CASE("check lua types", "[basic]") {
    Moon::Init();
    BEGIN_STACK_GUARD
    Moon::PushValues(2, true, "passed", std::vector<int>{1, 2, 3});
    Moon::PushNull();
    REQUIRE(Moon::GetValueType(-2) == moon::LuaType::Table);
    REQUIRE(Moon::GetValueType(2) == moon::LuaType::Boolean);
    REQUIRE(Moon::GetValueType(-1) == moon::LuaType::Null);
    Moon::Pop(5);
    END_STACK_GUARD
    Moon::CloseState();
}

TEST_CASE("push and get values, non-user types", "[basic]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { INFO(error); });

    SECTION("push and get a bool") {
        BEGIN_STACK_GUARD
        Moon::PushValue(true);
        auto b = Moon::GetValue<bool>(-1);
        REQUIRE(b);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get an integer") {
        BEGIN_STACK_GUARD
        Moon::PushValue(3);
        auto i = Moon::GetValue<int>(-1);
        REQUIRE(i == 3);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get an unsigned integer") {
        BEGIN_STACK_GUARD
        Moon::PushValue(3);
        auto u = Moon::GetValue<uint32_t>(-1);
        REQUIRE(u == 3);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a float") {
        BEGIN_STACK_GUARD
        Moon::PushValue(3.14f);
        auto f = Moon::GetValue<float>(-1);
        REQUIRE(f == 3.14f);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a double") {
        BEGIN_STACK_GUARD
        Moon::PushValue(3.14);
        auto d = Moon::GetValue<double>(-1);
        REQUIRE(d == 3.14);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a string") {
        BEGIN_STACK_GUARD
        Moon::PushValue("passed");
        auto s = Moon::GetValue<std::string>(-1);
        REQUIRE(s == "passed");
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a C string") {
        BEGIN_STACK_GUARD
        Moon::PushValue("passed");
        const auto* cs = Moon::GetValue<const char*>(-1);
        REQUIRE(strcmp(cs, "passed") == 0);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a char") {
        BEGIN_STACK_GUARD
        Moon::PushValue('m');
        auto c = Moon::GetValue<char>(-1);
        REQUIRE(c == 'm');
        Moon::Pop();
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("push and get values, data containers", "[basic]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { INFO(error); });

    SECTION("get a vector") {
        BEGIN_STACK_GUARD
        Moon::RunCode("vec = {1, 2, 3}");
        auto v = Moon::GetGlobalVariable<std::vector<int>>("vec");
        REQUIRE(v[1] == 2);
        END_STACK_GUARD
    }

    SECTION("get a map") {
        BEGIN_STACK_GUARD
        Moon::RunCode("map = {x = {a = 1}, y = {b = 2}}");
        auto m = Moon::GetGlobalVariable<moon::LuaMap<moon::LuaMap<int>>>("map");
        REQUIRE(m.at("y").at("b") == 2);
        END_STACK_GUARD
    }

    SECTION("push and get an int vector") {
        static auto randomNumber = []() -> int { return std::rand() % 100; };
        std::vector<int> vec(100);
        std::generate(vec.begin(), vec.end(), randomNumber);
        BEGIN_STACK_GUARD
        Moon::PushValue(vec);
        auto v = Moon::GetValue<std::vector<int>>(-1);
        REQUIRE(vec == v);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a map") {
        static auto randomNumber = []() -> int { return std::rand() % 100; };
        std::map<std::string, int> map;
        for (int i = 0; i < 100; ++i) {
            map.emplace(std::to_string(i), randomNumber());
        }
        BEGIN_STACK_GUARD
        Moon::PushValue(map);
        auto m = Moon::GetValue<std::map<std::string, int>>(-1);
        REQUIRE(map == m);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get an unordered map") {
        static auto randomNumber = []() -> int { return std::rand() % 100; };
        moon::LuaMap<int> map;
        for (int i = 0; i < 100; ++i) {
            map.emplace(std::to_string(i), randomNumber());
        }
        BEGIN_STACK_GUARD
        Moon::PushValue(map);
        auto m = Moon::GetValue<moon::LuaMap<int>>(-1);
        REQUIRE(map.at("1") == m.at("1"));
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get complex data containers (nested vectors and maps)") {
        moon::LuaMap<moon::LuaMap<bool>> map;
        moon::LuaMap<bool> nested_map;
        nested_map.emplace("first", true);
        map.emplace("first", nested_map);
        std::vector<std::vector<std::vector<double>>> vec;
        vec.push_back({{2, 3.14, 4}, {4.6, 5, 6}});
        vec.push_back({{6, 7, 8}, {8.1, 9, 10.10}});
        std::vector<moon::LuaMap<std::vector<double>>> other_vec;
        other_vec.push_back({{"first", {2, 3.14, 4}}, {"second", {4.6, 5, 6}}});
        moon::LuaMap<std::vector<std::vector<std::vector<double>>>> other_map;
        other_map.emplace("first", vec);
        other_map.emplace("second", vec);
        BEGIN_STACK_GUARD
        Moon::PushValue(map);
        Moon::PushValue(vec);
        Moon::PushValue(other_vec);
        Moon::PushValue(other_map);
        auto a = Moon::GetValue<moon::LuaMap<moon::LuaMap<bool>>>(1);
        REQUIRE(a.at("first").at("first"));
        auto b = Moon::GetValue<std::vector<std::vector<std::vector<double>>>>(-3);
        REQUIRE(b[0][0][1] == 3.14);
        auto c = Moon::GetValue<std::vector<moon::LuaMap<std::vector<double>>>>(-2);
        REQUIRE(c[0].at("first")[2] == 4);
        auto d = Moon::GetValue<moon::LuaMap<std::vector<std::vector<std::vector<double>>>>>(Moon::GetTop());
        REQUIRE(d.at("second")[1][1][2] == 10.10);
        Moon::Pop(4);
        END_STACK_GUARD
    }

    Moon::CloseState();
}
