#pragma once

#ifndef MOON_SCRIPT_H
#define MOON_SCRIPT_H

#include "moon/lookupproxy.h"

namespace moon {
class Script {
public:
    static constexpr bool global = true;

    Script(lua_State* L, std::string&& code) : m_state(L) {
        m_top = lua_gettop(m_state);
        if (luaL_loadstring(m_state, &code[0]) != LUA_OK) {
            throw std::runtime_error("failed to load script");  // TODO: replace this. Wrap it
        }
        if (lua_pcall(m_state, 0, LUA_MULTRET, 0) != LUA_OK) {
            throw std::runtime_error("failed to run script");
        }
        m_pop = lua_gettop(m_state) - m_top;
    }

    Script(const Script&) = delete;

    Script(Script&&) = delete;

    [[nodiscard]] int Push() const { return m_pop; }

    /// Getter for Lua state.
    /// \return Lua state.
    [[nodiscard]] lua_State* GetState() const { return m_state; }

    template <typename T>
    operator T() const {
        return LookupProxy<Script, int>{this, std::forward<int>(m_top + meta::count_expected_v<T>)}.template Get<T>();
    }

private:
    lua_State* m_state{nullptr};
    int m_top{0};
    int m_pop{0};
};
}  // namespace moon

#endif
