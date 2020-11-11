#pragma once

#include <map>
#include <stdexcept>
#include <vector>

#include "types.h"

namespace moon_helpers {
class Marshalling {
public:
    static inline int GetValue(moon_types::MarshallingType<int>, lua_State* L, int index) {
        ensure_type(lua_isinteger(L, index));
        return static_cast<int>(lua_tointeger(L, index));
    }

    static inline float GetValue(moon_types::MarshallingType<float>, lua_State* L, int index) {
        ensure_type(lua_isnumber(L, index));
        return static_cast<float>(lua_tonumber(L, index));
    }

    static inline double GetValue(moon_types::MarshallingType<double>, lua_State* L, int index) {
        ensure_type(lua_isnumber(L, index));
        return static_cast<double>(lua_tonumber(L, index));
    }

    static inline bool GetValue(moon_types::MarshallingType<bool>, lua_State* L, int index) {
        ensure_type(lua_isboolean(L, index));
        return lua_toboolean(L, index) != 0;
    }

    static inline unsigned GetValue(moon_types::MarshallingType<unsigned>, lua_State* L, int index) {
        ensure_type(lua_isinteger(L, index));
        return static_cast<unsigned>(lua_tointeger(L, index));
    }

    static inline std::string GetValue(moon_types::MarshallingType<std::string>, lua_State* L, int index) {
        ensure_type(lua_isstring(L, index));
        size_t size;
        const char* buffer = lua_tolstring(L, index, &size);
        return std::string{buffer, size};
    }

    static inline const char* GetValue(moon_types::MarshallingType<const char*>, lua_State* L, int index) {
        ensure_type(lua_isstring(L, index));
        return lua_tostring(L, index);
    }

    static inline void* GetValue(moon_types::MarshallingType<void*>, lua_State* L, int index) {
        ensure_type(lua_isuserdata(L, index));
        return lua_touserdata(L, index);
    }

    static inline moon_types::LuaFunction GetValue(moon_types::MarshallingType<moon_types::LuaFunction>, lua_State* L, int index) {
        ensure_type(lua_isfunction(L, index));
        return moon_types::LuaFunction(L, index);
    }

    static inline moon_types::LuaDynamicMap GetValue(moon_types::MarshallingType<moon_types::LuaDynamicMap>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        return moon_types::LuaDynamicMap(L, index);
    }

    template <typename R>
    static inline R GetValue(moon_types::MarshallingType<R>, lua_State* L, int index) {
        ensure_type(lua_isuserdata(L, index));
        return static_cast<R>(lua_touserdata(L, index));
    }

    template <typename T>
    static std::vector<T> GetValue(moon_types::MarshallingType<std::vector<T>>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        size_t size = lua_rawlen(L, -1);
        std::vector<T> vec;
        vec.reserve(size);
        for (size_t i = 1; i <= size; ++i) {
            lua_pushinteger(L, i);
            lua_gettable(L, -2);
            if (lua_type(L, -1) == LUA_TNIL) {
                break;
            }
            vec.emplace_back(GetValue(moon_types::MarshallingType<T>{}, L, -1));
            lua_pop(L, 1);
        }
        return vec;
    }

    template <typename T>
    [[nodiscard]] static moon_types::LuaMap<T> GetValue(moon_types::MarshallingType<moon_types::LuaMap<T>>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        lua_pushnil(L);
        moon_types::LuaMap<T> map;
        while (lua_next(L, -2) != 0) {
            map.emplace(GetValue(moon_types::MarshallingType<std::string>{}, L, -2), GetValue(moon_types::MarshallingType<T>{}, L, -1));
            lua_pop(L, 1);
        }
        return map;
    }

    static void PushValue(lua_State* L, int value) { lua_pushinteger(L, value); }

    static void PushValue(lua_State* L, float value) { lua_pushnumber(L, value); }

    static void PushValue(lua_State* L, double value) { lua_pushnumber(L, value); }

    static void PushValue(lua_State* L, bool value) { lua_pushboolean(L, (int)value); }

    static void PushValue(lua_State* L, unsigned value) { lua_pushinteger(L, value); }

    static void PushValue(lua_State* L, const std::string& value) { lua_pushstring(L, value.c_str()); }

    static void PushValue(lua_State* L, const char* value) { lua_pushstring(L, value); }

    static void PushValue(lua_State* L, void* value) { lua_pushlightuserdata(L, value); }

    static void PushValue(lua_State* L, const moon_types::LuaDynamicMap& value) { value.Push(); }

    template <typename T>
    static void PushValue(lua_State* L, const std::vector<T>& value) {
        lua_newtable(L);
        unsigned index = 1;
        for (const auto& element : value) {
            PushValue(L, index);
            PushValue(L, element);
            lua_settable(L, -3);
            ++index;
        }
    }

    template <typename T>
    static void PushValue(lua_State* L, const moon_types::LuaMap<T>& value) {
        lua_newtable(L);
        for (const auto& element : value) {
            PushValue(L, element.first);
            PushValue(L, element.second);
            lua_settable(L, -3);
        }
    }

    template <typename T>
    static void PushValue(lua_State* L, const std::map<std::string, T>& value) {
        lua_newtable(L);
        for (const auto& element : value) {
            PushValue(L, element.first);
            PushValue(L, element.second);
            lua_settable(L, -3);
        }
    }

    template <typename Class>
    static void PushValue(lua_State* L, Class* value) {
        auto** a = static_cast<Class**>(lua_newuserdata(L, sizeof(Class*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, Class::Binding.GetName());
        lua_setmetatable(L, -2);
    }

    static void PushNull(lua_State* L) { lua_pushnil(L); }

private:
    static void ensure_type(int check) {
        if (check != 1) {
            throw std::runtime_error("Lua type check failed!");
        }
    }
};
}  // namespace moon_helpers

using MS = moon_helpers::Marshalling;
