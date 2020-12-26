#pragma once

#include "moon/moon.h"

int testClass = -1;
#ifdef RegisterClass
#undef RegisterClass
#endif

class Script {
public:
    explicit Script(int prop) : m_prop(prop) {}

    explicit Script(lua_State*) : m_prop(Moon::GetValue<int>()) {}

    MOON_DECLARE_CLASS(Script)

    MOON_PROPERTY(m_prop)

    MOON_METHOD(Getter) {
        Moon::PushValue(m_prop + Moon::GetValue<int>());
        return 1;
    }

    MOON_METHOD(Setter) {
        testClass = m_prop = Moon::GetValue<int>();
        return 0;
    }

    [[nodiscard]] inline int GetProp() const { return m_prop; }

private:
    int m_prop;
};

MOON_DEFINE_BINDING(Script, false)
MOON_ADD_PROPERTY(m_prop)
MOON_ADD_METHOD(Getter)
MOON_ADD_METHOD(Setter);

std::string testCPPFunction;
MOON_METHOD(cppFunction) {
    testCPPFunction = Moon::GetValue<std::string>();
    return 0;
}

bool test_call_cpp_function_from_lua() {
    int top = Moon::GetTop();
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::RunCode(R"(cppFunction("passed"))");
    return testCPPFunction == "passed" && top == Moon::GetTop();
}

bool test_call_lua_function_pass_binding_object_modify_inside_lua() {
    int top = Moon::GetTop();
    Script o(20);
    Moon::LoadFile("scripts/luafunctions.lua");
    if (!Moon::CallFunction("Object", &o)) {
        return false;
    }
    return o.GetProp() == 1 && top == Moon::GetTop();
}

bool test_call_lua_function_pass_vector_get_string() {
    int top = Moon::GetTop();
    std::vector<std::string> vec;
    std::string s;
    Moon::LoadFile("scripts/luafunctions.lua");
    for (size_t i = 0; i < 100; ++i) {
        vec.push_back(std::to_string(i));
    }
    if (!Moon::CallFunction(s, "OnUpdate", vec, 100)) {
        return false;
    }
    return s == "99" && top == Moon::GetTop();
}

bool test_call_lua_function_pass_multiple_params_get_int() {
    int top = Moon::GetTop();
    int i;
    Moon::LoadFile("scripts/luafunctions.lua");
    if (!Moon::CallFunction(i, "Maths", 2, 3, 4)) {
        return false;
    }
    return i == 10 && top == Moon::GetTop();
}

bool test_call_lua_function_pass_multiple_params_get_vector() {
    int top = Moon::GetTop();
    std::vector<double> vecRet;
    Moon::LoadFile("scripts/luafunctions.lua");
    if (!Moon::CallFunction(vecRet, "VecTest", 2.2, 3.14, 4.0)) {
        return false;
    }
    return vecRet.size() == 3 && vecRet[1] == 3.14 && top == Moon::GetTop();
}

bool test_call_lua_function_get_anonymous_function_and_call_it_no_args() {
    int top = Moon::GetTop();
    moon::LuaFunction fun;
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::LoadFile("scripts/luafunctions.lua");
    if (!Moon::CallFunction(fun, "TestCallback")) {
        return false;
    }
    if (!fun()) {
        return false;
    }
    fun.Unload();
    return testCPPFunction == "passed_again" && top == Moon::GetTop();
}

bool test_call_lua_function_get_anonymous_function_and_call_it_with_args() {
    int top = Moon::GetTop();
    moon::LuaFunction fun;
    Moon::RegisterFunction("cppFunction", cppFunction);
    Moon::LoadFile("scripts/luafunctions.lua");
    if (!Moon::CallFunction(fun, "TestCallbackArgs")) {
        return false;
    }
    if (!Moon::LuaFunctionCall(fun, "passed_again")) {
        return false;
    }
    fun.Unload();
    return testCPPFunction == "passed_again" && top == Moon::GetTop();
}

bool test_cpp_class_bind_lua() {
    int top = Moon::GetTop();
    Moon::LoadFile("scripts/cppclass.lua");
    return testClass == 40 && top == Moon::GetTop();
}

bool test_get_global_lua_var_from_cpp() {
    int top = Moon::GetTop();
    Moon::LoadFile("scripts/luavariables.lua");
    const auto s = Moon::GetGlobalVariable<std::string>("string");
    const bool b = Moon::GetGlobalVariable<bool>("bool");
    const int i = Moon::GetGlobalVariable<int>("int");
    const auto f = Moon::GetGlobalVariable<float>("float");
    const auto d = Moon::GetGlobalVariable<double>("double");
    return s == "passed" && b && i == -1 && f == 12.6f && d == 3.14 && top == Moon::GetTop();
}

bool test_lua_run_code() {
    int top = Moon::GetTop();
    const bool status = Moon::RunCode("a = 'passed'");
    const auto s = Moon::GetGlobalVariable<std::string>("a");
    return status && s == "passed" && top == Moon::GetTop();
}

bool test_get_dynamic_map_from_lua() {
    int top = Moon::GetTop();
    Moon::LoadFile("scripts/luafunctions.lua");
    moon::LuaDynamicMap map;
    if (!Moon::CallFunction(map, "GetMap")) {
        return false;
    }
    bool value = false;
    if (!Moon::CallFunction(value, "GetMapValue", map)) {
        map.Unload();
        return false;
    }
    map.Unload();
    return value && top == Moon::GetTop();
}
