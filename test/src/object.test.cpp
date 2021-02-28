#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

TEST_CASE("reference type", "[basic]") {
    Moon::Init();
    int topRefIndex = (int)lua_rawlen(Moon::GetState(), LUA_REGISTRYINDEX);

    moon::Reference ref;
    Moon::Push(20);
    moon::Reference ref2(Moon::GetState());
    ref = std::move(ref2);
    REQUIRE(ref.IsLoaded());
    REQUIRE(ref.GetKey() == topRefIndex + 1);

    Moon::CloseState();
}

TEST_CASE("dynamic object type", "[object][basic]") {
    Moon::Init();

    SECTION("create objects from Lua stack") {
        BEGIN_STACK_GUARD

        REQUIRE(Moon::RunCode("return {x = 2, y = 'passed', z = {g = true, w = {1, 2, 3}, r = 3.14}}"));
        auto m = Moon::MakeObjectFromIndex();
        REQUIRE(m.GetState() == Moon::GetState());
        REQUIRE(m.IsLoaded());
        REQUIRE(m.GetType() == moon::LuaType::Table);
        m.Unload();
        REQUIRE_FALSE(m.IsLoaded());

        REQUIRE(Moon::RunCode("return function() print('passed') end"));
        auto f = Moon::MakeObjectFromIndex();
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());

        Moon::Push(32);
        auto n = Moon::MakeObjectFromIndex();
        REQUIRE(n.IsLoaded());
        REQUIRE(n.GetType() == moon::LuaType::Number);
        n.Unload();
        REQUIRE_FALSE(n.IsLoaded());

        Moon::Pop(3);
        END_STACK_GUARD
    }

    SECTION("get and push from/to Lua stack") {
        BEGIN_STACK_GUARD

        moon::Object o(Moon::GetState());
        o.Push();
        REQUIRE(Moon::GetType(-1) == moon::LuaType::Null);
        REQUIRE(Moon::RunCode("return function() print('passed') end"));
        auto f = Moon::MakeObjectFromIndex();
        REQUIRE(f.IsLoaded());
        REQUIRE(f.GetType() == moon::LuaType::Function);
        Moon::Push(f);
        REQUIRE(Moon::GetType(-1) == moon::LuaType::Function);
        f.Unload();
        REQUIRE_FALSE(f.IsLoaded());

        auto o2 = Moon::MakeObject("passed");
        REQUIRE(o2.IsLoaded());
        REQUIRE(o2.Is<std::string>());
        REQUIRE(o2.As<std::string>() == "passed");

        Moon::Pop(3);
        END_STACK_GUARD
    }

    SECTION("get c++ object from moon object") {
        BEGIN_STACK_GUARD

        Moon::Push(20);
        Moon::Push(true);
        Moon::Push("passed");
        REQUIRE(Moon::RunCode("return {x = 1, y = 2, z = 3}"));
        REQUIRE(Moon::RunCode("return {1, 2, 3}"));

        auto i = Moon::MakeObjectFromIndex(1);
        REQUIRE(i.IsLoaded());
        REQUIRE(i.GetType() == moon::LuaType::Number);
        REQUIRE(i.As<int>() == 20);

        auto b = Moon::MakeObjectFromIndex(2);
        REQUIRE(b.IsLoaded());
        REQUIRE(b.GetType() == moon::LuaType::Boolean);
        REQUIRE(b.As<bool>());

        auto s = Moon::MakeObjectFromIndex(3);
        REQUIRE(s.IsLoaded());
        REQUIRE(s.GetType() == moon::LuaType::String);
        REQUIRE(s.As<std::string>() == "passed");
        // Chain with Object
        REQUIRE(s.As<moon::Object>().As<std::string>() == "passed");

        auto m = Moon::MakeObjectFromIndex(4);
        REQUIRE(m.IsLoaded());
        REQUIRE(m.GetType() == moon::LuaType::Table);
        auto map = m.As<std::unordered_map<std::string, int>>();
        REQUIRE(Moon::EnsureMapKeys({"x", "y", "z"}, map));
        REQUIRE(map.at("x") == 1);

        auto v = Moon::MakeObjectFromIndex(5);
        REQUIRE(v.IsLoaded());
        REQUIRE(v.GetType() == moon::LuaType::Table);
        REQUIRE(v.As<std::vector<int>>()[0] == 1);

        Moon::Pop(5);
        END_STACK_GUARD
    }

    SECTION("test c++ objects from moon objects") {
        {
            Moon::Push(20);
            auto o = Moon::MakeObjectFromIndex();
            REQUIRE(o.Is<int>());
            REQUIRE_FALSE(o.Is<double>());
            Moon::Pop();
        }
        {
            Moon::Push(20.0);
            auto o = Moon::MakeObjectFromIndex();
            REQUIRE(o.Is<double>());
            REQUIRE_FALSE(o.Is<int>());
            Moon::Pop();
        }
    }

    SECTION("object lifetime") {
        int topRefIndex = (int)lua_rawlen(Moon::GetState(), LUA_REGISTRYINDEX);
        BEGIN_STACK_GUARD

        {
            Moon::Push(20);
            auto o = Moon::MakeObjectFromIndex();
            Moon::Pop();
            REQUIRE(o.IsLoaded());
            REQUIRE(o.GetKey() == topRefIndex + 1);
        }

        {  // copy
            Moon::Push(20);
            auto o = Moon::MakeObjectFromIndex();
            Moon::Pop();
            auto o2 = o;
            moon::Object o3;
            o3 = o2;
            o3 = o3;
            REQUIRE(o.IsLoaded());
            REQUIRE(o2.IsLoaded());
            REQUIRE(o3.IsLoaded());
            REQUIRE(o != o2);
            REQUIRE(o2 != o3);
            REQUIRE(o3.GetKey() == topRefIndex + 3);
        }

        {  // move
            Moon::Push(20);
            auto o = Moon::MakeObjectFromIndex();
            Moon::Pop();
            REQUIRE(o.IsLoaded());
            int key = o.GetKey();
            auto o2 = std::move(o);
            REQUIRE(o.GetKey() == LUA_NOREF);
            REQUIRE(o2.IsLoaded());
            REQUIRE(key == o2.GetKey());
            REQUIRE(o2.GetKey() == topRefIndex + 1);
        }

        END_STACK_GUARD
    }

    SECTION("error handling") {
        BEGIN_STACK_GUARD

        moon::Object o{};
        Moon::Push(20);
        auto o2 = Moon::MakeObjectFromIndex();
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

SCENARIO("lua dynamic objects", "[object][basic]") {
    Moon::Init();
    BEGIN_STACK_GUARD

    GIVEN("an empty lua stack") {
        REQUIRE(Moon::GetTop() == 0);

        WHEN("integrals are created directly in lua stack") {
            int a = 1;
            unsigned b = 1;
            uint16_t c = 1;
            char d = 1;
            auto a_ = Moon::MakeObject(a);
            auto b_ = Moon::MakeObject(b);
            auto c_ = Moon::MakeObject(c);
            auto d_ = Moon::MakeObject(d);
            moon::Object objs[] = {a_, b_, c_, d_};

            THEN("all types must be valid and checked as any integral") {
                for (const auto& obj : objs) {
                    REQUIRE(obj.GetType() == moon::LuaType::Number);
                    REQUIRE(obj.Is<int>());
                    REQUIRE(obj.Is<unsigned>());
                    REQUIRE(obj.Is<uint16_t>());
                    REQUIRE(obj.Is<char>());
                }
            }

            AND_THEN("types should not be confused as floating point") {
                for (const auto& obj : objs) {
                    REQUIRE_FALSE(obj.Is<float>());
                    REQUIRE_FALSE(obj.Is<double>());
                }
            }

            AND_THEN("objects can be converted to integrals") {
                for (const auto& obj : objs) {
                    REQUIRE(obj.As<int>() == 1);
                    REQUIRE(obj.As<unsigned>() == 1);
                    REQUIRE(obj.As<uint16_t>() == 1);
                    REQUIRE(obj.As<char>() == 1);
                }
            }

            AND_THEN("objects can be converted to string, to use as index in table") {
                for (const auto& obj : objs) {
                    REQUIRE(obj.As<std::string>() == "1");
                    REQUIRE(strcmp(obj.As<const char*>(), "1") == 0);
                }
            }

            AND_THEN("trying to convert objects to wrong type raises an error") {
                for (const auto& obj : objs) {
                    obj.As<bool>();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    obj.As<std::vector<int>>();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    obj.As<float>();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    obj.As<double>();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                }
            }

            CHECK_STACK_GUARD
        }

        AND_WHEN("floating points are created in lua stack") {
            float a = 1.f;
            double b = 1.0;
            auto a_ = Moon::MakeObject(a);
            auto b_ = Moon::MakeObject(b);
            moon::Object objs[] = {a_, b_};

            THEN("all types must be valid") {
                for (const auto& obj : objs) {
                    REQUIRE(obj.GetType() == moon::LuaType::Number);
                    REQUIRE(obj.Is<float>());
                    REQUIRE(obj.Is<double>());
                }
            }

            AND_THEN("types should not be confused as integrals") {
                for (const auto& obj : objs) {
                    REQUIRE_FALSE(obj.Is<int>());
                    REQUIRE_FALSE(obj.Is<unsigned>());
                    REQUIRE_FALSE(obj.Is<uint16_t>());
                    REQUIRE_FALSE(obj.Is<char>());
                }
            }

            AND_THEN("objects can be converted to floating point") {
                REQUIRE(a_.As<float>() == 1.f);  // TODO(mpinto): WIll this fail in some systems?
                REQUIRE(b_.As<double>() == 1.0);
            }

            CHECK_STACK_GUARD
        }
    }

    // TODO: Add more tests with more types.

    END_STACK_GUARD
    REQUIRE_FALSE(Moon::HasError());
    Moon::CloseState();
}

SCENARIO("callable objects", "[object][functions][basic]") {
    Moon::Init();

    BEGIN_STACK_GUARD

    GIVEN("some valid functions in lua stack") {
        REQUIRE(Moon::RunCode("return function() return true end"));
        REQUIRE(Moon::RunCode("test = 'failed'; return function() test = 'passed' end"));
        REQUIRE(Moon::RunCode("test = 'failed'; return function(a) test = a end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a == 'passed' and b and c == 1 end"));

        WHEN("we try to create some callable objects") {
            std::vector<moon::Object> functions = {Moon::MakeObjectFromIndex(1), Moon::MakeObjectFromIndex(2), Moon::MakeObjectFromIndex(3),
                                                   Moon::MakeObjectFromIndex(4)};

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
                functions[1].Call<void>();
                REQUIRE(Moon::Get<std::string>("test") == "passed");
                functions[2].Call<void>("passed");
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
        CHECK_STACK_GUARD
    }

    AND_GIVEN("some faulty functions in lua stack") {
        REQUIRE(Moon::RunCode("test = false; return function() assert(test); return 'failed' end"));
        REQUIRE(Moon::RunCode("return function(a) assert(a); return 'failed'; end"));
        Moon::Push("string");

        WHEN("we try to create some callable objects") {
            std::vector<moon::Object> functions = {Moon::MakeObjectFromIndex(1), Moon::MakeObjectFromIndex(2), Moon::MakeObjectFromIndex(3), {}};

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
                    f.Call<void>();
                    REQUIRE(Moon::HasError());
                    Moon::ClearError();
                    f.Call<void>(false);
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
        CHECK_STACK_GUARD
    }

    END_STACK_GUARD
    REQUIRE_FALSE(Moon::HasError());

    Moon::CloseState();
}
