#pragma once

#ifndef MOON_H
#define MOON_H

#include <cstring>
#include <functional>
#include <lua.hpp>
#include <map>
#include <optional>
#include <sstream>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "object.h"
#include "stateview.h"

#define MOON_DECLARE_CLASS(_class) static moon::Binding<_class> Binding;

#define MOON_PROPERTY(_property)                       \
    int Get_##_property(lua_State* L) {                \
        Moon::Push(_property);                         \
        return 1;                                      \
    }                                                  \
    int Set_##_property(lua_State* L) {                \
        _property = Moon::Get<decltype(_property)>(1); \
        return 0;                                      \
    }

#define MOON_METHOD(_name) int _name(lua_State* L)

#define MOON_DEFINE_BINDING(_class) \
    using BindingType = _class;     \
    moon::Binding<BindingType> BindingType::Binding = moon::Binding<BindingType>(#_class)

#define MOON_REMOVE_GC .RemoveGC()

#define MOON_ADD_METHOD(_method) .AddMethod({#_method, &BindingType::_method})

#define MOON_ADD_PROPERTY(_prop) .AddProperty({#_prop, &BindingType::Get_##_prop, &BindingType::Set_##_prop})

#define MOON_ADD_PROPERTY_CUSTOM(_prop, _getter, _setter) .AddProperty({#_prop, &BindingType::_getter, &BindingType::_setter})

/// Handles all the logic related to "communication" between C++ and Lua, initializing it.
/// Acts as an engine to allow to call C++ functions from Lua and vice-versa, registers
/// and exposes C++ classes to Lua, pops and pushes values to Lua stack, runs file scripts and
/// string scripts, allows easy access to nested properties in Lua objects, as well as global ones.
class Moon {
public:
    /// Initializes Lua state, opens libs and initializes helper classes.
    static void Init() {
        s_state = luaL_newstate();
        luaL_openlibs(s_state);
        moon::Logger::SetCallback([](moon::Logger::Level, const std::string&) {});
        moon::Invokable::Register(s_state);
        moon::StateView::Instance().initialize(s_state);
    }

    /// Closes Lua state.
    static inline void CloseState() {
        lua_close(s_state);
        s_state = nullptr;
    }

    /// Set a logger callback.
    /// \param logger Callback which is gonna be called every time a log occurs.
    static inline void SetLogger(std::function<void(moon::Logger::Level, const std::string&)> logger) {
        moon::Logger::SetCallback(std::move(logger));
    }

    /// Getter for Lua state/stack.
    /// \return Lua state pointer.
    static inline lua_State* GetState() { return s_state; }

    /// Getter for top index in Lua stack.
    /// \return Lua stack top index.
    static inline int GetTop() { return lua_gettop(s_state); }

    /// Checks if provided index is valid within the range of current Lua stack.
    /// \param index Index to check.
    /// \return Whether or not index is valid.
    static bool IsValidIndex(int index) {
        int top = GetTop();
        if (index == 0 || index > top) {
            return false;
        }
        if (index < 0) {
            return IsValidIndex(top + index + 1);
        }
        return true;
    }

    /// Lua stack can be accessed with positive (up de stack) or negative (down the stack) indexes.
    /// This method converts a negative index to a positive one. Returns original if already positive.
    /// \param index Index to convert.
    /// \return Newly converted index.
    static inline int ConvertNegativeIndex(int index) { return lua_absindex(s_state, index); }

    /// Loads specified file script.
    /// \param filePath File path to load.
    /// \return Whether or not file was successfully loaded.
    static bool LoadFile(const char* filePath) {
        if (!checkStatus(luaL_loadfile(s_state, filePath), "Error loading file")) {
            return false;
        }
        return checkStatus(lua_pcall(s_state, 0, LUA_MULTRET, 0), "Loading file failed");
    }

    /// Runs Lua code snippet.
    /// \param code Code to run.
    /// \return Whether or not code ran without errors.
    static bool RunCode(const char* code) {
        if (!checkStatus(luaL_loadstring(s_state, code), "Error running code")) {
            return false;
        }
        return checkStatus(lua_pcall(s_state, 0, LUA_MULTRET, 0), "Running code failed");
    }

    /// Get type of Lua value at index in stack or with specific name.
    /// \tparam Keys Type of key(s) (integer/string).
    /// \param keys Key(s) to check type of. When multiple are provided, nested global search will be used.
    /// \return Moon type of value.
    template <typename... Keys>
    static inline moon::LuaType GetType(Keys&&... keys) {
        return moon::Core::GetType(s_state, std::forward<Keys>(keys)...);
    }

    /// Checks if Lua value in index/name can be interpreted as specified C type.
    /// \tparam T C type to check.
    /// \tparam Keys Type of key (integer/string).
    /// \param keys Key to check. When multiple are provided, nested global search will be used.
    /// \return Whether or not value can be interpreted at specific type.
    template <typename T, typename... Keys>
    static inline bool Check(Keys&&... keys) {
        return moon::Core::Check<T>(s_state, std::forward<Keys>(keys)...);
    }

    /// Cleans/nulls a variable, nested or not.
    /// \tparam Keys Key type forwarding.
    /// \param keys Nested (or not) keys path to arrive at variable to clean.
    template <typename... Keys>
    static inline void Clean(Keys&&... keys) {
        moon::Core::Clean(s_state, std::forward<Keys>(keys)...);
    }

    /// Gets element(s) at specified Lua stack index and/or global name as C object.
    /// \tparam Rets C type(s) to cast Lua object to.
    /// \tparam Keys Integral or string
    /// \param keys Index or global name to get from Lua stack.
    /// \return C object.
    template <typename... Rets, typename... Keys>
    static inline decltype(auto) Get(Keys&&... keys) {
        return moon::Core::Get<Rets...>(s_state, std::forward<Keys>(keys)...);
    }

    /// Gets a single nested (or global, if single key is provided) value from Lua global table.
    /// \tparam Ret C type to cast Lua object to.
    /// \tparam Keys Integral or string.
    /// \param keys Path to arrive at variable.
    /// \return C object.
    template <typename Ret, typename... Keys>
    static inline decltype(auto) GetNested(Keys&&... keys) {
        return moon::Core::GetNested<Ret>(s_state, std::forward<Keys>(keys)...);
    }

    /// Sets one or multiple pairs name/value globals in Lua.
    /// \tparam Pairs Pair(s) type(s).
    /// \param pairs Name and value pair to set.
    /// \example Set("first", 1, "second", true,...)
    template <typename... Pairs>
    static inline void Set(Pairs&&... pairs) {
        moon::Core::Set(s_state, std::forward<Pairs>(pairs)...);
    }

    /// Sets a nested (or global) variable value. New keys are also created in table.
    /// \example A lua table: a = { b = 2 }, can become a = { b = 2, c = 3 } with Moon::SetNested("a", "c", 3)
    /// \tparam Args Argument types.
    /// \param args The last argument must be the value to set variable, the others, the path to the variable.
    /// \example Lua: map = { x = 1, y = { z = 3 } } => To set "z" as 6 =>
    /// Moon::SetNested("map", "y", "z", 6)
    template <typename... Args>
    static inline void SetNested(Args&&... args) {
        moon::Core::SetNested(s_state, std::forward<Args>(args)...);
    }

    /// Recursive helper method to push multiple values directly to Lua stack.
    /// \tparam Args Values types.
    /// \param args Values to push to Lua stack.
    template <typename... Args>
    static inline void Push(Args&&... args) {
        (moon::Core::Push(s_state, std::forward<Args>(args)), ...);
    }

    /// Get, assign or call a global variable from Lua. Assignment operator, callable and implicit conversion are enabled for ease of use.
    /// \tparam Key Type of key.
    /// \param key Global variable name or stack index.
    /// \return Lookup proxy with assignment, call and implicit cast enabled.
    template <typename Key>
    static inline moon::LookupProxy<moon::StateView, Key> At(Key&& key) {
        return {&moon::StateView::Instance(), std::forward<Key>(key)};
    }

    /// Access/create global scope and nested variables/functions in tables, directly with stl style operators, like [] and ().
    /// \example auto& view = Moon::View();<br>
    /// view["int"] = 2;<br>
    /// view["string"] = "passed";<br>
    /// view["f"] = [](bool test) { return test; };<br>
    /// view["f"](true);<br>
    /// view["map"]["x"][1] = 2;
    /// \return
    static inline moon::StateView& View() { return moon::StateView::Instance(); }

    /// Push a nil (null) value to Lua stack.
    static inline void PushNull() { lua_pushnil(s_state); }

    /// Pushes a new empty table/map to stack.
    static inline void PushTable() { lua_newtable(s_state); }

    /// Tries to pop specified number of elements from stack. Logs an error if top is reached not popping any more.
    /// \param nrOfElements Number of elements to pop from Lua stack.
    static void Pop(int nrOfElements = 1) {
        while (nrOfElements > 0) {
            if (GetTop() <= 0) {
                moon::Logger::Warning("tried to pop stack but was empty already");
                break;
            }
            lua_pop(s_state, 1);
            --nrOfElements;
        }
    }

    /// Registers and exposes C++ class to Lua.
    /// \tparam T Class to be registered.
    /// \param nameSpace Class namespace.
    template <class T>
    static inline void RegisterClass(const char* nameSpace = nullptr) {
        moon::LuaClass<T>::Register(s_state, nameSpace);
    }

    /// Registers and exposes C++ function to Lua.
    /// \tparam Name Name type forwarding.
    /// \tparam Func Function type.
    /// \param name Function name to register in Lua.
    /// \param func Function to register.
    template <typename Name, typename Func>
    static inline void RegisterFunction(Name&& name, Func&& func) {
        moon::Core::PushFunction(s_state, std::forward<Func>(func));
        moon::Core::FieldHandler<true, Name>{}.Set(s_state, -1, std::forward<Name>(name));
    }

    /// Calls a global Lua function.
    /// \tparam Ret Return types.
    /// \tparam Name Name type forwarding.
    /// \tparam Args Argument types.
    /// \param name Global function name.
    /// \param args Arguments to call function with.
    /// \return Return value of function.
    template <typename... Ret, typename Key, typename... Args>
    static inline decltype(auto) Call(Key&& key, Args&&... args) {
        moon::Core::FieldHandler<true, Key>{}.Get(s_state, 0, std::forward<Key>(key));
        return moon::Core::Call<Ret...>(s_state, std::forward<Args>(args)...);
    }

    /// Make moon object directly in Lua stack from C object. Stack is immediately popped, since the reference is stored.
    /// \tparam T C object type.
    /// \param value C object.
    /// \return Newly created moon object.
    template <typename T>
    static inline moon::Object MakeObject(T&& value) {
        moon::Core::Push(s_state, std::forward<T>(value));
        return moon::Object::CreateAndPop(s_state);
    }

    /// Creates and stores a new ref of element at provided index.
    /// \param index Index of element to create ref. Defaults to top of stack.
    /// \return A new moon Object.
    static inline moon::Object MakeObjectFromIndex(int index = -1) { return {s_state, index}; }

    /// Prints element at specified index. Shows value when possible or type otherwise.
    /// \param index Index in stack to print.
    /// \return String log of element.
    static std::string StackElementToStringDump(int index) {
        if (!IsValidIndex(index)) {
            moon::Logger::Warning("tried to print element at invalid index");
            return "";
        }
        index = ConvertNegativeIndex(index);

        static auto printArray = [](int index, size_t size) -> std::string {
            std::stringstream dump;
            dump << "[";
            for (size_t i = 1; i <= size; ++i) {
                lua_pushinteger(s_state, i);
                lua_gettable(s_state, index);
                int top = GetTop();
                if (lua_type(s_state, top) == LUA_TNIL) {
                    break;
                }
                dump << StackElementToStringDump(top);
                if (i + 1 <= size) {
                    dump << ", ";
                }
                Pop();
            }
            dump << "]";
            return dump.str();
        };

        static auto printMap = [](int index) -> std::string {
            std::stringstream dump;
            dump << "{";
            PushNull();
            bool first = true;
            while (lua_next(s_state, index) != 0) {
                if (!first) {
                    dump << ", ";
                }
                first = false;
                dump << "\"" << Get<std::string>(-2) << "\": " << StackElementToStringDump(GetTop());
                Pop();
            }
            dump << "}";
            return dump.str();
        };

        moon::LuaType type = GetType(index);
        switch (type) {
            case moon::LuaType::Boolean: {
                return Get<bool>(index) ? "true" : "false";
            }
            case moon::LuaType::Number: {
                return std::to_string(Get<double>(index));
            }
            case moon::LuaType::String: {
                return "\"" + Get<std::string>(index) + "\"";
            }
            case moon::LuaType::Table: {
                auto size = (size_t)lua_rawlen(s_state, index);
                return size > 0 ? printArray(index, size) : printMap(index);
            }
            default: {  // NOTE(mpinto): Other values, print type
                return lua_typename(s_state, (int)type);
            }
        }
    }

    /// Returns current Lua stack as string. Tries to show values when possible or types otherwise.
    /// \return String containing all current Lua stack elements.
    static std::string GetStackDump() {
        int top = GetTop();
        std::stringstream dump;
        dump << "***** LUA STACK *****" << std::endl;
        for (int i = 1; i <= top; ++i) {
            int invertedIndex = top - i + 1;
            dump << i << " (-" << invertedIndex << ") => " << StackElementToStringDump(i) << std::endl;
        }
        return dump.str();
    }

    /// Logs current stack to logger.
    static inline void LogStackDump() { moon::Logger::Info(GetStackDump()); }

    /// Ensures that a given LuaMap contains all desired keys. Useful, since maps obtained from lua are dynamic.
    /// \tparam T LuaMap type.
    /// \param keys Keys to check in map.
    /// \param map Map to check.
    /// \return True if map contains all keys, false otherwise.
    template <typename T>
    static inline bool EnsureMapKeys(const std::vector<std::string>& keys, const moon::LuaMap<T>& map) {
        return std::all_of(keys.cbegin(), keys.cend(), [&map](const std::string& key) { return map.find(key) != map.cend(); });
    }

private:
    /// Checks for lua status and returns if ok or not.
    /// \param status Status code obtained from lua function.
    /// \param errMessage Default error message to print if none is obtained from lua stack.
    /// \return Whether or not no error occurred.
    static bool checkStatus(int status, const char* errMessage = "") {
        if (status != LUA_OK) {
            const auto* msg = Get<const char*>(-1);
            Pop();
            moon::Logger::Error(msg != nullptr ? msg : errMessage);
            return false;
        }
        return true;
    }

    /// Lua state with static storage.
    static inline lua_State* s_state{nullptr};
};

#endif
