#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("moon::LuaRef", "[types][basic]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf("%s\n", error.c_str()); });

    SECTION("get from Lua stack") {
        BEGIN_STACK_GUARD

        Moon::RunCode("return {x = 2, y = 'passed', z = {g = true, w = {1, 2, 3}, r = 3.14}}");
        auto m = Moon::Get<moon::LuaRef>(-1);
        REQUIRE(m.IsLoaded());
        REQUIRE(m.GetType() == moon::LuaType::Table);
        m.Unload();
        REQUIRE_FALSE(m.IsLoaded());

        Moon::RunCode("return function() print('passed') end");
        auto f = Moon::Get<moon::LuaRef>(-1);
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());

        Moon::Pop(2);
        END_STACK_GUARD
    }

    SECTION("get and push from/to Lua stack") {
        BEGIN_STACK_GUARD

        Moon::RunCode("return function() print('passed') end");
        auto f = Moon::Get<moon::LuaRef>(-1);
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        Moon::Push(f);
        REQUIRE(Moon::GetType(-1) == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());

        Moon::Pop(2);
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("moon::LuaDynamicMap", "[types][functions][basic]") {
    Moon::Init();

    SECTION("get from Lua stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return {x = 2, y = 'passed', z = {g = true, w = {1, 2, 3}, r = 3.14}}");
        auto m = Moon::Get<moon::LuaDynamicMap>(-1);
        REQUIRE(m.IsLoaded());
        m.Unload();
        REQUIRE_FALSE(m.IsLoaded());
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("get and push from/to Lua stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return {x = 2, y = 'passed', z = {g = true, w = {1, 2, 3}, r = 3.14}}");
        auto m = Moon::Get<moon::LuaDynamicMap>(-1);
        REQUIRE(m.IsLoaded());
        Moon::RunCode("function TestMap(map) return map['z']['g'] end");
        bool b = false;
        REQUIRE(Moon::Call(b, "TestMap", m));
        m.Unload();
        REQUIRE_FALSE(m.IsLoaded());
        REQUIRE(b);
        Moon::Pop();
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("moon::LuaFunction", "[types][basic]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf("%s\n", error.c_str()); });

    SECTION("get from Lua stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return function() local a = 'dummy' end");
        auto f = Moon::Get<moon::LuaFunction>(-1);
        REQUIRE(f.IsLoaded());
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("get and push from/to Lua stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return function() local a = 'dummy' end");
        auto f = Moon::Get<moon::LuaFunction>(-1);
        REQUIRE(f.IsLoaded());
        Moon::Push(f);
        REQUIRE(Moon::GetType(-1) == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());
        Moon::Pop(2);
        END_STACK_GUARD
    }

    SECTION("call LuaFunction with Moon") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return function(a) assert(a == 'passed') end");
        auto f = Moon::Get<moon::LuaFunction>(-1);
        REQUIRE(f.IsLoaded());
        REQUIRE(Moon::LuaFunctionCall(f, "passed"));
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("call LuaFunction operator") {
        BEGIN_STACK_GUARD
        Moon::RunCode("test = false; return function() test = true end");
        auto f = Moon::Get<moon::LuaFunction>(-1);
        REQUIRE(f.IsLoaded());
        REQUIRE(f());
        REQUIRE(Moon::Get<bool>("test"));
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());
        Moon::Pop();
        END_STACK_GUARD
    }

    Moon::CloseState();
}
