#pragma once

#include <lua.hpp>
#include <unordered_map>

namespace moon_types {
constexpr const char* LUA_REF_HOLDER_META_NAME{"LuaRefHolder"};

using LuaCFunction = int (*)(lua_State*);

template <typename T>
using LuaMap = std::unordered_map<std::string, T>;

template <typename T>
struct MarshallingType {};

enum class LuaType {
    Null = LUA_TNIL,
    Boolean = LUA_TBOOLEAN,
    LightUserData = LUA_TLIGHTUSERDATA,
    Number = LUA_TNUMBER,
    String = LUA_TSTRING,
    Table = LUA_TTABLE,
    Function = LUA_TFUNCTION,
    UserData = LUA_TUSERDATA,
    Thread = LUA_TTHREAD
};

/**
 * @brief Stores a Lua function in a table and saves its index. It can then be used to call that function.
 * When not needed, reference must be manually unloaded.
 */
class LuaFunction {
public:
    LuaFunction() = default;

    LuaFunction(lua_State* L, int index) : m_state(L) { Load(index); }

    /**
     * @brief Creates a new metatable that will hold all the references to lua functions.
     *
     * @param L Lua state to create metatable on.
     */
    static void Register(lua_State* L) { luaL_newmetatable(L, LUA_REF_HOLDER_META_NAME); }

    /**
     * @brief Checks if key is set, and actions are allowed.
     *
     * @return true Valid actions, key is set.
     * @return false Invalid actions, key is not set.
     */
    [[nodiscard]] inline bool IsLoaded() const { return m_key != -1; }

    /**
     * @brief Creates function reference in lua metatable and saves its key.
     *
     * @param index Index, in stack, of function to load.
     * @return true Function was successfully loaded.
     * @return false Function could not be loaded.
     */
    bool Load(int index) {
        lua_pushvalue(m_state, index);
        luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
        lua_pushvalue(m_state, -2);
        if (lua_isfunction(m_state, -1)) {
            m_key = luaL_ref(m_state, -2);
        }
        lua_pop(m_state, 2);
        return IsLoaded();
    }

    /**
     * @brief Unloads reference from lua metatable and resets key.
     */
    void Unload() {
        if (IsLoaded()) {
            luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
            luaL_unref(m_state, -1, m_key);
            lua_pop(m_state, 1);
            m_key = -1;
        }
    }

    /**
     * @brief Calls saved lua function.
     *
     * @return true Function called with success.
     * @return false Function call returned errors or action is not valid.
     */
    bool Call() {
        if (IsLoaded()) {
            luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
            lua_rawgeti(m_state, -1, m_key);
            int status = lua_pcall(m_state, 0, 0, 0);
            lua_pop(m_state, 2);
            return status == LUA_OK;
        }
        return false;
    }

    bool operator()() { return Call(); }

private:
    /**
     * @brief Key to later retrieve lua function.
     */
    int m_key{-1};

    /**
     * @brief Lua state.
     */
    lua_State* m_state{nullptr};
};

/**
 * @brief Stores a ref to a lua table.
 */
class LuaDynamicMap {
public:
    LuaDynamicMap() = default;

    explicit LuaDynamicMap(lua_State* L) : m_state(L) {}

    LuaDynamicMap(lua_State* L, int index) : m_state(L) {
        lua_pushvalue(m_state, index);
        luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
        lua_pushvalue(m_state, -2);
        if (lua_istable(m_state, -1)) {
            m_key = luaL_ref(m_state, -2);
        }
        lua_pop(m_state, 2);
    }

    /**
     * @brief Checks if key is set, and actions are allowed.
     *
     * @return true Valid actions, key is set.
     * @return false Invalid actions, key is not set.
     */
    [[nodiscard]] inline bool IsLoaded() const { return m_key != -1; }

    /**
     * @brief Unloads reference from lua metatable and resets key.
     */
    void Unload() {
        if (IsLoaded()) {
            luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
            luaL_unref(m_state, -1, m_key);
            lua_pop(m_state, 1);
            m_key = -1;
        }
    }

    /**
     * @brief Push value to stack.
     */
    void Push() const {
        if (!IsLoaded()) {
            if (m_state != nullptr) {
                lua_pushnil(m_state);
            }
            return;
        }
        luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
        lua_rawgeti(m_state, -1, m_key);
        lua_remove(m_state, -2);
    }

private:
    /**
     * @brief Key to later retrieve lua function.
     */
    int m_key{-1};

    /**
     * @brief Lua state.
     */
    lua_State* m_state{nullptr};
};
}  // namespace moon_types
