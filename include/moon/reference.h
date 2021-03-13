#pragma once

#include "types.h"

namespace moon {
/// Creates a reference of Lua object for specified index and stack in Lua registry.
class Reference {
public:
    Reference() = default;

    Reference(lua_State* L, int index) {
        lua_pushvalue(L, index);
        m_key = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    explicit Reference(lua_State* L) : Reference(L, -1) {}

    Reference(const Reference&) noexcept = delete;

    Reference(Reference&& other) noexcept : m_key(other.m_key) { other.m_key = LUA_NOREF; }

    Reference& operator=(const Reference&) noexcept = delete;

    Reference& operator=(Reference&& other) noexcept {
        m_key = other.m_key;
        other.m_key = LUA_NOREF;
        return *this;
    }

    /// Getter for the key identifier of the reference.
    /// \return Integer identifier.
    [[nodiscard]] inline int GetKey() const { return m_key; }

    /// Checks if key is set, and actions are allowed.
    /// \return Whether or not the reference is valid.
    [[nodiscard]] inline bool IsLoaded() const { return m_key != LUA_NOREF && m_key != LUA_REFNIL; }

    /// Unloads reference from Lua registry and resets key.
    void Unload(lua_State* L) {
        if (IsLoaded()) {
            luaL_unref(L, LUA_REGISTRYINDEX, m_key);
            m_key = LUA_NOREF;
        }
    }

    /// Pushed reference to specified stack.
    int Push(lua_State* L) const {
        if (!IsLoaded()) {
            if (L != nullptr) {
                lua_pushnil(L);
                return 1;
            }
            return 0;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_key);
        return 1;
    }

    /// Getter for the type of stored reference. Returns null if no reference was created.
    /// \param L Lua stack.
    /// \return Moon type.
    [[nodiscard]] LuaType GetType(lua_State* L) const {
        if (!IsLoaded()) {
            return LuaType::Null;
        }
        Push(L);
        auto type = static_cast<LuaType>(lua_type(L, -1));
        lua_pop(L, 1);
        return type;
    }

protected:
    explicit Reference(int key) : m_key(key) {}

    /// Reference identifier.
    int m_key{LUA_NOREF};
};

namespace meta {
template <typename T, typename Ret = T>
using is_moon_reference_t = std::enable_if_t<std::is_base_of_v<Reference, std::decay_t<T>>, Ret>;

template <typename T>
constexpr bool is_moon_reference_v = std::is_base_of_v<Reference, std::decay_t<T>>;
}  // namespace meta
}  // namespace moon
