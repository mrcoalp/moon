#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("dynamic object type", "[object][basic]") {
    Moon::Init();
    std::string error;
    Moon::SetLogger([&error](const std::string& error_) { error = error_; });

    SECTION("create objects from Lua stack") {
        BEGIN_STACK_GUARD

        REQUIRE(Moon::RunCode("return {x = 2, y = 'passed', z = {g = true, w = {1, 2, 3}, r = 3.14}}"));
        auto m = Moon::MakeObject();
        REQUIRE(m.IsLoaded());
        REQUIRE(m.GetType() == moon::LuaType::Table);
        m.Unload();
        REQUIRE_FALSE(m.IsLoaded());

        REQUIRE(Moon::RunCode("return function() print('passed') end"));
        auto f = Moon::MakeObject();
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());

        Moon::Push(32);
        auto n = Moon::MakeObject();
        REQUIRE(n.IsLoaded());
        REQUIRE(n.GetType() == moon::LuaType::Number);
        n.Unload();
        REQUIRE_FALSE(n.IsLoaded());

        Moon::Pop(3);
        END_STACK_GUARD
    }

    SECTION("get and push from/to Lua stack") {
        BEGIN_STACK_GUARD

        Moon::RunCode("return function() print('passed') end");
        auto f = Moon::MakeObject();
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        Moon::Push(f);
        REQUIRE(Moon::GetType(-1) == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());

        Moon::Pop(2);
        END_STACK_GUARD
    }

    SECTION("get c++ object from moon object") {
        BEGIN_STACK_GUARD

        Moon::Push(20);
        Moon::Push(true);
        Moon::Push("passed");
        REQUIRE(Moon::RunCode("return {x = 1, y = 2, z = 3}"));
        REQUIRE(Moon::RunCode("return {1, 2, 3}"));

        auto i = Moon::MakeObject(1);
        REQUIRE(i.IsLoaded());
        REQUIRE(i.GetType() == moon::LuaType::Number);
        REQUIRE(i.As<int>() == 20);

        auto b = Moon::MakeObject(2);
        REQUIRE(b.IsLoaded());
        REQUIRE(b.GetType() == moon::LuaType::Boolean);
        REQUIRE(b.As<bool>());

        auto s = Moon::MakeObject(3);
        REQUIRE(s.IsLoaded());
        REQUIRE(s.GetType() == moon::LuaType::String);
        REQUIRE(s.As<std::string>() == "passed");

        auto m = Moon::MakeObject(4);
        REQUIRE(m.IsLoaded());
        REQUIRE(m.GetType() == moon::LuaType::Table);
        auto map = m.As<std::unordered_map<std::string, int>>();
        REQUIRE(Moon::EnsureMapKeys({"x", "y", "z"}, map));
        REQUIRE(map.at("x") == 1);

        auto v = Moon::MakeObject(5);
        REQUIRE(v.IsLoaded());
        REQUIRE(v.GetType() == moon::LuaType::Table);
        REQUIRE(v.As<std::vector<int>>()[0] == 1);

        Moon::Pop(5);
        END_STACK_GUARD
    }

    SECTION("object lifetime") {
        int count = LUA_NOREF;
        BEGIN_STACK_GUARD

        {
            Moon::Push(20);
            auto o = Moon::MakeObject();
            Moon::Pop();
            REQUIRE(o.IsLoaded());
        }

        {
            Moon::Push(20);
            auto o = Moon::MakeObject();
            Moon::Pop();
            auto o2 = o;
            count = o2.GetKey();
            REQUIRE(o.IsLoaded());
            REQUIRE(o2.IsLoaded());
            REQUIRE(o.GetKey() != o2.GetKey());
        }

        {
            Moon::Push(20);
            auto o = Moon::MakeObject();
            Moon::Pop();
            REQUIRE(o.IsLoaded());
            int key = o.GetKey();
            auto o2 = std::move(o);
            REQUIRE(o.GetKey() == LUA_NOREF);
            count = o2.GetKey();
            REQUIRE(o2.IsLoaded());
            REQUIRE(key == o2.GetKey());
        }

        END_STACK_GUARD

        REQUIRE(count <= 2);  // If indices are not unref this will fail, cause the reference count keeps incrementing
    }

    REQUIRE(error.empty());
    Moon::CloseState();
}

TEST_CASE("callable objects", "[object][functions][basic]") {
    Moon::Init();

    SECTION("get from Lua stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return function() local a = 'dummy' end");
        auto f = Moon::MakeObject();
        REQUIRE(f.IsLoaded());
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("get and push from/to Lua stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return function() local a = 'dummy' end");
        auto f = Moon::MakeObject();
        REQUIRE(f.IsLoaded());
        Moon::Push(f);
        REQUIRE(Moon::GetType(-1) == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());
        Moon::Pop(2);
        END_STACK_GUARD
    }

    SECTION("call object") {
        BEGIN_STACK_GUARD

        REQUIRE(Moon::RunCode("return function() return true end"));
        REQUIRE(Moon::RunCode("return function() assert(false) end"));
        REQUIRE(Moon::RunCode("return function(a) assert(a) end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a == 'passed' and b and c == 1 end"));
        Moon::Push("string");

        auto f = Moon::MakeObject(1);
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        REQUIRE(f.Call<bool>());

        auto f2 = Moon::MakeObject(2);
        REQUIRE(f2.IsLoaded());
        REQUIRE(f2.GetType() == moon::LuaType::Function);
        f2.Call();
        {
            REQUIRE(Moon::HasError());
            auto error = Moon::GetErrorMessage();
            REQUIRE(error.find("assertion failed") != error.size());
            Moon::ClearError();
        }

        auto f3 = Moon::MakeObject(3);
        REQUIRE(f3.IsLoaded());
        REQUIRE(f3.GetType() == moon::LuaType::Function);
        f3.Call(false);
        {
            REQUIRE(Moon::HasError());
            auto error = Moon::GetErrorMessage();
            REQUIRE(error.find("assertion failed") != error.size());
            Moon::ClearError();
        }

        auto f4 = Moon::MakeObject(4);
        REQUIRE(f4.IsLoaded());
        REQUIRE(f4.GetType() == moon::LuaType::Function);
        REQUIRE(f4.Call<bool>("passed", true, 1));

        auto f5 = Moon::MakeObject(5);
        REQUIRE(f5.IsLoaded());
        REQUIRE_FALSE(f5.GetType() == moon::LuaType::Function);
        f5.Call();
        REQUIRE(Moon::HasError());
        Moon::ClearError();

        Moon::Pop(5);
        END_STACK_GUARD
    }

    SECTION("call object operator") {
        BEGIN_STACK_GUARD

        REQUIRE(Moon::RunCode("test = false; return function() test = true end"));
        auto f = Moon::MakeObject();
        REQUIRE(f.IsLoaded());
        f();
        REQUIRE(Moon::Get<bool>("test"));

        REQUIRE(Moon::RunCode("test = false; string = 'failed'; return function(t, s) test = t; string = s; end"));
        auto f2 = Moon::MakeObject();
        REQUIRE(f2.IsLoaded());
        f2(true, "passed");
        REQUIRE(Moon::Get<bool>("test"));
        REQUIRE(Moon::Get<std::string>("string") == "passed");

        Moon::Pop(2);
        END_STACK_GUARD
    }

    REQUIRE_FALSE(Moon::HasError());
    Moon::CloseState();
}
