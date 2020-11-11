#pragma once

#include <algorithm>

#include "luaclass.h"
#include "marshalling.h"

/**
 * @brief Handles all the logic related to "communication" between C++ and Lua, initializing it.
 * Acts as an engine to allow to call C++ functions from Lua and vice-versa, registers
 * and exposes C++ classes to Lua, pops and pushes values to Lua stack, runs file scripts and
 * string scripts.
 */
class Moon {
public:
    /**
     * @brief Initializes Lua state and opens libs.
     */
    static void Init();

    /**
     * @brief Closes Lua state main thread.
     */
    static void CloseState();

    static inline void SetState(lua_State* state) { s_state = state; }

    static inline void SetLogger(const std::function<void(const std::string&)>& logger) { s_logger = logger; }

    /**
     * @brief Get the Lua state object.
     *
     * @return const lua_State*
     */
    [[nodiscard]] static inline lua_State* GetState() { return s_state; }

    /**
     * @brief Getter for the current top index.
     *
     * @return Current lua stack top index.
     */
    [[nodiscard]] static inline int GetTop() { return lua_gettop(s_state); }

    /**
     * @brief Returns a string containing all values current in the stack. Useful for debug purposes.
     */
    static std::string GetStackDump();

    /**
     * @brief Calls logger with all values current in the stack. Useful for debug purposes.
     */
    static inline void LogStackDump() { s_logger(GetStackDump()); }

    /**
     * @brief Loads Lua file script to stack.
     *
     * @param filePath Path to file to be loaded.
     * @return true File loaded successfully.
     * @return false Failed to load file.
     */
    static bool LoadFile(const char* filePath);

    /**
     * @brief Runs Lua script snippet.
     *
     * @param code String to run.
     * @return true Success running script.
     * @return false Failed to run script.
     */
    static bool RunCode(const char* code);

    /**
     * @brief Get the type of value in stack, at position index.
     *
     * @param index Position of stack to check.
     * @return LuaType
     */
    static moon_types::LuaType GetValueType(int index = 1) { return static_cast<moon_types::LuaType>(lua_type(s_state, index)); }

    /**
     * @brief Get value from Lua stack.
     *
     * @tparam R Value type.
     * @param index Stack index.
     * @return R
     */
    template <typename R>
    [[nodiscard]] static inline R GetValue(const int index = 1) {
        return MS::GetValue(moon_types::MarshallingType<R>{}, s_state, index);
    }

    /**
     * @brief Get the global variable from Lua.
     *
     * @tparam R Type of returned variable.
     * @param name Name of the variable.
     * @return R Lua global variable.
     */
    template <typename R>
    [[nodiscard]] static inline R GetGlobalVariable(const char* name) {
        lua_getglobal(s_state, name);
        const R r = GetValue<R>(-1);
        lua_pop(s_state, 1);
        return r;
    }

    /**
     * @brief Ensures that a given LuaMap contains all desired keys. Useful, since maps obtained from lua are dynamic.
     *
     * @tparam T LuaMap type.
     * @param keys Keys to check in map.
     * @param map Map to check.
     * @return True if map contains all keys, false otherwise.
     */
    template <typename T>
    static bool EnsureMapKeys(const std::vector<std::string>& keys, const moon_types::LuaMap<T>& map) {
        return std::all_of(keys.cbegin(), keys.cend(), [&map](const std::string& key) { return map.find(key) != map.cend(); });
    }

    /**
     * @brief Push value to Lua stack.
     *
     * @tparam T Value type.
     * @param value Value pushed to Lua stack.
     */
    template <typename T>
    static void PushValue(T value) {
        MS::PushValue(s_state, value);
    }

    /**
     * @brief Push global variable to Lua stack.
     *
     * @tparam T Value type.
     * @param name Variable name.
     * @param value Value pushed to Lua stack.
     */
    template <typename T>
    static void PushGlobalVariable(const char* name, T value) {
        PushValue(value);
        lua_setglobal(s_state, name);
    }

    /**
     * @brief Recursive helper method to push multiple values to Lua stack.
     *
     * @tparam T Value type.
     * @tparam Args Rest of the values types.
     * @param first Value pushed to Lua stack.
     * @param args Rest of the value to push to Lua stack.
     */
    template <typename T, typename... Args>
    static void PushValues(T first, Args&&... args) {
        PushValue(first);
        PushValues(std::forward<Args>(args)...);
    }

    /**
     * @brief Stop method to recursive PushValues overload.
     *
     * @tparam T Value type.
     * @param first Value pushed to Lua stack.
     */
    template <typename T>
    static void PushValues(T first) {
        PushValue(first);
    }

    /**
     * @brief Push a nil (null) value to Lua stack.
     */
    static void PushNull();

    /**
     * @brief Registers and exposes C++ class to Lua.
     *
     * @tparam T Class to be registered.
     * @param nameSpace Class namespace.
     */
    template <class T>
    static void RegisterClass(const char* nameSpace = nullptr) {
        moon_helpers::LuaClass<T>::Register(s_state, nameSpace);
    }

    /**
     * @brief Registers and exposes C++ anonymous function to Lua.
     *
     * @param name Name of the function to be used in Lua.
     * @param fn Function pointer.
     */
    static void RegisterFunction(const char* name, moon_types::LuaCFunction fn);

    /**
     * @brief Calls Lua function without arguments.
     *
     * @param name Name of the Lua function.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    static bool CallFunction(const char* name);

    /**
     * @brief Calls Lua function.
     *
     * @tparam Args Lua function argument types.
     * @param name Name of the Lua function.
     * @param args Lua function arguments.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename... Args>
    static bool CallFunction(const char* name, Args&&... args) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            lua_pop(s_state, 1);
            return false;
        }
        PushValues(std::forward<Args>(args)...);
        return checkStatus(lua_pcall(s_state, sizeof...(Args), 0, 0), "Failed to call LUA function");
    }

    /**
     * @brief Calls Lua function and saves return in lValue.
     *
     * @tparam T Return type for lValue.
     * @param lValue Return from Lua function.
     * @param name Name of the Lua function.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename T>
    static bool CallFunction(T& lValue, const char* name) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            lua_pop(s_state, 1);
            return false;
        }
        if (!checkStatus(lua_pcall(s_state, 0, 1, 0), "Failed to call LUA function")) {
            lValue = T{};
            lua_pop(s_state, 1);
            return false;
        }
        lValue = GetValue<T>(-1);
        lua_pop(s_state, 1);
        return true;
    }

    /**
     * @brief Calls Lua function and saves return in lValue.
     *
     * @tparam T Return type for lValue.
     * @tparam Args Args Lua function argument types.
     * @param lValue Return from Lua function.
     * @param name Name of the Lua function.
     * @param args Lua function arguments.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename T, typename... Args>
    static bool CallFunction(T& lValue, const char* name, Args&&... args) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            lua_pop(s_state, 1);
            return false;
        }
        PushValues(std::forward<Args>(args)...);
        if (!checkStatus(lua_pcall(s_state, sizeof...(Args), 1, 0), "Failed to call LUA function")) {
            lValue = T{};
            lua_pop(s_state, 1);
            return false;
        }
        lValue = GetValue<T>(-1);
        lua_pop(s_state, 1);
        return true;
    }

private:
    /**
     * @brief Lua state with static storage.
     */
    static lua_State* s_state;

    static std::function<void(const std::string&)> s_logger;

    /**
     * @brief Checks for lua status and returns if ok or not.
     *
     * @param status Status code obtained from lua function
     * @param errMessage Default error message to print if none is obtained from lua stack
     * @return true Lua ok
     * @return false Something failed
     */
    static bool checkStatus(int status, const char* errMessage = "");
};
