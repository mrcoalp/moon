#pragma once

#ifndef MOON_H
#define MOON_H

#include <algorithm>
#include <cstring>
#include <functional>
#include <lua.hpp>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#define MOON_DECLARE_CLASS(_class) \
    static bool GC;                \
    static moon::Binding<_class> Binding;

#define MOON_PROPERTY(_property)                           \
    int Get_##_property(lua_State* L) {                    \
        Moon::PushValue(_property);                        \
        return 1;                                          \
    }                                                      \
    int Set_##_property(lua_State* L) {                    \
        _property = Moon::GetValue<decltype(_property)>(); \
        return 0;                                          \
    }

#define MOON_METHOD(_name) int _name(lua_State* L)

#define MOON_DEFINE_BINDING(_class, _gc) \
    using BindingType = _class;          \
    bool BindingType::GC = _gc;          \
    moon::Binding<BindingType> BindingType::Binding = moon::Binding<BindingType>(#_class)

#define MOON_ADD_METHOD(_method) .AddMethod({#_method, &BindingType::_method})

#define MOON_ADD_PROPERTY(_prop) .AddProperty({#_prop, &BindingType::Get_##_prop, &BindingType::Set_##_prop})

#define MOON_ADD_PROPERTY_CUSTOM(_prop, _getter, _setter) .AddProperty({#_prop, &BindingType::_getter, &BindingType::_setter})

class Moon;  // Forward declaration to use as friend class

namespace moon {
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

class LuaRef {
public:
    LuaRef() = default;
    explicit LuaRef(lua_State* L) : m_state(L) {}
    LuaRef(lua_State* L, int index) : m_state(L) { Load(index); }

    /**
     * @brief Checks if key is set, and actions are allowed.
     *
     * @return true Valid actions, key is set.
     * @return false Invalid actions, key is not set.
     */
    [[nodiscard]] inline bool IsLoaded() const { return m_key != -1 && m_state != nullptr; }

    /**
     * @brief Getter for the Lua state.
     *
     * @return Lua state pointer.
     */
    [[nodiscard]] inline lua_State* GetState() const { return m_state; }

    /**
     * @brief Getter for the type of stored ref. Returns null if no reference was created.
     *
     * @return Type of ref stored.
     */
    [[nodiscard]] LuaType GetType() const {
        if (!IsLoaded()) {
            return LuaType::Null;
        }
        Push();
        auto type = static_cast<LuaType>(lua_type(m_state, -1));
        lua_pop(m_state, 1);
        return type;
    }

    /**
     * @brief Sets Lua state.
     *
     * @param L Lua state to set.
     */
    inline void SetState(lua_State* L) { m_state = L; }

    /**
     * @brief Loads reference.
     *
     * @param index Index of stack to create ref.
     * @return Whether or not the reference was loaded.
     */
    bool Load(int index) {
        if (m_state == nullptr) {
            return false;
        }
        Unload();  // Unload previous value, if any
        lua_pushvalue(m_state, index);
        luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
        lua_pushvalue(m_state, -2);
        m_key = luaL_ref(m_state, -2);
        lua_pop(m_state, 2);
        return IsLoaded();
    }

    /**
     * @brief Loads reference if type is equal to type check.
     *
     * @param index Index of stack to create ref.
     * @param typeCheck Type of reference to check.
     * @return Whether or not the reference was loaded.
     */
    bool Load(int index, LuaType typeCheck) {
        if (m_state == nullptr) {
            return false;
        }
        Unload();  // Unload previous value, if any
        lua_pushvalue(m_state, index);
        luaL_getmetatable(m_state, LUA_REF_HOLDER_META_NAME);
        lua_pushvalue(m_state, -2);
        if (static_cast<LuaType>(lua_type(m_state, -1)) != typeCheck) {
            lua_pop(m_state, 3);
            return false;
        }
        m_key = luaL_ref(m_state, -2);
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

/**
 * @brief Stores a Lua function in a table and saves its index. It can then be used to call that function.
 * When not needed, reference must be manually unloaded.
 */
class LuaFunction {
public:
    LuaFunction() = default;

    LuaFunction(lua_State* L, int index) {
        m_ref.SetState(L);
        m_ref.Load(index, LuaType::Function);
    }

    /**
     * @brief Push function to stack.
     */
    void Push() const { m_ref.Push(); }

    /**
     * @brief Unloads reference from lua metatable.
     */
    void Unload() { m_ref.Unload(); }

    /**
     * @brief Checks if ref is loaded.
     *
     * @return true Ref is loaded.
     * @return false Ref is not loaded.
     */
    [[nodiscard]] inline bool IsLoaded() const { return m_ref.IsLoaded(); }

    /**
     * @brief Calls saved lua function.
     *
     * @return true Function called with success.
     * @return false Function call returned errors or action is not valid.
     */
    bool Call() const {
        if (m_ref.IsLoaded()) {
            m_ref.Push();
            auto* state = m_ref.GetState();
            int status = lua_pcall(state, 0, 0, 0);
            return status == LUA_OK;
        }
        return false;
    }

    bool operator()() const { return Call(); }

private:
    /**
     * @brief Reference that handles function storage.
     */
    LuaRef m_ref;

    /**
     * @brief Befriend Moon class
     */
    friend class ::Moon;
};

/**
 * @brief Stores a ref to a lua table.
 */
class LuaDynamicMap {
public:
    LuaDynamicMap() = default;

    explicit LuaDynamicMap(lua_State* L) { m_ref.SetState(L); }

    LuaDynamicMap(lua_State* L, int index) {
        m_ref.SetState(L);
        m_ref.Load(index, LuaType::Table);
    }

    /**
     * @brief Push table to stack.
     */
    void Push() const { m_ref.Push(); }

    /**
     * @brief Unloads reference from lua metatable.
     */
    void Unload() { m_ref.Unload(); }

    /**
     * @brief Checks if ref is loaded.
     *
     * @return true Ref is loaded.
     * @return false Ref is not loaded.
     */
    [[nodiscard]] inline bool IsLoaded() const { return m_ref.IsLoaded(); }

private:
    /**
     * @brief Reference that handles table storage.
     */
    LuaRef m_ref;
};

/**
 * @brief Converts C++ class to Lua metatable.
 * LunaFive modded - http://lua-users.org/wiki/LunaFive
 * This modded version uses a different method to bind class methods.
 *
 * @tparam T Class to expose to Lua.
 */
template <class T>
class LuaClass {
public:
    struct PropertyType {
        const char* name;

        int (T::*getter)(lua_State*);

        int (T::*setter)(lua_State*);
    };

    struct FunctionType {
        const char* name;

        int (T::*func)(lua_State*);
    };

    /**
     * @brief Retrieves a wrapped class from the arguments passed to the func, specified by narg (position).
     * This func will raise an exception if the argument is not of the correct type.
     *
     * @param L Lua State
     * @param narg Position to check
     * @return T*
     */
    static T* Check(lua_State* L, int narg) {
        T** obj = static_cast<T**>(luaL_checkudata(L, narg, T::Binding.GetName()));
        if (!obj) {
            return nullptr;
        }             // LightCheck returns nullptr if not found.
        return *obj;  // pointer to T object
    }

    /**
     * @brief Retrieves a wrapped class from the arguments passed to the func, specified by narg (position).
     * This func will return nullptr if the argument is not of the correct type.  Useful for supporting
     * multiple types of arguments passed to the func
     *
     * @param L Lua State
     * @param narg Position to check
     * @return T*
     */
    static T* LightCheck(lua_State* L, int narg) {
        T** obj = static_cast<T**>(luaL_testudata(L, narg, T::Binding.GetName()));
        if (!obj) {
            return nullptr;
        }             // LightCheck returns nullptr if not found.
        return *obj;  // pointer to T object
    }

    /**
     * @brief Registers your class with Lua.  Leave nameSpace "" if you want to load it into the global space.
     *
     * @param L Lua State
     * @param nameSpace Namespace to load into
     */
    static void Register(lua_State* L, const char* nameSpace = nullptr) {
        if (nameSpace != nullptr && strlen(nameSpace) != 0u) {
            lua_getglobal(L, nameSpace);
            if (lua_isnil(L, -1))  // Create namespace if not present
            {
                lua_newtable(L);
                lua_pushvalue(L, -1);  // Duplicate table pointer since setglobal pops the value
                lua_setglobal(L, nameSpace);
            }
            lua_pushcfunction(L, &LuaClass<T>::constructor);
            lua_setfield(L, -2, T::Binding.GetName());
            lua_pop(L, 1);
        } else {
            lua_pushcfunction(L, &LuaClass<T>::constructor);
            lua_setglobal(L, T::Binding.GetName());
        }

        luaL_newmetatable(L, T::Binding.GetName());
        int metatable = lua_gettop(L);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, &LuaClass<T>::gc_obj);
        lua_settable(L, metatable);

        lua_pushstring(L, "__tostring");
        lua_pushcfunction(L, &LuaClass<T>::to_string);
        lua_settable(L, metatable);
        // To be able to compare two Luna objects (not natively possible with full userdata)
        lua_pushstring(L, "__eq");
        lua_pushcfunction(L, &LuaClass<T>::equals);
        lua_settable(L, metatable);

        lua_pushstring(L, "__index");
        lua_pushcfunction(L, &LuaClass<T>::property_getter);
        lua_settable(L, metatable);

        lua_pushstring(L, "__newindex");
        lua_pushcfunction(L, &LuaClass<T>::property_setter);
        lua_settable(L, metatable);

        unsigned i = 0;
        for (const auto& property : T::Binding.GetProperties()) {  // Register some properties in it
            lua_pushstring(L, property.name);                      // Having some string associated with them
            lua_pushnumber(L, i);                                  // And an index of which property it is
            lua_settable(L, metatable);
            ++i;
        }

        i = 0;
        for (const auto& method : T::Binding.GetMethods()) {
            lua_pushstring(L, method.name);     // Register some functions in it
            lua_pushnumber(L, i | (1u << 8u));  // Add a number indexing which func it is
            lua_settable(L, metatable);
            ++i;
        }
    }

    /**
     * @brief Loads an instance of the class into the Lua stack, and provides you a pointer so you can modify it.
     *
     * @param L Lua State
     * @param instance Instance to push
     */
    static void Push(lua_State* L, T* instance) {
        T** a = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));  // Create userdata
        *a = instance;
        luaL_getmetatable(L, T::Binding.GetName());
        lua_setmetatable(L, -2);
    }

private:
    /**
     * @brief constructor (internal)
     *
     * @param L Lua State
     * @return int
     */
    static int constructor(lua_State* L) {
        T* ap = new T(L);
        T** a = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));  // Push value = userdata
        *a = ap;

        luaL_getmetatable(L, T::Binding.GetName());  // Fetch global metatable T::classname
        lua_setmetatable(L, -2);
        return 1;
    }

    /**
     * @brief property_getter (internal)
     *
     * @param L Lua State
     * @return int
     */
    static int property_getter(lua_State* L) {
        lua_getmetatable(L, 1);  // Look up the index of a name
        lua_pushvalue(L, 2);     // Push the name
        lua_rawget(L, -2);       // Get the index

        if (lua_isnumber(L, -1) != 0) {  // Check if we got a valid index
            auto _index = static_cast<unsigned>(lua_tonumber(L, -1));

            T** obj = static_cast<T**>(lua_touserdata(L, 1));

            lua_pushvalue(L, 3);

            // A func
            if ((_index & (1u << 8u)) != 0u) {
                lua_pushnumber(L, _index ^ (1u << 8u));  // Push the right func index
                lua_pushlightuserdata(L, obj);
                lua_pushcclosure(L, &LuaClass<T>::function_dispatch, 2);
                return 1;  // Return a func
            }

            lua_pop(L, 2);     // Pop metatable and _index
            lua_remove(L, 1);  // Remove userdata
            lua_remove(L, 1);  // Remove [key]

            return ((*obj)->*(T::Binding.GetProperties()[_index].getter))(L);
        }

        return 1;
    }

    /**
     * @brief property_setter (internal)
     *
     * @param L Lua State
     * @return int
     */
    static int property_setter(lua_State* L) {
        lua_getmetatable(L, 1);  // Look up the index from name
        lua_pushvalue(L, 2);     //
        lua_rawget(L, -2);       //

        if (lua_isnumber(L, -1) != 0) {  // Check if we got a valid index
            auto _index = static_cast<unsigned>(lua_tonumber(L, -1));

            T** obj = static_cast<T**>(lua_touserdata(L, 1));

            if (!obj || !*obj) {
                luaL_error(L, "Internal error, no object given!");
                return 0;
            }

            if ((_index >> 8u) != 0u) {  // Try to set a func
                char c[128];
#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
                sprintf_s(c, "Moon: Trying to set the method [%s] of class [%s]", (*obj)->T::Binding.GetMethods()[_index ^ (1u << 8u)].name,
                          T::Binding.GetName());
#else
                sprintf(c, "Moon: Trying to set the method [%s] of class [%s]", (*obj)->T::Binding.GetMethods()[_index ^ (1u << 8u)].name,
                        T::Binding.GetName());
#endif
                luaL_error(L, c);
                return 0;
            }

            lua_pop(L, 2);     // Pop metatable and _index
            lua_remove(L, 1);  // Remove userdata
            lua_remove(L, 1);  // Remove [key]

            return ((*obj)->*(T::Binding.GetProperties()[_index].setter))(L);
        }

        return 0;
    }

    /**
     * @brief function_dispatch (internal)
     *
     * @param L Lua State
     * @return int
     */
    static int function_dispatch(lua_State* L) {
        int i = (int)lua_tonumber(L, lua_upvalueindex(1));
        T** obj = static_cast<T**>(lua_touserdata(L, lua_upvalueindex(2)));

        return ((*obj)->*(T::Binding.GetMethods()[i].func))(L);
    }

    /**
     * @brief gc_obj (internal)
     *
     * @param L Lua State
     * @return int
     */
    static int gc_obj(lua_State* L) {
        // NOTE(MPINTO): Defined in binding, whether or not to let Lua handle garbage collection
        // Useful to use C++ existent object and avoid seg faults
        if (T::GC) {
            T** obj = static_cast<T**>(lua_touserdata(L, -1));

            if (obj && *obj) {
                delete (*obj);
            }
        }

        return 0;
    }

    static int to_string(lua_State* L) {
        T** obj = static_cast<T**>(lua_touserdata(L, -1));

        if (obj) {
            lua_pushfstring(L, "%s (%p)", T::Binding.GetName(), (void*)*obj);
        } else {
            lua_pushstring(L, "Empty object");
        }

        return 1;
    }

    /**
     * @brief Method which compares two Luna objects.
     * The full userdatas (as opposed to light userdata) can't be natively compared one to other, we have to had this to
     * do it.
     *
     * @param L Lua State
     * @return int
     */
    static int equals(lua_State* L) {
        T** obj1 = static_cast<T**>(lua_touserdata(L, -1));
        T** obj2 = static_cast<T**>(lua_touserdata(L, 1));

        lua_pushboolean(L, *obj1 == *obj2);

        return 1;
    }
};

class Marshalling {
public:
    static inline int GetValue(moon::MarshallingType<int>, lua_State* L, int index) {
        ensure_type(lua_isinteger(L, index));
        return static_cast<int>(lua_tointeger(L, index));
    }

    static inline float GetValue(moon::MarshallingType<float>, lua_State* L, int index) {
        ensure_type(lua_isnumber(L, index));
        return static_cast<float>(lua_tonumber(L, index));
    }

    static inline double GetValue(moon::MarshallingType<double>, lua_State* L, int index) {
        ensure_type(lua_isnumber(L, index));
        return static_cast<double>(lua_tonumber(L, index));
    }

    static inline bool GetValue(moon::MarshallingType<bool>, lua_State* L, int index) {
        ensure_type(lua_isboolean(L, index));
        return lua_toboolean(L, index) != 0;
    }

    static inline unsigned GetValue(moon::MarshallingType<unsigned>, lua_State* L, int index) {
        ensure_type(lua_isinteger(L, index));
        return static_cast<unsigned>(lua_tointeger(L, index));
    }

    static inline std::string GetValue(moon::MarshallingType<std::string>, lua_State* L, int index) {
        ensure_type(lua_isstring(L, index));
        size_t size;
        const char* buffer = lua_tolstring(L, index, &size);
        return std::string{buffer, size};
    }

    static inline const char* GetValue(moon::MarshallingType<const char*>, lua_State* L, int index) {
        ensure_type(lua_isstring(L, index));
        return lua_tostring(L, index);
    }

    static inline void* GetValue(moon::MarshallingType<void*>, lua_State* L, int index) {
        ensure_type(lua_isuserdata(L, index));
        return lua_touserdata(L, index);
    }

    static inline moon::LuaFunction GetValue(moon::MarshallingType<moon::LuaFunction>, lua_State* L, int index) {
        ensure_type(lua_isfunction(L, index));
        return {L, index};
    }

    static inline moon::LuaDynamicMap GetValue(moon::MarshallingType<moon::LuaDynamicMap>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        return {L, index};
    }

    template <typename R>
    static inline R GetValue(moon::MarshallingType<R>, lua_State* L, int index) {
        ensure_type(lua_isuserdata(L, index));
        return static_cast<R>(lua_touserdata(L, index));
    }

    template <typename T>
    static std::vector<T> GetValue(moon::MarshallingType<std::vector<T>>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        auto size = (size_t)lua_rawlen(L, -1);
        std::vector<T> vec;
        vec.reserve(size);
        for (size_t i = 1; i <= size; ++i) {
            lua_pushinteger(L, i);
            lua_gettable(L, -2);
            if (lua_type(L, -1) == LUA_TNIL) {
                break;
            }
            vec.emplace_back(GetValue(moon::MarshallingType<T>{}, L, -1));
            lua_pop(L, 1);
        }
        return vec;
    }

    template <typename T>
    [[nodiscard]] static moon::LuaMap<T> GetValue(moon::MarshallingType<moon::LuaMap<T>>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        lua_pushnil(L);
        moon::LuaMap<T> map;
        while (lua_next(L, -2) != 0) {
            map.emplace(GetValue(moon::MarshallingType<std::string>{}, L, -2), GetValue(moon::MarshallingType<T>{}, L, -1));
            lua_pop(L, 1);
        }
        return map;
    }

    template <typename T>
    [[nodiscard]] static std::map<std::string, T> GetValue(moon::MarshallingType<std::map<std::string, T>>, lua_State* L, int index) {
        ensure_type(lua_istable(L, index));
        lua_pushnil(L);
        std::map<std::string, T> map;
        while (lua_next(L, -2) != 0) {
            map.emplace(GetValue(moon::MarshallingType<std::string>{}, L, -2), GetValue(moon::MarshallingType<T>{}, L, -1));
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

    static void PushValue(lua_State*, const moon::LuaRef& value) { value.Push(); }

    static void PushValue(lua_State*, const moon::LuaFunction& value) { value.Push(); }

    static void PushValue(lua_State*, const moon::LuaDynamicMap& value) { value.Push(); }

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
    static void PushValue(lua_State* L, const moon::LuaMap<T>& value) {
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

private:
    static void ensure_type(int check) {
        if (check != 1) {
            throw std::runtime_error("Lua type check failed!");
        }
    }
};

template <class BindableClass>
class Binding {
public:
    using LuaFunction = typename LuaClass<BindableClass>::FunctionType;
    using LuaProperty = typename LuaClass<BindableClass>::PropertyType;

    explicit Binding(const char* name) : m_name(name) {}

    ~Binding() = default;

    [[nodiscard]] inline const char* GetName() const { return m_name; }

    [[nodiscard]] inline const auto& GetMethods() const { return m_methods; }

    [[nodiscard]] inline const auto& GetProperties() const { return m_properties; }

    Binding& AddMethod(LuaFunction func) {
        m_methods.push_back(func);
        return *this;
    }

    Binding& AddProperty(LuaProperty prop) {
        m_properties.push_back(prop);
        return *this;
    }

private:
    const char* m_name;
    std::vector<LuaFunction> m_methods;
    std::vector<LuaProperty> m_properties;
};
}  // namespace moon

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
    static void Init() {
        s_state = luaL_newstate();
        luaL_openlibs(s_state);
        luaL_newmetatable(s_state, moon::LUA_REF_HOLDER_META_NAME);
    }

    /**
     * @brief Closes Lua state.
     */
    static void CloseState() {
        lua_close(s_state);
        s_state = nullptr;
    }

    /**
     * @brief Set a Lua state, providing pointer.
     *
     * @param state State to set.
     */
    static inline void SetState(lua_State* state) { s_state = state; }

    /**
     * @brief Set a logger callback to report errors.
     *
     * @param logger Callback which is gonna be called every time an error is captured.
     */
    static inline void SetLogger(const std::function<void(const std::string&)>& logger) { s_logger = logger; }

    /**
     * @brief Get the Lua state object.
     *
     * @return const lua_State*
     */
    static inline lua_State* GetState() { return s_state; }

    /**
     * @brief Getter for the current top index.
     *
     * @return Current lua stack top index.
     */
    static inline int GetTop() { return lua_gettop(s_state); }

    /**
     * @brief Returns a string containing all values current in the stack. Useful for debug purposes.
     */
    static std::string GetStackDump() {
        int top = lua_gettop(s_state);
        std::stringstream dump;
        dump << "***** LUA STACK *****" << std::endl;
        for (int i = 1; i <= top; ++i) {
            int t = lua_type(s_state, i);
            switch (t) {
                case LUA_TSTRING:
                    dump << GetValue<const char*>(i) << std::endl;
                    break;
                case LUA_TBOOLEAN:
                    dump << std::boolalpha << GetValue<bool>(i) << std::endl;
                    break;
                case LUA_TNUMBER:
                    dump << GetValue<double>(i) << std::endl;
                    break;
                default:  // NOTE(mpinto): Other values, print type
                    dump << lua_typename(s_state, t) << std::endl;
                    break;
            }
        }
        return dump.str();
    }

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
    static bool LoadFile(const char* filePath) {
        if (!checkStatus(luaL_loadfile(s_state, filePath), "Error loading file")) {
            return false;
        }
        return checkStatus(lua_pcall(s_state, 0, LUA_MULTRET, 0), "Loading file failed");
    }

    /**
     * @brief Runs Lua script snippet.
     *
     * @param code String to run.
     * @return true Success running script.
     * @return false Failed to run script.
     */
    static bool RunCode(const char* code) {
        if (!checkStatus(luaL_loadstring(s_state, code), "Error running code")) {
            return false;
        }
        return checkStatus(lua_pcall(s_state, 0, LUA_MULTRET, 0), "Running code failed");
    }

    /**
     * @brief Get the type of value in stack, at position index.
     *
     * @param index Position of stack to check.
     * @return LuaType
     */
    static moon::LuaType GetValueType(int index = 1) { return static_cast<moon::LuaType>(lua_type(s_state, index)); }

    /**
     * @brief Get value from Lua stack.
     *
     * @tparam R Value type.
     * @param index Stack index.
     * @return R
     */
    template <typename R>
    static inline R GetValue(const int index = 1) {
        try {
            return moon::Marshalling::GetValue(moon::MarshallingType<R>{}, s_state, index);
        } catch (const std::exception& e) {
            std::stringstream error;
            error << "An exception was caught by Moon: " << e.what();
            s_logger(error.str());
        }
        return {};
    }

    /**
     * @brief Get the global variable from Lua.
     *
     * @tparam R Type of returned variable.
     * @param name Name of the variable.
     * @return R Lua global variable.
     */
    template <typename R>
    static inline R GetGlobalVariable(const char* name) {
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
    static bool EnsureMapKeys(const std::vector<std::string>& keys, const moon::LuaMap<T>& map) {
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
        moon::Marshalling::PushValue(s_state, value);
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
    static void PushNull() { lua_pushnil(s_state); }

    /**
     * @brief Pushes a new empty table/map to stack.
     */
    static void PushTable() { lua_newtable(s_state); }

    /**
     * @brief Pops from stack provided nr of elements.
     *
     * @param nrOfValues Quantity of elements to pop from stack
     */
    static void Pop(int nrOfElements = 1) { lua_pop(s_state, nrOfElements); }

    /**
     * @brief Registers and exposes C++ class to Lua.
     *
     * @tparam T Class to be registered.
     * @param nameSpace Class namespace.
     */
    template <class T>
    static void RegisterClass(const char* nameSpace = nullptr) {
        moon::LuaClass<T>::Register(s_state, nameSpace);
    }

    /**
     * @brief Registers and exposes C++ anonymous function to Lua.
     *
     * @param name Name of the function to be used in Lua.
     * @param fn Function pointer.
     */
    static void RegisterFunction(const char* name, moon::LuaCFunction fn) { lua_register(s_state, name, fn); }

    /**
     * @brief Calls Lua function (top of stack) without arguments.
     *
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    static bool CallFunction() {
        if (!lua_isfunction(s_state, -1)) {
            return false;
        }
        return checkStatus(lua_pcall(s_state, 0, 0, 0), "Failed to call LUA function");
    }

    /**
     * @brief Calls Lua function without arguments.
     *
     * @param name Name of the Lua function.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    static bool CallFunction(const char* name) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            lua_pop(s_state, 1);
            return false;
        }
        return checkStatus(lua_pcall(s_state, 0, 0, 0), "Failed to call LUA function");
    }

    /**
     * @brief Calls Lua function (top of stack).
     *
     * @tparam Args Lua function argument types.
     * @param args Lua function arguments.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename... Args>
    static bool CallFunction(Args&&... args) {
        if (!lua_isfunction(s_state, -1)) {
            return false;
        }
        PushValues(std::forward<Args>(args)...);
        return checkStatus(lua_pcall(s_state, sizeof...(Args), 0, 0), "Failed to call LUA function");
    }

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
     * @brief Calls Lua function (top of stack) and saves return in lValue.
     *
     * @tparam T Return type for lValue.
     * @param lValue Return from Lua function.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename T>
    static bool CallFunction(T& lValue) {
        if (!lua_isfunction(s_state, -1)) {
            return false;
        }
        if (!checkStatus(lua_pcall(s_state, 0, 1, 0), "Failed to call LUA function")) {
            lValue = T{};
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
            return false;
        }
        lValue = GetValue<T>(-1);
        lua_pop(s_state, 1);
        return true;
    }

    /**
     * @brief Calls Lua function (top of stack) and saves return in lValue.
     *
     * @tparam T Return type for lValue.
     * @tparam Args Args Lua function argument types.
     * @param lValue Return from Lua function.
     * @param args Lua function arguments.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename T, typename... Args>
    static bool CallFunction(T& lValue, Args&&... args) {
        if (!lua_isfunction(s_state, -1)) {
            return false;
        }
        PushValues(std::forward<Args>(args)...);
        if (!checkStatus(lua_pcall(s_state, sizeof...(Args), 1, 0), "Failed to call LUA function")) {
            lValue = T{};
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
            return false;
        }
        lValue = GetValue<T>(-1);
        lua_pop(s_state, 1);
        return true;
    }

    /**
     * @brief Calls a LuaFunction ref stored in registry.
     *
     * @param function LuaFunction to call.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    static bool LuaFunctionCall(const moon::LuaFunction& function) { return function(); }

    /**
     * @brief Calls a LuaFunction ref stored in registry, with arguments.
     *
     * @tparam Args Arguments types.
     * @param function LuaFunction to call.
     * @param args Arguments to push to function.
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    template <typename... Args>
    static bool LuaFunctionCall(const moon::LuaFunction& function, Args&&... args) {
        if (function.m_ref.IsLoaded()) {
            function.m_ref.Push();
            PushValues(std::forward<Args>(args)...);
            auto* state = function.m_ref.GetState();
            int status = lua_pcall(state, sizeof...(Args), 0, 0);
            return status == LUA_OK;
        }
        return false;
    }

    /**
     * @brief Sets top of stack as a global variable.
     *
     * @param name Name of the variable to set.
     */
    static void SetGlobalVariable(const char* name) { lua_setglobal(s_state, name); }

    /**
     * @brief Cleans/nulls a global variable.
     *
     * @param name Variable to clean.
     */
    static void CleanGlobalVariable(const char* name) {
        PushNull();
        SetGlobalVariable(name);
    }

    /**
     * @brief Creates and stores a new ref of element at provided index.
     *
     * @param index Index of element to create ref. Defaults to top of stack.
     * @return A new LuaRef.
     */
    static moon::LuaRef CreateRef(int index = -1) { return {s_state, index}; }

    /**
     * @brief Creates a new empty dynamic map and stores it as a ref.
     *
     * @return Empty LuaDynamicMap
     */
    static moon::LuaDynamicMap CreateDynamicMap() {
        PushTable();
        moon::LuaDynamicMap map(s_state, -1);
        Pop();
        return map;
    }

private:
    /**
     * @brief Lua state with static storage.
     */
    inline static lua_State* s_state{nullptr};

    /**
     * @brief Logger callback to be defined by client.
     */
    inline static std::function<void(const std::string&)> s_logger{[](const std::string&) {}};

    /**
     * @brief Checks for lua status and returns if ok or not.
     *
     * @param status Status code obtained from lua function
     * @param errMessage Default error message to print if none is obtained from lua stack
     * @return true Lua ok
     * @return false Something failed
     */
    static bool checkStatus(int status, const char* errMessage = "") {
        if (status != LUA_OK) {
            if (status == LUA_ERRSYNTAX) {
                const char* msg = lua_tostring(s_state, -1);
                s_logger(msg != nullptr ? msg : errMessage);
            } else if (status == LUA_ERRFILE) {
                const char* msg = lua_tostring(s_state, -1);
                s_logger(msg != nullptr ? msg : errMessage);
            }
            return false;
        }
        return true;
    }
};

#endif
