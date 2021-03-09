#include <catch2/catch.hpp>

#include "helpers.h"

SCENARIO("moon object reference base class", "[basic][reference]") {
    Moon::Init();
    std::string error;
    Moon::SetLogger([&error](moon::Logger::Level level, const std::string& msg) {
        switch (level) {
            case moon::Logger::Level::Info:
            case moon::Logger::Level::Warning:
                break;
            case moon::Logger::Level::Error:
                error = msg;
                break;
        }
    });
    BEGIN_STACK_GUARD

    GIVEN("an empty Lua stack and an unloaded reference") {
        REQUIRE(Moon::GetTop() == 0);
        moon::Reference ref;
        REQUIRE_FALSE(ref.IsLoaded());

        WHEN("a value is pushed to Lua and a new reference is created from top index") {
            int topRefIndex = (int)lua_rawlen(Moon::GetState(), LUA_REGISTRYINDEX);
            Moon::Push(20);
            moon::Stack::PopGuard popGuard{Moon::GetState()};
            moon::Reference ref2(Moon::GetState());
            int key = ref2.GetKey();

            THEN("new reference must be loaded") { REQUIRE(ref2.IsLoaded()); }

            AND_THEN("new reference key should be one more than previous top ref index") { REQUIRE(key == topRefIndex + 1); }

            AND_THEN("we can move new reference to old one") {
                ref = std::move(ref2);
                REQUIRE(ref.IsLoaded());
                REQUIRE(ref.GetKey() == key);
            }
        }
    }

    END_STACK_GUARD
    REQUIRE(error.empty());
    Moon::CloseState();
}

TEST_CASE("dynamic object type", "[object][basic]") {
    Moon::Init();
    std::string error;
    Moon::SetLogger([&error](moon::Logger::Level level, const std::string& msg) {
        switch (level) {
            case moon::Logger::Level::Info:
            case moon::Logger::Level::Warning:
                break;
            case moon::Logger::Level::Error:
                error = msg;
                break;
        }
    });

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
        REQUIRE_FALSE(error.empty());
        error.clear();
        o2.As<bool>();
        REQUIRE_FALSE(error.empty());
        error.clear();

        END_STACK_GUARD
    }

    REQUIRE(error.empty());
    Moon::CloseState();
}

SCENARIO("lua dynamic objects", "[object][basic]") {
    Moon::Init();
    std::string info, warning, error;
    LoggerSetter logs{info, warning, error};
    BEGIN_STACK_GUARD

    GIVEN("an empty lua stack") {
        REQUIRE(Moon::GetTop() == 0);

        WHEN("an object is created without args") {
            moon::Object obj;

            THEN("object should not be loaded") {
                REQUIRE_FALSE(obj.IsLoaded());
                REQUIRE(obj.GetState() == nullptr);
            }

            AND_THEN("object type should be null") { REQUIRE(obj.GetType() == moon::LuaType::Null); }

            AND_THEN("object should not match any C type") {
                REQUIRE_FALSE(obj.Is<int>());
                REQUIRE_FALSE(obj.Is<float>());
                REQUIRE_FALSE(obj.Is<std::string>());
                REQUIRE_FALSE(obj.Is<void*>());
            }
        }

        AND_WHEN("integrals are created directly in lua stack") {
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

            AND_THEN("objects can be assigned to integrals") {
                for (const auto& obj : objs) {
                    int i = obj;
                    unsigned u = obj;
                    uint16_t u2 = obj;
                    char ch = obj;
                    REQUIRE(i == 1);
                    REQUIRE(u == 1);
                    REQUIRE(u2 == 1);
                    REQUIRE(ch == 1);
                }
            }

            AND_THEN("objects can be converted to string, to use as index in table") {
                for (const auto& obj : objs) {
                    REQUIRE(obj.As<std::string>() == "1");
                    REQUIRE(strcmp(obj.As<const char*>(), "1") == 0);
                    std::string s = obj;
                    REQUIRE(s == "1");
                }
            }

            AND_THEN("trying to convert objects to wrong type raises an error") {
                for (const auto& obj : objs) {
                    obj.As<bool>();
                    REQUIRE_FALSE(error.empty());
                    error.clear();
                    obj.As<std::vector<int>>();
                    REQUIRE_FALSE(error.empty());
                    error.clear();
                    obj.As<float>();
                    REQUIRE_FALSE(error.empty());
                    error.clear();
                    obj.As<double>();
                    REQUIRE_FALSE(error.empty());
                    error.clear();
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
                double dl = b_;
                REQUIRE(dl == 1.0);
            }

            CHECK_STACK_GUARD
        }

        AND_WHEN("booleans are created in lua stack") {
            bool a = true;
            auto a_ = Moon::MakeObject(a);

            THEN("type should be valid") { REQUIRE(a_.Is<bool>()); }

            AND_THEN("type should not be confused as integral") {
                REQUIRE_FALSE(a_.Is<int>());
                REQUIRE_FALSE(a_.Is<unsigned>());
                REQUIRE_FALSE(a_.Is<uint16_t>());
                REQUIRE_FALSE(a_.Is<char>());
            }

            AND_THEN("objects can be converted to booleans") {
                REQUIRE(a_.As<bool>());
                bool b = a_;
                REQUIRE(b);
            }

            CHECK_STACK_GUARD
        }

        AND_WHEN("strings are created in lua stack") {
            std::string a = "passed";
            const char* b = "passed";
            auto a_ = Moon::MakeObject(a);
            auto b_ = Moon::MakeObject(b);

            THEN("types should be valid") {
                REQUIRE(a_.Is<std::string>());
                REQUIRE(b_.Is<std::string>());
                REQUIRE(a_.Is<const char*>());
                REQUIRE(b_.Is<const char*>());
            }

            AND_THEN("type should not be confused as other pointers") {
                REQUIRE_FALSE(a_.Is<int*>());
                REQUIRE_FALSE(b_.Is<void*>());
            }

            AND_THEN("objects can be converted to strings and c strings") {
                REQUIRE(a_.As<std::string>() == "passed");
                REQUIRE(strcmp(a_.As<const char*>(), "passed") == 0);
            }

            CHECK_STACK_GUARD
        }

        AND_WHEN("pointers are created in lua stack") {
            auto* a = new int;
            auto* b = new float;
            auto a_ = Moon::MakeObject(a);
            auto b_ = Moon::MakeObject(b);

            THEN("types should be valid and simply considered userdata") {
                REQUIRE(a_.Is<int*>());
                REQUIRE(b_.Is<void*>());
                REQUIRE(a_.Is<char*>());
                REQUIRE(b_.Is<unsigned*>());
            }

            delete a;
            delete b;

            CHECK_STACK_GUARD
        }
    }

    END_STACK_GUARD
    REQUIRE(error.empty());
    Moon::CloseState();
}

SCENARIO("callable objects", "[object][functions][basic]") {
    Moon::Init();
    std::string info, warning, error;
    LoggerSetter logs{info, warning, error};
    BEGIN_STACK_GUARD

    GIVEN("some valid functions in lua stack") {
        REQUIRE(Moon::RunCode("return function() return true end"));
        REQUIRE(Moon::RunCode("test = 'failed'; return function() test = 'passed' end"));
        REQUIRE(Moon::RunCode("test = 'failed'; return function(a) test = a end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a == 'passed' and b and c == 1 end"));
        REQUIRE(Moon::RunCode("return function(a, b, c) return a, b, c end"));

        WHEN("we try to create some callable objects") {
            moon::Stack::PopGuard pop{Moon::GetState(), 5};

            std::vector<moon::Object> functions = {Moon::MakeObjectFromIndex(1), Moon::MakeObjectFromIndex(2), Moon::MakeObjectFromIndex(3),
                                                   Moon::MakeObjectFromIndex(4), Moon::MakeObjectFromIndex(5)};

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

            AND_THEN("multiple values can be returned in call") {
                REQUIRE(std::get<1>(functions[4].Call<std::tuple<std::string, bool, int>>("passed", true, 1)));
                REQUIRE(std::get<1>(functions[4].Call<std::string, bool, int>("passed", true, 1)));
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
                    REQUIRE(logs.ErrorCheck());
                    f.Call<void>(false);
                    REQUIRE(logs.ErrorCheck());
                    f.Call<int>();
                    REQUIRE(logs.ErrorCheck());
                    f.Call<int>(true);
                    REQUIRE(logs.ErrorCheck());
                }
                Moon::Set("test", true);
                functions[0].Call<int>();
                REQUIRE(logs.ErrorCheck());
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
    REQUIRE(logs.NoErrors());
    Moon::CloseState();
}
