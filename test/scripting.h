#pragma once

#ifndef SCRIPTING_H
#define SCRIPTING_H

#include "moon/moon.h"

#ifdef RegisterClass
#undef RegisterClass
#endif

class Scripting {
public:
    Scripting() = default;

    explicit Scripting(int prop) : m_prop(prop) {}

    explicit Scripting(lua_State*) : m_prop(Moon::GetValue<int>()) {}

    static std::string testCPPFunction;
    static int testClass;

    MOON_DECLARE_CLASS(Scripting)

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

    int m_prop;
};

MOON_METHOD(cppFunction);

#endif
