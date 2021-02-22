#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("dynamic object type", "[object][basic]") {
    Moon::Init();

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

        REQUIRE(Moon::RunCode("return function() print('passed') end"));
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
        // Chain with Object
        REQUIRE(s.As<moon::Object>().As<std::string>() == "passed");

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

        {  // copy
            Moon::Push(20);
            auto o = Moon::MakeObject();
            Moon::Pop();
            auto o2 = o;
            count = o2.GetKey();
            REQUIRE(o.IsLoaded());
            REQUIRE(o2.IsLoaded());
            REQUIRE(o != o2);
        }

        REQUIRE((count > LUA_NOREF && count <= 2));

        {  // move
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

        REQUIRE((count > LUA_NOREF && count <= 2));  // If indices are not unref this will fail, cause the reference count keeps incrementing
    }

    SECTION("error handling") {
        BEGIN_STACK_GUARD

        moon::Object o{};
        Moon::Push(20);
        auto o2 = Moon::MakeObject();
        Moon::Pop();

        o.As<int>();
        REQUIRE(Moon::HasError());
        Moon::ClearError();
        o2.As<bool>();
        REQUIRE(Moon::HasError());
        Moon::ClearError();

        END_STACK_GUARD
    }

    REQUIRE_FALSE(Moon::HasError());
    Moon::CloseState();
}

SCENARIO("callable objects", "[object][functions][basic]") {
    Moon::Init();

    GIVEN("some valid functions in lua stack") {
        BEGIN_STACK_GUARD

        REQUIRE(Moon::RunCode("return function() return true end"));
        REQUIRE(Moon::RunCode("test = 'failed'; return function() test = 'passed' end"));
        REQUIRE(Moon::RunCode("test = 'failed'; return function(a) test = a end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a == 'passed' and b and c == 1 end"));

        WHEN("we try to create some callable objects") {
            std::vector<moon::Object> functions = {Moon::MakeObject(1), Moon::MakeObject(2), Moon::MakeObject(3), Moon::MakeObject(4)};

            THEN("objects should be loaded") {
                for (const auto& f : functions) {
                    REQUIRE(f.IsLoaded());
                }
            }

            AND_THEN("objects can be pushed to lua stack") {
                for (const auto& f : functions) {
                    Moon::Push(f);
                    REQUIRE(Moon::GetType(-1) == moon::LuaType::Function);
                    Moon::Pop();
                }
            }

            AND_THEN("objects can be called with Call method") {
                REQUIRE(functions[0].Call<bool>());
                functions[1].Call();
                REQUIRE(Moon::Get<std::string>("test") == "passed");
                functions[2].Call("passed");
                REQUIRE(Moon::Get<std::string>("test") == "passed");
                REQUIRE(functions[3].Call<bool>("passed", true, 1));
            }

            AND_THEN("objects can be called with () operator") {
                functions[1]();
                REQUIRE(Moon::Get<std::string>("test") == "passed");
                functions[2]("passed");
                REQUIRE(Moon::Get<std::string>("test") == "passed");
            }

            AND_THEN("objects can be unloaded") {
                for (auto& f : functions) {
                    f.Unload();
                    REQUIRE_FALSE(f.IsLoaded());
                }
            }
        }

        Moon::Pop(4);
        END_STACK_GUARD
    }

    AND_GIVEN("some faulty functions in lua stack") {
        BEGIN_STACK_GUARD

        REQUIRE(Moon::RunCode("test = false; return function() assert(test); return 'failed' end"));
        REQUIRE(Moon::RunCode("return function(a) assert(a); return 'failed'; end"));
        Moon::Push("string");

        WHEN("we try to create some callable objects") {
            std::vector<moon::Object> functions = {Moon::MakeObject(1), Moon::MakeObject(2), Moon::MakeObject(3), {}};

            THEN("objects should be loaded except one") {
                for (size_t i = 0; i < 3; ++i) {
                    REQUIRE(functions[i].IsLoaded());
                }
                REQUIRE_FALSE(functions[3].IsLoaded());
            }

            AND_THEN("objects should be functions except two") {
                for (size_t i = 0; i < 2; ++i) {
                    REQUIRE(functions[i].GetType() == moon::LuaType::Function);
                }
                REQUIRE_FALSE(functions[2].GetType() == moon::LuaType::Function);
                REQUIRE_FALSE(functions[3].GetType() == moon::LuaType::Function);
            }

            AND_THEN("calling objects should generate errors") {
                for (const auto& f : functions) {
                    f.Call();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    f.Call(false);
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    f.Call<int>();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    f.Call<int>(true);
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                }
                Moon::Push("test", true);
                functions[0].Call<int>();
                REQUIRE(Moon::HasError());
                Moon::ClearError();
            }

            AND_THEN("objects can be unloaded") {
                for (auto& f : functions) {
                    f.Unload();
                    REQUIRE_FALSE(f.IsLoaded());
                }
            }
        }

        Moon::Pop(3);
        END_STACK_GUARD
    }

    REQUIRE_FALSE(Moon::HasError());
    Moon::CloseState();
}
