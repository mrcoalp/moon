#pragma once

#ifndef USER_DEFINED_TYPE_H
#define USER_DEFINED_TYPE_H

#include "moon/moon.h"

class UserDefinedType {
public:
    UserDefinedType() = default;

    explicit UserDefinedType(int prop) : m_prop(prop) {}

    explicit UserDefinedType(lua_State*) : m_prop(Moon::Get<int>(1)) {}

    MOON_DECLARE_CLASS(UserDefinedType)

    MOON_PROPERTY(m_prop)

    MOON_METHOD(Getter) {
        Moon::Push(m_prop + Moon::Get<int>(1));
        return 1;
    }

    MOON_METHOD(Setter) {
        s_testClass = m_prop = Moon::Get<int>(1);
        return 0;
    }

    [[nodiscard]] inline int GetProp() const { return m_prop; }

    static inline int Test() { return s_testClass; }

private:
    int m_prop{0};
    static int s_testClass;
};

#endif
