#include "scripting.h"
#include "tests.h"

TEST(call_cpp_function_from_lua) {
    BEGIN_STACK_GUARD
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::RunCode(R"(cppFunction("passed"))");
    END_STACK_GUARD
    EXPECT(Scripting::testCPPFunction == "passed")
}

TEST(call_lua_function_pass_binding_object_modify_inside_lua) {
    Scripting o(20);
    BEGIN_STACK_GUARD
    Moon::LoadFile("scripts/luafunctions.lua");
    EXPECT(Moon::CallFunction("Object", &o))
    END_STACK_GUARD
    EXPECT(o.GetProp() == 1)
}

TEST(call_lua_function_pass_vector_get_string) {
    std::vector<std::string> vec;
    std::string s;
    for (size_t i = 0; i < 100; ++i) {
        vec.push_back(std::to_string(i));
    }
    BEGIN_STACK_GUARD
    Moon::LoadFile("scripts/luafunctions.lua");
    EXPECT(Moon::CallFunction(s, "OnUpdate", vec, 100))
    END_STACK_GUARD
    EXPECT(s == "99")
}

TEST(call_lua_function_pass_multiple_params_get_int) {
    int i;
    BEGIN_STACK_GUARD
    Moon::LoadFile("scripts/luafunctions.lua");
    EXPECT(Moon::CallFunction(i, "Maths", 2, 3, 4))
    END_STACK_GUARD
    EXPECT(i == 10)
}

TEST(call_lua_function_pass_multiple_params_get_vector) {
    std::vector<double> vecRet;
    BEGIN_STACK_GUARD
    Moon::LoadFile("scripts/luafunctions.lua");
    EXPECT(Moon::CallFunction(vecRet, "VecTest", 2.2, 3.14, 4.0))
    END_STACK_GUARD
    EXPECT(vecRet.size() == 3)
    EXPECT(vecRet[1] == 3.14)
}

TEST(call_lua_function_get_anonymous_function_and_call_it_no_args) {
    moon::LuaFunction fun;
    BEGIN_STACK_GUARD
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::LoadFile("scripts/luafunctions.lua");
    EXPECT(Moon::CallFunction(fun, "TestCallback"))
    EXPECT(fun())
    fun.Unload();
    END_STACK_GUARD
    EXPECT(Scripting::testCPPFunction == "passed_again")
}

TEST(call_lua_function_get_anonymous_function_and_call_it_with_args) {
    moon::LuaFunction fun;
    Scripting::testCPPFunction = "";
    BEGIN_STACK_GUARD
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::LoadFile("scripts/luafunctions.lua");
    EXPECT(Moon::CallFunction(fun, "TestCallbackArgs"))
    EXPECT(Moon::LuaFunctionCall(fun, "passed_again"))
    END_STACK_GUARD
    fun.Unload();
    EXPECT(Scripting::testCPPFunction == "passed_again")
}

TEST(cpp_class_bind_lua) {
    Moon::LoadFile("scripts/cppclass.lua");
    EXPECT(Scripting::testClass == 40)
}

TEST(lua_run_code) {
    const bool status = Moon::RunCode("a = 'passed'");
    EXPECT(status)
    BEGIN_STACK_GUARD
    const auto s = Moon::GetGlobalVariable<std::string>("a");
    EXPECT(s == "passed")
    END_STACK_GUARD
}

TEST(get_dynamic_map_from_lua) {
    Moon::LoadFile("scripts/luafunctions.lua");
    moon::LuaDynamicMap map;
    bool value = false;
    BEGIN_STACK_GUARD
    EXPECT(Moon::CallFunction(map, "GetMap"))
    EXPECT(Moon::CallFunction(value, "GetMapValue", map))
    map.Unload();
    END_STACK_GUARD
    EXPECT(value)
}

TEST(create_object_ref_and_use_it) {
    Moon::LoadFile("scripts/luafunctions.lua");
    Scripting object(0);
    BEGIN_STACK_GUARD
    Moon::PushValue(&object);
    auto ref = Moon::CreateRef();
    Moon::Pop();
    EXPECT(Moon::CallFunction("Object", ref))
    ref.Unload();
    END_STACK_GUARD
    EXPECT(Scripting::testClass == 1)
}

TEST(create_empty_dynamic_map_in_lua_with_expected_fail) {
    Moon::LoadFile("scripts/luafunctions.lua");
    moon::LuaDynamicMap map;
    bool value = false;
    BEGIN_STACK_GUARD
    EXPECT(!Moon::CallFunction("AddValueToMap", map))
    map = Moon::CreateDynamicMap();
    EXPECT(Moon::CallFunction("AddValueToMap", map))
    EXPECT(Moon::CallFunction(value, "GetMapValue", map))
    map.Unload();
    END_STACK_GUARD
    EXPECT(value)
}

TEST(get_global_vars_from_lua) {
    Moon::LoadFile("scripts/luavariables.lua");
    BEGIN_STACK_GUARD
    const auto s = Moon::GetGlobalVariable<std::string>("string");
    EXPECT(s == "passed")
    const bool b = Moon::GetGlobalVariable<bool>("bool");
    EXPECT(b)
    const int i = Moon::GetGlobalVariable<int>("int");
    EXPECT(i == -1)
    const auto f = Moon::GetGlobalVariable<float>("float");
    EXPECT(f == 12.6f)
    const auto d = Moon::GetGlobalVariable<double>("double");
    EXPECT(d == 3.14)
    END_STACK_GUARD
}

TEST(get_c_object_from_lua) {
    Scripting o(2);
    BEGIN_STACK_GUARD
    Moon::PushValue(&o);
    auto a = Moon::GetValue<Scripting>(-1);
    Moon::Pop();
    EXPECT(a.GetProp() == 2)
    END_STACK_GUARD
}

TEST(get_complex_data_containers_from_lua) {
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
    EXPECT(a.at("first").at("first"))
    auto b = Moon::GetValue<std::vector<std::vector<std::vector<double>>>>(-3);
    EXPECT(b[0][0][1] == 3.14)
    auto c = Moon::GetValue<std::vector<moon::LuaMap<std::vector<double>>>>(-2);
    EXPECT(c[0].at("first")[2] == 4)
    auto d = Moon::GetValue<moon::LuaMap<std::vector<std::vector<std::vector<double>>>>>(Moon::GetTop());
    EXPECT(d.at("second")[1][1][2] == 10.10)
    Moon::Pop(4);
    END_STACK_GUARD
}
