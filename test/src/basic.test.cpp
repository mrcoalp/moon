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

    SECTION("pop stack protection", "[basic]") {
        BEGIN_STACK_GUARD
        Moon::Pop();
        Moon::Pop(4);
        END_STACK_GUARD
    }

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
    std::string log;
    Moon::SetLogger([&log](const auto& msg) { log = msg; });

    SECTION("print a boolean") {
        Moon::Push(true);
        REQUIRE(Moon::StackElementToStringDump(-1) == "true");
        Moon::Pop();
    }

    SECTION("print a number") {
        double d = 2;
        Moon::Push(d);
        REQUIRE(Moon::StackElementToStringDump(-1) == std::to_string(d));
        Moon::Pop();
    }

    SECTION("print a string") {
        Moon::Push("passed");
        REQUIRE(Moon::StackElementToStringDump(-1) == R"("passed")");
        Moon::Pop();
    }

    SECTION("print a vector") {
        Moon::Push(std::vector<std::string>{"passed", "passed_again"});
        REQUIRE(Moon::StackElementToStringDump(-1) == R"(["passed", "passed_again"])");
        Moon::Pop();
    }

    SECTION("print a map") {
        REQUIRE(Moon::RunCode("return {x = 1, y = 2}"));
        REQUIRE_FALSE(Moon::StackElementToStringDump(-1).empty());
        Moon::Pop();
    }

    SECTION("print other values") {
        REQUIRE(Moon::RunCode("return function() assert(true) end"));
        REQUIRE_FALSE(Moon::StackElementToStringDump(-1).empty());
        Moon::Pop();
    }

    SECTION("handling invalid indexes") { REQUIRE(Moon::StackElementToStringDump(-1).empty()); }

    SECTION("printing whole stack") {
        Moon::PushValues(1, 2, 3, true, "string");
        Moon::LogStackDump();
        REQUIRE(log.size() > std::string("***** LUA STACK *****").size());
        Moon::Pop(5);
    }

    Moon::CloseState();
}

TEST_CASE("run code with and without errors", "[basic]") {
    Moon::Init();
    REQUIRE(Moon::RunCode("assert(true)"));
    REQUIRE_FALSE(Moon::HasError());
    REQUIRE_FALSE(Moon::RunCode("assert(false)"));
    REQUIRE(Moon::HasError());
    Moon::ClearError();
    REQUIRE_FALSE(Moon::RunCode("&"));
    REQUIRE(Moon::HasError());
    Moon::ClearError();
    Moon::CloseState();
}

TEST_CASE("loading files", "[basic]") {
    Moon::Init();
    std::string error;
    Moon::SetLogger([&error](const std::string& error_) { error = error_; });
    REQUIRE_FALSE(Moon::LoadFile("failed.lua"));
    REQUIRE_FALSE(Moon::LoadFile("scripts/failed.lua"));
    REQUIRE(!error.empty());
    error.clear();
    REQUIRE(Moon::LoadFile("scripts/passed.lua"));
    REQUIRE(error.empty());
    Moon::CloseState();
}

CATCH_REGISTER_ENUM(moon::LuaType, moon::LuaType::Null, moon::LuaType::Boolean, moon::LuaType::LightUserData, moon::LuaType::Number,
                    moon::LuaType::String, moon::LuaType::Table, moon::LuaType::Function, moon::LuaType::UserData, moon::LuaType::Thread);

TEST_CASE("check lua types", "[basic]") {
    Moon::Init();
    BEGIN_STACK_GUARD
    Moon::PushValues(2, true, "passed", std::vector<int>{1, 2, 3});
    Moon::PushNull();
    Moon::PushTable();
    REQUIRE(Moon::GetType(-3) == moon::LuaType::Table);
    REQUIRE(Moon::GetType(2) == moon::LuaType::Boolean);
    REQUIRE(Moon::GetType(-2) == moon::LuaType::Null);
    REQUIRE(Moon::GetType(-1) == moon::LuaType::Table);
    Moon::Pop(6);
    END_STACK_GUARD
    Moon::CloseState();
}

TEST_CASE("push and get values, non-user types", "[basic]") {
    Moon::Init();

    SECTION("push and get a bool") {
        BEGIN_STACK_GUARD
        Moon::Push(true);
        auto b = Moon::Get<bool>(-1);
        REQUIRE(b);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get an integer") {
        BEGIN_STACK_GUARD
        Moon::Push(3);
        auto i = Moon::Get<int>(-1);
        REQUIRE(i == 3);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get an unsigned integer") {
        BEGIN_STACK_GUARD
        Moon::Push(3);
        auto u = Moon::Get<uint32_t>(-1);
        REQUIRE(u == 3);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a float") {
        BEGIN_STACK_GUARD
        Moon::Push(3.14f);
        auto f = Moon::Get<float>(-1);
        REQUIRE(f == 3.14f);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a double") {
        BEGIN_STACK_GUARD
        Moon::Push(3.14);
        auto d = Moon::Get<double>(-1);
        REQUIRE(d == 3.14);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a string") {
        BEGIN_STACK_GUARD
        Moon::Push("passed");
        auto s = Moon::Get<std::string>(-1);
        REQUIRE(s == "passed");
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a C string") {
        BEGIN_STACK_GUARD
        Moon::Push("passed");
        const auto* cs = Moon::Get<const char*>(-1);
        REQUIRE(strcmp(cs, "passed") == 0);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get a char") {
        BEGIN_STACK_GUARD
        Moon::Push('m');
        auto c = Moon::Get<char>(-1);
        REQUIRE(c == 'm');
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push a string try to get a bool") {
        BEGIN_STACK_GUARD
        Moon::Push("not_passed");
        auto b = Moon::Get<bool>(-1);
        REQUIRE_FALSE(b);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push a string try to get a bool with throw") {
        BEGIN_STACK_GUARD
        Moon::Push("not_passed");
        REQUIRE_THROWS(moon::Marshalling::GetValue<bool>(Moon::GetState(), -1));
        Moon::Pop();
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("push and get values, data containers", "[basic]") {
    Moon::Init();

    SECTION("get a vector") {
        BEGIN_STACK_GUARD
        Moon::RunCode("vec = {1, 2, 3, nil, 5}");
        auto v = Moon::Get<std::vector<int>>("vec");
        REQUIRE(v.size() == 3);  // Stop when encountering null value
        REQUIRE(v[1] == 2);
        END_STACK_GUARD
    }

    SECTION("get a map") {
        BEGIN_STACK_GUARD
        Moon::RunCode("map = {x = {a = 1}, y = {b = 2}}");
        auto m = Moon::Get<moon::LuaMap<moon::LuaMap<int>>>("map");
        REQUIRE(m.at("y").at("b") == 2);
        END_STACK_GUARD
    }

    SECTION("push and get an int vector") {
        static auto randomNumber = []() -> int { return std::rand() % 100; };
        std::vector<int> vec(100);
        std::generate(vec.begin(), vec.end(), randomNumber);
        BEGIN_STACK_GUARD
        Moon::Push(vec);
        auto v = Moon::Get<std::vector<int>>(-1);
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
        Moon::Push(map);
        auto m = Moon::Get<std::map<std::string, int>>(-1);
        REQUIRE(map == m);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get an unordered map") {
        static auto randomNumber = []() -> int { return std::rand() % 100; };
        moon::LuaMap<int> map;
        std::vector<std::string> keys;
        for (int i = 0; i < 100; ++i) {
            map.emplace(std::to_string(i), randomNumber());
            keys.emplace_back(std::to_string(i));
        }
        BEGIN_STACK_GUARD
        Moon::Push(map);
        auto m = Moon::Get<moon::LuaMap<int>>(-1);
        REQUIRE(Moon::EnsureMapKeys(keys, m));
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
        Moon::Push(map);
        Moon::Push(vec);
        Moon::Push(other_vec);
        Moon::Push(other_map);
        auto a = Moon::Get<moon::LuaMap<moon::LuaMap<bool>>>(1);
        REQUIRE(a.at("first").at("first"));
        auto b = Moon::Get<std::vector<std::vector<std::vector<double>>>>(-3);
        REQUIRE(b[0][0][1] == 3.14);
        auto c = Moon::Get<std::vector<moon::LuaMap<std::vector<double>>>>(-2);
        REQUIRE(c[0].at("first")[2] == 4);
        auto d = Moon::Get<moon::LuaMap<std::vector<std::vector<std::vector<double>>>>>(Moon::GetTop());
        REQUIRE(d.at("second")[1][1][2] == 10.10);
        Moon::Pop(4);
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("set and get global variables", "[basic]") {
    Moon::Init();
    Moon::RunCode(R"(
string = "passed"
bool = true
int = -1
float = 12.6
double = 3.14
)");
    Moon::Push("constChar", "passes");
    BEGIN_STACK_GUARD
    REQUIRE(Moon::Get<std::string>("string") == "passed");
    REQUIRE(Moon::Get<bool>("bool"));
    REQUIRE(Moon::Get<int>("int") == -1);
    REQUIRE(Moon::Get<float>("float") == 12.6f);
    REQUIRE(Moon::Get<double>("double") == 3.14);
    REQUIRE(std::string(Moon::Get<const char*>("constChar")) == "passes");
    Moon::CleanGlobalVariable("double");
    REQUIRE_FALSE(Moon::Get<double>("double") == 3.14);
    END_STACK_GUARD
    Moon::CloseState();
}
