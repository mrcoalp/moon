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
#include <type_traits>
#include <unordered_map>
#include <vector>

#define MOON_DECLARE_CLASS(_class) static moon::Binding<_class> Binding;

#define MOON_PROPERTY(_property)                      \
    int Get_##_property(lua_State* L) {               \
        Moon::Push(_property);                        \
        return 1;                                     \
    }                                                 \
    int Set_##_property(lua_State* L) {               \
        _property = Moon::Get<decltype(_property)>(); \
        return 0;                                     \
    }

#define MOON_METHOD(_name) int _name(lua_State* L)

#define MOON_DEFINE_BINDING(_class) \
    using BindingType = _class;     \
    moon::Binding<BindingType> BindingType::Binding = moon::Binding<BindingType>(#_class)

#define MOON_REMOVE_GC .RemoveGC()

#define MOON_ADD_METHOD(_method) .AddMethod({#_method, &BindingType::_method})

#define MOON_ADD_PROPERTY(_prop) .AddProperty({#_prop, &BindingType::Get_##_prop, &BindingType::Set_##_prop})

#define MOON_ADD_PROPERTY_CUSTOM(_prop, _getter, _setter) .AddProperty({#_prop, &BindingType::_getter, &BindingType::_setter})

class Moon;  // Forward declaration to use as friend class

namespace moon {
constexpr const char* LUA_REF_HOLDER_META_NAME{"LuaRefHolder"};
constexpr const char* LUA_INVOKABLE_HOLDER_META_NAME{"LuaInvokableHolder"};

using LuaCFunction = int (*)(lua_State*);

template <typename T>
using LuaMap = std::unordered_map<std::string, T>;

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

class Reference {
public:
    Reference() = default;

    Reference(lua_State* L, int index) {
        Unload(L);  // Unload previous value, if any
        lua_pushvalue(L, index);
        luaL_getmetatable(L, LUA_REF_HOLDER_META_NAME);
        lua_pushvalue(L, -2);
        m_key = luaL_ref(L, -2);
        lua_pop(L, 2);
    }

    explicit Reference(lua_State* L) : Reference(L, -1) {}

    Reference(const Reference&) noexcept = delete;

    Reference(Reference&& reference) noexcept : m_key(reference.m_key) { reference.m_key = LUA_NOREF; }

    Reference& operator=(const Reference&) noexcept = delete;

    Reference& operator=(Reference&& reference) noexcept {
        m_key = reference.m_key;
        reference.m_key = LUA_NOREF;
        return *this;
    }

    static void Register(lua_State* L) {
        luaL_newmetatable(L, LUA_REF_HOLDER_META_NAME);
        lua_pop(L, 1);
    }

    /**
     * @brief Checks if key is set, and actions are allowed.
     *
     * @return true Valid actions, key is set.
     * @return false Invalid actions, key is not set.
     */
    [[nodiscard]] inline bool IsLoaded() const { return m_key != LUA_NOREF && m_key != LUA_REFNIL; }

    /**
     * @brief Unloads reference from lua metatable and resets key.
     */
    void Unload(lua_State* L) {
        if (IsLoaded()) {
            luaL_getmetatable(L, LUA_REF_HOLDER_META_NAME);
            luaL_unref(L, -1, m_key);
            lua_pop(L, 1);
            m_key = LUA_NOREF;
        }
    }

    /**
     * @brief Push value to stack.
     */
    int Push(lua_State* L) const {
        if (!IsLoaded()) {
            if (L != nullptr) {
                lua_pushnil(L);
                return 1;
            }
            return 0;
        }
        luaL_getmetatable(L, LUA_REF_HOLDER_META_NAME);
        lua_rawgeti(L, -1, m_key);
        lua_remove(L, -2);
        return 1;
    }

    /**
     * @brief Getter for the type of stored ref. Returns null if no reference was created.
     *
     * @return Type of ref stored.
     */
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

    int copy(lua_State* L) const {
        Push(L);
        luaL_getmetatable(L, LUA_REF_HOLDER_META_NAME);
        lua_pushvalue(L, -2);
        int key = luaL_ref(L, -2);
        lua_pop(L, 2);
        return key;
    }

private:
    /**
     * @brief Key to later retrieve lua function.
     */
    int m_key{LUA_NOREF};
};

class BasicReference : public Reference {
public:
    BasicReference() = default;

    explicit BasicReference(lua_State* L) : m_state(L), Reference(L, -1) {}

    BasicReference(lua_State* L, int index) : m_state(L), Reference(L, index) {}

    BasicReference(const BasicReference& reference) : m_state(reference.m_state), Reference(reference.copy(reference.m_state)) {}

    BasicReference(BasicReference&& reference) noexcept : m_state(reference.m_state), Reference(std::move(reference)) { reference.m_state = nullptr; }

    ~BasicReference() {
        if (m_state == nullptr) {
            return;
        }
        Unload();
    }

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
    [[nodiscard]] LuaType GetType() const { return Reference::GetType(m_state); }

    /**
     * @brief Sets Lua state.
     *
     * @param L Lua state to set.
     */
    inline void SetState(lua_State* L) { m_state = L; }

    /**
     * @brief Unloads reference from lua metatable and resets key.
     */
    void Unload() { Reference::Unload(m_state); }

    /**
     * @brief Push value to stack.
     */
    void Push() const { Reference::Push(m_state); }

private:
    /**
     * @brief Lua state.
     */
    lua_State* m_state{nullptr};
};

/**
 * @brief Stores a Lua function in a table and saves its index. It can then be used to call that function.
 * When not needed, reference must be manually unloaded.
 */
class LuaFunction : public BasicReference {
public:
    LuaFunction() = default;

    explicit LuaFunction(lua_State* L) : BasicReference(L) {}

    LuaFunction(lua_State* L, int index) : BasicReference(L, index) {}

    /**
     * @brief Calls saved lua function.
     *
     * @return true Function called with success.
     * @return false Function call returned errors or action is not valid.
     */
    [[nodiscard]] bool Call() const {
        if (IsLoaded()) {
            Push();
            return lua_pcall(GetState(), 0, 0, 0) == LUA_OK;
        }
        return false;
    }

    bool operator()() const { return Call(); }
};

/**
 * @brief Stores a ref to a lua table.
 */
class LuaDynamicMap : public BasicReference {
public:
    LuaDynamicMap() = default;

    explicit LuaDynamicMap(lua_State* L) : BasicReference(L) {}

    LuaDynamicMap(lua_State* L, int index) : BasicReference(L, index) {}
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
            // Create namespace if not present
            if (lua_isnil(L, -1)) {
                lua_pop(L, 1);
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

        if (T::Binding.GetGC()) {
            lua_pushstring(L, "__gc");
            lua_pushcfunction(L, &LuaClass<T>::gc_obj);
            lua_settable(L, metatable);
        }

        unsigned i = 0;
        for (const auto& property : T::Binding.GetProperties()) {  // Register some properties in it
            lua_pushstring(L, property.name);                      // Having some string associated with them
            lua_pushnumber(L, i++);                                // And an index of which property it is
            lua_settable(L, metatable);
        }

        i = 0;
        for (const auto& method : T::Binding.GetMethods()) {
            lua_pushstring(L, method.name);       // Register some functions in it
            lua_pushnumber(L, i++ | (1u << 8u));  // Add a number indexing which func it is
            lua_settable(L, metatable);
        }
        lua_pop(L, 1);
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
#if !defined(__MINGW32__) && (defined(_WIN32) || defined(__WIN32__) || defined(WIN32))
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
        T** obj = static_cast<T**>(lua_touserdata(L, -1));

        if (obj && *obj) {
            delete (*obj);
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

    [[nodiscard]] inline bool GetGC() const { return m_gc; }

    Binding& RemoveGC() {
        m_gc = false;
        return *this;
    }

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
    bool m_gc{true};
};

template <typename T>
using IsBool = std::enable_if_t<std::is_same_v<T, bool>, T>;
template <typename T>
using IsNumber = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>, T>;
template <typename T>
using IsString = std::enable_if_t<std::is_same_v<T, std::string>, T>;
template <typename T>
using IsCString = std::enable_if_t<std::is_same_v<T, const char*>, T>;
template <typename T>
using IsLuaRef = std::enable_if_t<std::is_base_of_v<Reference, T>, T>;
template <typename T>
using IsPointer = std::enable_if_t<std::is_pointer_v<T> && !std::is_same_v<T, const char*>, T>;
template <typename T>
struct Vector : std::false_type {};
template <typename T>
struct Vector<std::vector<T>> : std::true_type {};
template <typename T>
using IsVector = std::enable_if_t<Vector<T>::value, T>;
template <typename T>
struct Map : std::false_type {};
template <typename T>
struct Map<LuaMap<T>> : std::true_type {};
template <typename T>
struct Map<std::map<std::string, T>> : std::true_type {};
template <typename T>
using IsMap = std::enable_if_t<Map<T>::value, T>;
template <typename T>
using IsBinding = std::enable_if_t<std::is_same_v<decltype(T::Binding), Binding<T>>, T>;

class Marshalling {
public:
    template <typename R>
    static IsBool<R> GetValue(lua_State* L, int index) {
        assertType(lua_isboolean(L, index));
        return lua_toboolean(L, index) != 0;
    }

    template <typename R>
    static IsNumber<R> GetValue(lua_State* L, int index) {
        assertType(lua_isnumber(L, index));
        return static_cast<R>(lua_tonumber(L, index));
    }

    template <typename R>
    static IsString<R> GetValue(lua_State* L, int index) {
        assertType(lua_isstring(L, index));
        size_t size;
        const char* buffer = lua_tolstring(L, index, &size);
        return R{buffer, size};
    }

    template <typename R>
    static IsCString<R> GetValue(lua_State* L, int index) {
        assertType(lua_isstring(L, index));
        return lua_tostring(L, index);
    }

    template <typename R>
    static IsLuaRef<R> GetValue(lua_State* L, int index) {
        return {L, index};
    }

    template <typename R>
    static IsPointer<R> GetValue(lua_State* L, int index) {
        assertType(lua_isuserdata(L, index));
        return *static_cast<R*>(lua_touserdata(L, index));
    }

    template <typename R>
    static IsBinding<R> GetValue(lua_State* L, int index) {
        assertType(lua_isuserdata(L, index));
        return *GetValue<R*>(L, index);
    }

    template <typename R>
    static IsVector<R> GetValue(lua_State* L, int index) {
        assertType(lua_istable(L, index));
        ensureValidIndexInRecursion(index, L);
        auto size = (size_t)lua_rawlen(L, index);
        R vec;
        vec.reserve(size);
        for (size_t i = 1; i <= size; ++i) {
            lua_pushinteger(L, i);
            lua_gettable(L, index);
            if (lua_type(L, -1) == LUA_TNIL) {
                lua_pop(L, 1);
                break;
            }
            vec.emplace_back(std::move(GetValue<typename R::value_type>(L, lua_gettop(L))));
            lua_pop(L, 1);
        }
        return vec;
    }

    template <typename R>
    static IsMap<R> GetValue(lua_State* L, int index) {
        assertType(lua_istable(L, index));
        ensureValidIndexInRecursion(index, L);
        lua_pushnil(L);
        R map;
        while (lua_next(L, index) != 0) {
            map.emplace(std::move(GetValue<std::string>(L, -2)), std::move(GetValue<typename R::mapped_type>(L, lua_gettop(L))));
            lua_pop(L, 1);
        }
        return map;
    }

    static void PushValue(lua_State* L, bool value) { lua_pushboolean(L, (int)value); }

    static void PushValue(lua_State* L, int value) { lua_pushinteger(L, value); }

    static void PushValue(lua_State* L, unsigned value) { lua_pushinteger(L, value); }

    static void PushValue(lua_State* L, float value) { lua_pushnumber(L, value); }

    static void PushValue(lua_State* L, double value) { lua_pushnumber(L, value); }

    static void PushValue(lua_State* L, const std::string& value) { lua_pushstring(L, value.c_str()); }

    static void PushValue(lua_State* L, const char* value) { lua_pushstring(L, value); }

    static void PushValue(lua_State* L, void* value) { lua_pushlightuserdata(L, value); }

    static void PushValue(lua_State* L, moon::LuaCFunction value) { lua_pushcfunction(L, value); }

    static void PushValue(lua_State* L, const moon::Reference& value) { value.Push(L); }

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
    static std::enable_if_t<std::is_same_v<decltype(Class::Binding), Binding<Class>>, void> PushValue(lua_State* L, Class* value) {
        auto** a = static_cast<Class**>(lua_newuserdata(L, sizeof(Class*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, Class::Binding.GetName());
        lua_setmetatable(L, -2);
    }

    template <typename Class>
    static void PushValue(lua_State* L, Class* value, const char* name) {
        auto** a = static_cast<Class**>(lua_newuserdata(L, sizeof(Class*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, name);
        lua_setmetatable(L, -2);
    }

private:
    /**
     * @brief Asserts the type of value.
     *
     * @param check Whether or not the type check was successful.
     */
    static void assertType(int check) {
        if (check != 1) {
            throw std::runtime_error("Lua type check failed!");
        }
    }

    /**
     * @brief When using more complex data containers (like maps or vectors) recursion inside this can not be made with negative indexes.
     * If we try to get, for example, a std::vector<std::vector<int>> and provide -1 as index, in the recursive call to get the value, we would always
     * access the last value in the stack, which is not the right element (since we push indexes of the vector). We must, in this case, convert to the
     * real index in the stack, by the right order.
     *
     * @param index Index to check and adapt if needed.
     * @param L Lua state pointer.
     */
    static void ensureValidIndexInRecursion(int& index, lua_State* L) {
        if (index < 0) {
            index = lua_gettop(L) + index + 1;
        }
    }
};

template <int... is>
struct Indices {};

template <int n, int... is>
struct BuildIndices : BuildIndices<n - 1, n - 1, is...> {};

template <int... is>
struct BuildIndices<0, is...> : Indices<is...> {};

class LuaInvokable {
public:
    virtual ~LuaInvokable() = default;

    virtual int Call(lua_State*) const = 0;

    static void Register(lua_State* L) {
        luaL_newmetatable(L, moon::LUA_INVOKABLE_HOLDER_META_NAME);
        int metatable = lua_gettop(L);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, &LuaInvokable::call);
        lua_settable(L, metatable);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, &LuaInvokable::gc);
        lua_settable(L, metatable);

        lua_pushstring(L, "__metatable");
        lua_pushstring(L, "Access restricted");
        lua_settable(L, metatable);

        lua_pop(L, 1);
    }

protected:
    static inline std::function<void(const std::string&)> s_logger;

private:
    static int call(lua_State* L) {
        void* storage = lua_touserdata(L, 1);
        auto* invokable = *static_cast<moon::LuaInvokable**>(storage);
        lua_remove(L, 1);
        return invokable->Call(L);
    }

    static int gc(lua_State* L) {
        void* storage = lua_touserdata(L, 1);
        auto* invokable = *static_cast<moon::LuaInvokable**>(storage);
        delete invokable;
        return 0;
    }

    friend class ::Moon;
};

template <typename Ret, typename... Args>
class LuaInvokableCPP : public LuaInvokable {
public:
    explicit LuaInvokableCPP(std::function<Ret(Args...)> func_) : func(std::move(func_)) {}

    inline int Call(lua_State* L) const final { return callHelper(BuildIndices<sizeof...(Args)>{}, func, L); }

private:
    std::function<Ret(Args...)> func;

    template <int... indices, typename RetHelper>
    static int callHelper(Indices<indices...>, const std::function<RetHelper(Args...)>& func, lua_State* L) {
        try {
            auto output = func(std::move(Marshalling::GetValue<Args>(L, indices + 1))...);
            Marshalling::PushValue(L, output);
        } catch (const std::exception& e) {
            std::stringstream error;
            error << "Moon error: " << e.what();
            s_logger(error.str());
            lua_pushnil(L);
        }
        return 1;
    }

    template <int... indices>
    static int callHelper(Indices<indices...>, const std::function<void(Args...)>& func, lua_State* L) {
        try {
            func(std::move(Marshalling::GetValue<Args>(L, indices + 1))...);
        } catch (const std::exception& e) {
            std::stringstream error;
            error << "Moon error: " << e.what();
            s_logger(error.str());
        }
        return 0;
    }
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
        s_logger = [](const auto&) {};
        moon::Reference::Register(s_state);
        moon::LuaInvokable::Register(s_state);
        moon::LuaInvokable::s_logger = s_logger;
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
    static inline void SetLogger(const std::function<void(const std::string&)>& logger) {
        s_logger = logger;
        moon::LuaInvokable::s_logger = s_logger;
    }

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
     * @brief Checks if provided index is under the limits of Lua stack.
     *
     * @param index Index to check.
     * @return Whether or not the index is valid, inside the current stack elements.
     */
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

    /**
     * @brief Converts a negative index to a positive one. If already positive, returns it.
     *
     * @param index Index to convert.
     * @return Converted index.
     */
    static int ConvertNegativeIndex(int index) {
        if (index >= 0) {
            return index;
        }
        return GetTop() + index + 1;
    }

    /**
     * @brief Returns a string containing all values current in the stack. Useful for debug purposes.
     */
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

    /**
     * @brief String of element in stack value or type (if value can not be printed in C).
     *
     * @param index Index of element to get value or type.
     * @return String containing value of element or type.
     */
    static std::string StackElementToStringDump(int index) {
        if (!IsValidIndex(index)) {
            s_logger("Moon warning: Tried to print element at invalid index!");
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
    static moon::LuaType GetType(int index = 1) { return static_cast<moon::LuaType>(lua_type(s_state, index)); }

    /**
     * @brief Get value from Lua stack.
     *
     * @tparam R Value type.
     * @param index Stack index.
     * @return R
     */
    template <typename R>
    static inline R Get(const int index = 1) {
        try {
            return std::move(moon::Marshalling::GetValue<R>(s_state, index));
        } catch (const std::exception& e) {
            error(e.what());
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
    static inline R Get(const char* name) {
        lua_getglobal(s_state, name);
        const R r = Get<R>(GetTop());
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
    static void Push(T value) {
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
    static void Push(const char* name, T value) {
        Push(value);
        SetGlobalVariable(name);
    }

    /**
     * @brief Recursive helper method to push multiple values to Lua stack.
     *
     * @tparam Args Values types.
     * @param args Values to push to Lua stack.
     */
    template <typename... Args>
    static void PushValues(Args&&... args) {
        (Push(std::forward<Args>(args)), ...);
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
    static void Pop(int nrOfElements = 1) {
        while (nrOfElements > 0) {
            if (GetTop() <= 0) {
                error("Tried to pop stack but was empty already!");
                break;
            }
            lua_pop(s_state, 1);
            --nrOfElements;
        }
    }

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
     * @brief Registers and exposes C++ function to Lua.
     *
     * @tparam Func Function type.
     * @param name Function name to register in Lua.
     * @param func Function to register.
     */
    template <typename Func>
    static void RegisterFunction(const std::string& name, Func&& func) {
        auto deducedFunc = std::function{std::forward<Func>(func)};
        moon::Marshalling::PushValue(s_state, new moon::LuaInvokableCPP(deducedFunc), moon::LUA_INVOKABLE_HOLDER_META_NAME);
        SetGlobalVariable(name.c_str());
    }

    /**
     * @brief Calls Lua function (top of stack) without arguments.
     *
     * @return true Function successfully called.
     * @return false Unable to call function.
     */
    static bool Call() {
        if (!lua_isfunction(s_state, -1)) {
            error("Tried to call a non function value!");
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
    static bool Call(const char* name) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            error("Tried to call a non function value!");
            Pop();
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
    static bool Call(Args&&... args) {
        if (!lua_isfunction(s_state, -1)) {
            error("Tried to call a non function value!");
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
    static bool Call(const char* name, Args&&... args) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            Pop();
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
    static bool Call(T& lValue) {
        if (!lua_isfunction(s_state, -1)) {
            return false;
        }
        if (!checkStatus(lua_pcall(s_state, 0, 1, 0), "Failed to call LUA function")) {
            lValue = T{};
            return false;
        }
        lValue = Get<T>(GetTop());
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
    static bool Call(T& lValue, const char* name) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            Pop();
            return false;
        }
        if (!checkStatus(lua_pcall(s_state, 0, 1, 0), "Failed to call LUA function")) {
            return false;
        }
        lValue = Get<T>(GetTop());
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
    static bool Call(T& lValue, Args&&... args) {
        if (!lua_isfunction(s_state, -1)) {
            return false;
        }
        PushValues(std::forward<Args>(args)...);
        if (!checkStatus(lua_pcall(s_state, sizeof...(Args), 1, 0), "Failed to call LUA function")) {
            lValue = T{};
            return false;
        }
        lValue = Get<T>(GetTop());
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
    static bool Call(T& lValue, const char* name, Args&&... args) {
        lua_getglobal(s_state, name);
        if (!lua_isfunction(s_state, -1)) {
            Pop();
            return false;
        }
        PushValues(std::forward<Args>(args)...);
        if (!checkStatus(lua_pcall(s_state, sizeof...(Args), 1, 0), "Failed to call LUA function")) {
            return false;
        }
        lValue = Get<T>(GetTop());
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
        if (function.IsLoaded()) {
            function.Push();
            PushValues(std::forward<Args>(args)...);
            auto* state = function.GetState();
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
    static moon::BasicReference CreateRef(int index = -1) { return {s_state, index}; }

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

    /**
     * @brief Creates a new lua function from a C function and stores it as ref.
     *
     * @param functionRef C function to create Lua one.
     * @return LuaFunction
     */
    static moon::LuaFunction CreateFunction(moon::LuaCFunction functionRef) {
        Push(functionRef);
        moon::LuaFunction function(s_state, -1);
        Pop();
        return function;
    }

private:
    /**
     * @brief Lua state with static storage.
     */
    inline static lua_State* s_state{nullptr};

    /**
     * @brief Logger callback to be defined by client.
     */
    inline static std::function<void(const std::string&)> s_logger;

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
            const auto* msg = Get<const char*>(-1);
            Pop();
            std::stringstream error;
            error << "Lua error: " << (msg != nullptr ? msg : errMessage);
            s_logger(error.str());
            return false;
        }
        return true;
    }

    static void error(const std::string& message) {
        std::stringstream error;
        error << "Moon error: " << message;
        s_logger(error.str());
    }
};

#endif
