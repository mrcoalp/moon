#pragma once

#include "binding.h"
#include "moon.h"

int testClass = -1;
#ifdef RegisterClass
#undef RegisterClass
#endif

class Script {
public:
    explicit Script(int prop) : m_prop(prop) {}

    explicit Script(lua_State*) : m_prop(Moon::GetValue<int>()) {}

    LUA_DECLARE_CLASS(Script)

    LUA_PROPERTY(m_prop, int)

    LUA_METHOD(Getter) {
        Moon::PushValue(m_prop + Moon::GetValue<int>());
        return 1;
    }

    LUA_METHOD(Setter) {
        testClass = m_prop = Moon::GetValue<int>();
        return 0;
    }

    [[nodiscard]] inline int GetProp() const { return m_prop; }

private:
    int m_prop;
};

LUA_DEFINE_BINDING(Script, false)
LUA_ADD_PROPERTY(m_prop)
LUA_ADD_METHOD(Getter)
LUA_ADD_METHOD(Setter);

std::string testCPPFunction;
LUA_METHOD(cppFunction) {
    testCPPFunction = Moon::GetValue<std::string>();
    return 0;
}

bool test_call_cpp_function_from_lua() {
    Moon::Init();
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::LoadFile("assets/scripts/cppfunctions.lua");
    Moon::CloseState();
    return testCPPFunction == "passed";
}

bool test_call_lua_function_pass_binding_object_modify_inside_lua() {
    Script o(20);
    Moon::Init();
    Moon::RegisterClass<Script>();
    Moon::LoadFile("assets/scripts/luafunctions.lua");
    if (!Moon::CallFunction("Object", &o)) {
        return false;
    }
    Moon::CloseState();
    return o.GetProp() == 1;
}

bool test_call_lua_function_pass_vector_get_string() {
    std::vector<std::string> vec;
    std::string s;
    Moon::Init();
    Moon::LoadFile("assets/scripts/luafunctions.lua");
    for (size_t i = 0; i < 100; ++i) {
        vec.push_back(std::to_string(i));
    }
    if (!Moon::CallFunction(s, "OnUpdate", vec, 100)) {
        return false;
    }
    Moon::CloseState();
    return s == "99";
}

bool test_call_lua_function_pass_multiple_params_get_int() {
    int i;
    Moon::Init();
    Moon::LoadFile("assets/scripts/luafunctions.lua");
    if (!Moon::CallFunction(i, "Maths", 2, 3, 4)) {
        return false;
    }
    Moon::CloseState();
    return i == 10;
}

bool test_call_lua_function_pass_multiple_params_get_vector() {
    std::vector<double> vecRet;
    Moon::Init();
    Moon::LoadFile("assets/scripts/luafunctions.lua");
    if (!Moon::CallFunction(vecRet, "VecTest", 2.2, 3.14, 4.0)) {
        return false;
    }
    Moon::CloseState();
    return vecRet.size() == 3 && vecRet[1] == 3.14;
}

bool test_call_lua_function_get_anonymous_function_and_call_it() {
    moon_types::LuaFunction fun;
    Moon::Init();
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::LoadFile("assets/scripts/luafunctions.lua");
    if (!Moon::CallFunction(fun, "TestCallback")) {
        return false;
    }
    if (!fun()) {
        return false;
    }
    fun.Unload();
    Moon::CloseState();
    return testCPPFunction == "passed_again";
}

bool test_cpp_class_bind_lua() {
    Moon::Init();
    Moon::RegisterClass<Script>();
    Moon::LoadFile("assets/scripts/cppclass.lua");
    Moon::CloseState();
    return testClass == 40;
}

bool test_get_global_lua_var_from_cpp() {
    Moon::Init();
    Moon::LoadFile("assets/scripts/luavariables.lua");
    const std::string s = Moon::GetGlobalVariable<std::string>("string");
    const bool b = Moon::GetGlobalVariable<bool>("bool");
    const int i = Moon::GetGlobalVariable<int>("int");
    const float f = Moon::GetGlobalVariable<float>("float");
    const double d = Moon::GetGlobalVariable<double>("double");
    Moon::CloseState();
    return s == "passed" && b && i == -1 && f == 12.6f && d == 3.14;
}

bool test_lua_run_code() {
    Moon::Init();
    const bool status = Moon::RunCode("a = 'passed'");
    const std::string s = Moon::GetGlobalVariable<std::string>("a");
    Moon::CloseState();
    return status && s == "passed";
}

std::string dataStored;

bool test_get_dynamic_map_from_lua() {
    Moon::Init();
    Moon::LoadFile("assets/scripts/luafunctions.lua");
    moon_types::LuaDynamicMap map;
    if (!Moon::CallFunction(map, "GetMap")) {
        return false;
    }
    bool value = false;
    if (!Moon::CallFunction(value, "GetMapValue", map)) {
        return false;
    }
    map.Unload();
    Moon::CloseState();
    // Depth must be 2
    return value;
}
