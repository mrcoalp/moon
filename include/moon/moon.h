#pragma once

#ifndef MOON_H
#define MOON_H

#include <algorithm>
#include <cstring>
#include <functional>
#include <lua.hpp>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>
#include <utility>
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

namespace moon {
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
    void Push(lua_State* L) const {
        if (!IsLoaded()) {
            if (L != nullptr) {
                lua_pushnil(L);
            }
            return;
        }
        lua_rawgeti(L, LUA_REGISTRYINDEX, m_key);
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

template <typename T, typename Ret = T>
using IsBool = std::enable_if_t<std::is_same_v<T, bool>, Ret>;
template <typename T, typename Ret = T>
using IsIntegral = std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, Ret>;
template <typename T, typename Ret = T>
using IsFloatingPoint = std::enable_if_t<std::is_floating_point_v<T>, Ret>;
template <typename T, typename Ret = T>
using IsString = std::enable_if_t<std::is_same_v<T, std::string>, Ret>;
template <typename T, typename Ret = T>
using IsCString = std::enable_if_t<std::is_same_v<T, const char*>, Ret>;
template <typename T, typename Ret = T>
using IsLuaRef = std::enable_if_t<std::is_base_of_v<Reference, T>, Ret>;
template <typename T, typename Ret = T>
using IsPointer = std::enable_if_t<std::is_pointer_v<T> && !std::is_same_v<T, const char*>, Ret>;
template <typename T>
struct Vector : std::false_type {};
template <typename T>
struct Vector<std::vector<T>> : std::true_type {};
template <typename T, typename Ret = T>
using IsVector = std::enable_if_t<Vector<T>::value, Ret>;
template <typename T>
struct Map : std::false_type {};
template <typename T>
struct Map<LuaMap<T>> : std::true_type {};
template <typename T>
struct Map<std::map<std::string, T>> : std::true_type {};
template <typename T, typename Ret = T>
using IsMap = std::enable_if_t<Map<T>::value, Ret>;
template <typename T, typename Ret = T>
using IsBinding = std::enable_if_t<std::is_same_v<decltype(T::Binding), Binding<T>>, Ret>;
template <typename T>
struct Function : std::false_type {};
template <typename T>
struct Function<std::function<T>> : std::true_type {};
template <typename T, typename Ret = T>
using IsFunction = std::enable_if_t<Function<T>::value, Ret>;
template <typename>
struct Tuple : std::false_type {};
template <typename... T>
struct Tuple<std::tuple<T...>> : std::true_type {};
template <typename T, typename Ret = T>
using IsTuple = std::enable_if_t<Tuple<T>::value, Ret>;

/// Internal static helper methods.
class Helpers {
public:
    /// Lua function argument count per C++ args pack
    /// \tparam Args C++ arguments types.
    /// \return Number of arguments to call Lua function.
    template <typename... Args>
    static size_t ArgsCount() {
        return sum(ResultsCount<Args>()...);
    }

    /// Lua function return results count per C++ type.
    /// Count when using tuple is the same of tuple size.
    /// \tparam T C++ type to check.
    /// \return Number of return results of Lua function.
    template <typename T>
    static std::enable_if_t<Tuple<T>::value, size_t> ResultsCount() {
        return std::tuple_size_v<T>;
    }

    /// Lua function return results count per C++ type.
    /// Count when using other types than tuple is 1.
    /// \tparam T C++ type to check.
    /// \return Number of return results of Lua function.
    template <typename T>
    static std::enable_if_t<!Tuple<T>::value, size_t> ResultsCount() {
        return 1;
    }

private:
    /// Sum of all arguments. Useful for number of arguments count per type.
    /// \return Sum of all arguments.
    static size_t sum() { return 0; }

    /// Sum of all arguments. Useful for number of arguments count per type.
    /// \tparam T Argument type
    /// \param first Element to sum.
    /// \return Sum of all arguments.
    template <typename T>
    static size_t sum(T&& first) {
        return first;
    }

    /// Sum of all arguments. Useful for number of arguments count per type.
    /// \tparam T Argument type.
    /// \tparam Args Rest of arguments types.
    /// \param first Element to sum.
    /// \param rest Rest of elements to sum.
    /// \return Sum of all arguments.
    template <typename T, typename... Args>
    static size_t sum(T&& first, Args&&... rest) {
        return first + sum(rest...);
    }
};

template <typename T>
struct STLFunctionSpread;

template <typename T>
struct STLTupleSpread;

/// Abstracts interaction with Lua stack per C type. Gets and pushes values to/from Lua stack and validates types.
class Marshalling {
public:
#define assert_lua_type(_check, _type)            \
    if (!_check) {                                \
        std::stringstream ss;                     \
        ss << "Lua type check failed: " << _type; \
        throw std::runtime_error(ss.str());       \
    }

    template <typename R>
    static IsBool<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isboolean(L, index);
    }

    template <typename R>
    static IsIntegral<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isinteger(L, index);
    }

    template <typename R>
    static IsFloatingPoint<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isnumber(L, index) && !lua_isinteger(L, index);
    }

    template <typename R>
    static IsString<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isstring(L, index);
    }

    template <typename R>
    static IsCString<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isstring(L, index);
    }

    template <typename R>
    static IsPointer<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isuserdata(L, index);
    }

    template <typename R>
    static IsBinding<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isuserdata(L, index);
    }

    template <typename R>
    static IsVector<R, bool> CheckValue(lua_State* L, int index) {
        return lua_istable(L, index);
    }

    template <typename R>
    static IsMap<R, bool> CheckValue(lua_State* L, int index) {
        return lua_istable(L, index);
    }

    template <typename R>
    static IsFunction<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isfunction(L, index);
    }

    template <typename R>
    static IsBool<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "boolean");
        return lua_toboolean(L, index) != 0;
    }

    template <typename R>
    static IsIntegral<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "integer");
        return static_cast<R>(lua_tointeger(L, index));
    }

    template <typename R>
    static IsFloatingPoint<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "number");
        return static_cast<R>(lua_tonumber(L, index));
    }

    template <typename R>
    static IsString<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "string");
        size_t size;
        const char* buffer = lua_tolstring(L, index, &size);
        return R{buffer, size};
    }

    template <typename R>
    static IsCString<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "string");
        return lua_tostring(L, index);
    }

    template <typename R>
    static IsLuaRef<R> GetValue(lua_State* L, int index) {
        return {L, index};
    }

    template <typename R>
    static IsPointer<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "userdata");
        return *static_cast<R*>(lua_touserdata(L, index));
    }

    template <typename R>
    static IsBinding<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "userdata");
        return *GetValue<R*>(L, index);
    }

    template <typename R>
    static IsVector<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "table");
        ensurePositiveIndex(index, L);
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
        return std::move(vec);
    }

    template <typename R>
    static IsMap<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "table");
        ensurePositiveIndex(index, L);
        lua_pushnil(L);
        R map;
        while (lua_next(L, index) != 0) {
            map.emplace(std::move(GetValue<std::string>(L, -2)), std::move(GetValue<typename R::mapped_type>(L, lua_gettop(L))));
            lua_pop(L, 1);
        }
        return std::move(map);
    }

    template <typename R>
    static IsFunction<R> GetValue(lua_State* L, int index) {
        assert_lua_type(CheckValue<R>(L, index), "function");
        return STLFunctionSpread<R>::GetFunctor(L, index);
    }

    template <typename R>
    static IsTuple<R> GetValue(lua_State* L, int index) {
        ensurePositiveIndex(index, L);
        return STLTupleSpread<R>::GetTuple(L, index);
    }

    template <typename T>
    static IsBool<T, void> PushValue(lua_State* L, T value) {
        lua_pushboolean(L, value);
    }

    template <typename T>
    static IsIntegral<T, void> PushValue(lua_State* L, T value) {
        lua_pushinteger(L, value);
    }

    template <typename T>
    static IsFloatingPoint<T, void> PushValue(lua_State* L, T value) {
        lua_pushnumber(L, value);
    }

    static void PushValue(lua_State* L, const std::string& value) { lua_pushstring(L, value.c_str()); }

    static void PushValue(lua_State* L, const char* value) { lua_pushstring(L, value); }

    static void PushValue(lua_State* L, void* value) { lua_pushlightuserdata(L, value); }

    static void PushValue(lua_State* L, LuaCFunction value) { lua_pushcfunction(L, value); }

    static void PushValue(lua_State* L, const Reference& value) { value.Push(L); }

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
    static void PushValue(lua_State* L, const LuaMap<T>& value) {
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

    template <typename T>
    static IsBinding<T, void> PushValue(lua_State* L, T* value) {
        auto** a = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, T::Binding.GetName());
        lua_setmetatable(L, -2);
    }

    template <typename T>
    static IsTuple<T, void> PushValue(lua_State* L, T value) {
        STLTupleSpread<T>::PushTuple(L, value);
    }

    template <typename T>
    static void PushValue(lua_State* L, T* value, const char* name) {
        auto** a = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, name);
        lua_setmetatable(L, -2);
    }

    template <typename... Args>
    static void PushValues(lua_State* L, Args&&... args) {
        (PushValue(L, std::forward<Args>(args)), ...);
    }

    static std::optional<std::string> GetError(lua_State* L, int status, const char* errMessage = "") {
        if (status != LUA_OK) {
            const auto* msg = GetValue<const char*>(L, -1);
            lua_pop(L, 1);
            return std::optional<std::string>{msg != nullptr ? msg : errMessage};
        }
        return std::nullopt;
    }

private:
    /// When using more complex data containers (like maps or vectors) recursion inside this can not be made with negative indexes.
    /// If we try to get, for example, a std::vector and provide -1 as index, in the recursive call to get the value, we would
    /// always access the last value in the stack, which is not the right element (since we push indexes of the vector). We must, in this case,
    /// convert to the real index in the stack, by the right order.
    /// \param index Index to check and convert if negative.
    /// \param L Lua stack.
    static void ensurePositiveIndex(int& index, lua_State* L) {
        if (index < 0) {
            index = lua_gettop(L) + index + 1;
        }
    }
};

/// Pops specified elements from lua stack when destroyed.
class PopGuard {
public:
    /// Pops specified elements from lua stack when destroyed - ctor.
    /// \param L Lua state to pop from.
    /// \param elements Number of elements to pop from stack. Defaults to 1.
    explicit PopGuard(lua_State* L, int elements = 1) : m_state(L), m_elements(elements) {}

    /// Pops specified elements from lua stack when destroyed - dtor.
    ~PopGuard() { lua_pop(m_state, m_elements); }

private:
    /// Lua state pointer.
    lua_State* m_state{nullptr};
    /// Number of elements to pop.
    int m_elements{1};
};

/// Any Lua object retrieved directly from stack and saved as reference.
class Object : public Reference {
public:
    Object() = default;

    explicit Object(lua_State* L) : m_state(L), Reference(L, -1) {}

    Object(lua_State* L, int index) : m_state(L), Reference(L, index) {}

    Object(const Object& other) : m_state(other.m_state), Reference(other.copy()) {}

    Object(Object&& other) noexcept : m_state(other.m_state), Reference(std::move(other)) { other.m_state = nullptr; }

    ~Object() {
        if (m_state == nullptr) {
            return;
        }
        Unload();
    }

    Object& operator=(const Object& other) noexcept {
        if (&other == this) {
            return *this;
        }
        m_key = other.copy();
        m_state = other.m_state;
        return *this;
    }

    Object& operator=(Object&& other) noexcept {
        m_key = other.m_key;
        m_state = other.m_state;
        other.m_key = LUA_NOREF;
        other.m_state = nullptr;
        return *this;
    }

    inline bool operator==(const Object& other) const { return m_key == other.m_key && m_state == other.m_state; }

    inline bool operator!=(const Object& other) const { return !(*this == other); }

    /// Error reporter setter.
    /// \param reporter Reporter to use.
    static inline void SetErrorReporter(const std::function<void(const std::string&)>& reporter) { s_reportError = reporter; }

    /// Getter for the Lua state.
    /// \return Lua state pointer.
    [[nodiscard]] inline lua_State* GetState() const { return m_state; }

    /// Getter for the type of stored ref. Returns null if no reference was created.
    /// \return Type of ref stored.
    [[nodiscard]] LuaType GetType() const { return Reference::GetType(m_state); }

    /// Unloads reference from lua metatable and resets key.
    void Unload() { Reference::Unload(m_state); }

    /// Push value to stack.
    void Push() const { Reference::Push(m_state); }

    /// Checks if Object can be converted to specified C type.
    /// \tparam T Type to check.
    /// \return Whether or not Object can be interpreted as specified C type.
    template <typename T>
    [[nodiscard]] bool Is() const {
        if (!IsLoaded()) {
            return false;
        }
        Push();
        PopGuard guard{m_state};
        return Marshalling::CheckValue<T>(m_state, -1);
    }

    /// Get object as specified C type.
    /// \tparam Ret Type to get.
    /// \return Object as C type.
    template <typename Ret>
    Ret As() const {
        if (!IsLoaded()) {
            validate("Tried to get value from an Object not loaded");
            return {};
        }
        Push();
        PopGuard guard{m_state};
        try {
            return std::move(Marshalling::GetValue<Ret>(m_state, -1));
        } catch (const std::exception& e) {
            validate(e.what());
        }
        return {};
    }

    /// Try to call object as Lua function.
    /// \tparam Ret Void return specialization.
    /// \tparam Args Arguments types.
    /// \param args Arguments to call function with.
    /// \return Void.
    template <typename Ret, typename... Args>
    std::enable_if_t<std::is_void_v<Ret>, void> Call(Args&&... args) const {
        if (!IsLoaded()) {
            validate("Tried to call an Object not loaded");
            return;
        }
        Push();
        Marshalling::PushValues(m_state, std::forward<Args>(args)...);
        validate(Marshalling::GetError(m_state, lua_pcall(m_state, Helpers::ArgsCount<Args...>(), 0, 0)));
    }

    /// Try to call object as Lua function.
    /// \tparam Ret Return type of function.
    /// \tparam Args Arguments types.
    /// \param args Arguments to pass to Lua function.
    /// \return Return value of Lua function.
    template <typename Ret, typename... Args>
    std::enable_if_t<!std::is_void_v<Ret>, Ret> Call(Args&&... args) const {
        if (!IsLoaded()) {
            validate("Tried to call an Object not loaded");
            return {};
        }
        Push();
        Marshalling::PushValues(m_state, std::forward<Args>(args)...);
        if (!validate(Marshalling::GetError(m_state, lua_pcall(m_state, Helpers::ArgsCount<Args...>(), Helpers::ResultsCount<Ret>(), 0)))) {
            return {};
        }
        PopGuard guard{m_state, (int)Helpers::ResultsCount<Ret>()};
        try {
            return std::move(Marshalling::GetValue<Ret>(m_state, -1));
        } catch (const std::exception& e) {
            validate(e.what());
        }
        return {};
    }

    /// Tries to call object as void Lua function.
    /// \tparam Args Arguments types.
    /// \param args Arguments to pass to function.
    template <typename... Args>
    void operator()(Args&&... args) const {
        Call<void>(std::forward<Args>(args)...);
    }

    explicit operator bool() { return IsLoaded(); }

    explicit operator bool() const { return IsLoaded(); }

    /// Will save top stack element as ref and pop it, messing with stack.
    /// \param L Lua stack.
    /// \return Moon Object.
    static Object CreateAndPop(lua_State* L) { return std::move(Object(L, std::true_type{})); }

private:
    /// Will save top stack element as ref and pop it, messing with stack. To still leave a copy in stack, explicit specify element index.
    Object(lua_State* L, std::true_type) : m_state(L), Reference(luaL_ref(L, LUA_REGISTRYINDEX)) {}

    /// Copies reference in lua ref holder table.
    /// \return Key id of newly created copy.
    [[nodiscard]] int copy() const {
        if (!IsLoaded()) {
            return LUA_NOREF;
        }
        Push();
        return luaL_ref(m_state, LUA_REGISTRYINDEX);
    }

    /// When optional message is not empty, reports error. To be used with Marshalling error reporting.
    /// \param message Optional error message. When empty, we assumed there were no errors.
    /// \return Whether or not errors occurred.
    static bool validate(const std::optional<std::string>& message) {
        if (message.has_value()) {
            if (s_reportError) {
                s_reportError(message.value());
            }
            return false;
        }
        return true;
    }

    /// Error reporter function. To be defined by wrapper.
    inline static std::function<void(const std::string&)> s_reportError;
    /// Lua state.
    lua_State* m_state{nullptr};
};

/// Creates a lambda function (property `functor`) with specified template arguments signature that calls a moon::Object with same signature.
/// Basically a Lua function to C++ stl function converter.
/// \tparam Ret Return type of function.
/// \tparam Args Arguments type of function.
template <typename Ret, typename... Args>
struct STLFunctionSpread<std::function<Ret(Args...)>> {
    /// Returns functor that holds callable moon object with specified return and argument types.
    /// \param L Lua stack.
    /// \param index Index in Lua stack to get function from.
    /// \return STL function that will call moon object.
    static std::function<Ret(Args...)> GetFunctor(lua_State* L, int index) {
        Object obj(L, index);
        return std::move([obj](Args... args) { return obj.Call<Ret>(std::forward<Args>(args)...); });
    }
};

/// Helper to get and push tuples to Lua stack.
/// \tparam T Tuple type to operate with.
template <typename T>
struct STLTupleSpread {
public:
    /// Getter for specified tuple type from Lua stack.
    /// \param L Lua stack.
    /// \param index Index to end. Last index to get element from.
    /// \return Desired tuple.
    static T GetTuple(lua_State* L, int index) {
        constexpr size_t elements = std::tuple_size_v<T>;
        return getHelper(std::make_index_sequence<elements>{}, L, index - elements + 1);
    }

    /// Pushes tuple to Lua stack.
    /// \param L Lua stack.
    /// \param value Tuple to push.
    static void PushTuple(lua_State* L, T value) { pushHelper(std::make_index_sequence<std::tuple_size_v<T>>{}, L, value); }

private:
    /// Tuple index based expander helper method.
    /// \tparam indices Each of the tuple indices.
    /// \param L Lua stack.
    /// \param index Index in stack of first element to add to tuple.
    /// \return Desired tuple.
    template <size_t... indices>
    static T getHelper(std::index_sequence<indices...>, lua_State* L, int index) {
        return std::make_tuple(Marshalling::GetValue<std::tuple_element_t<indices, T>>(L, index + indices)...);
    }

    /// Tuple index based expander helper method.
    /// \tparam indices Each of the tuple indices.
    /// \param L Lua stack.
    /// \param value Tuple to push.
    template <size_t... indices>
    static void pushHelper(std::index_sequence<indices...>, lua_State* L, T value) {
        (Marshalling::PushValue(L, std::get<indices>(value)), ...);
    }
};

class Invokable {
public:
    virtual ~Invokable() = default;

    virtual int Call(lua_State*) const = 0;

    static void Register(lua_State* L) {
        luaL_newmetatable(L, moon::LUA_INVOKABLE_HOLDER_META_NAME);
        int metatable = lua_gettop(L);

        lua_pushstring(L, "__call");
        lua_pushcfunction(L, &Invokable::call);
        lua_settable(L, metatable);

        lua_pushstring(L, "__gc");
        lua_pushcfunction(L, &Invokable::gc);
        lua_settable(L, metatable);

        lua_pushstring(L, "__metatable");
        lua_pushstring(L, "Access restricted");
        lua_settable(L, metatable);

        lua_pop(L, 1);
    }

    static inline void SetErrorReporter(const std::function<void(const std::string&)>& reporter) { s_reportError = reporter; }

protected:
    static inline std::function<void(const std::string&)> s_reportError;

private:
    static int call(lua_State* L) {
        void* storage = lua_touserdata(L, 1);
        auto* invokable = *static_cast<moon::Invokable**>(storage);
        lua_remove(L, 1);
        return invokable->Call(L);
    }

    static int gc(lua_State* L) {
        void* storage = lua_touserdata(L, 1);
        auto* invokable = *static_cast<moon::Invokable**>(storage);
        delete invokable;
        return 0;
    }
};

template <typename Ret, typename... Args>
class InvokableSTLFunction : public Invokable {
public:
    explicit InvokableSTLFunction(std::function<Ret(Args...)> func_) : func(std::move(func_)) {}

    inline int Call(lua_State* L) const final { return callHelper(std::make_index_sequence<sizeof...(Args)>{}, func, L); }

private:
    std::function<Ret(Args...)> func;

    template <size_t... indices, typename RetHelper>
    static int callHelper(std::index_sequence<indices...>, const std::function<RetHelper(Args...)>& func, lua_State* L) {
        try {
            auto output = func(std::move(Marshalling::GetValue<Args>(L, indices + 1))...);
            Marshalling::PushValue(L, output);
        } catch (const std::exception& e) {
            if (s_reportError) {
                s_reportError(e.what());
            }
            lua_pushnil(L);
        }
        return 1;
    }

    template <size_t... indices>
    static int callHelper(std::index_sequence<indices...>, const std::function<void(Args...)>& func, lua_State* L) {
        try {
            func(std::move(Marshalling::GetValue<Args>(L, indices + 1))...);
        } catch (const std::exception& e) {
            if (s_reportError) {
                s_reportError(e.what());
            }
        }
        return 0;
    }
};
}  // namespace moon

/// Handles all the logic related to "communication" between C++ and Lua, initializing it.
/// Acts as an engine to allow to call C++ functions from Lua and vice-versa, registers
/// and exposes C++ classes to Lua, pops and pushes values to Lua stack, runs file scripts and
/// string scripts.
class Moon {
public:
    /// Initializes Lua state and opens libs.
    static void Init() {
        s_state = luaL_newstate();
        luaL_openlibs(s_state);
        s_logger = [](const auto&) {};
        s_error.clear();
        moon::Object::SetErrorReporter(error);
        moon::Invokable::Register(s_state);
        moon::Invokable::SetErrorReporter(error);
    }

    /// Closes Lua state.
    static void CloseState() {
        lua_close(s_state);
        s_state = nullptr;
    }

    /// Sets a new Lua state.
    /// \param state Lua state to set.
    static inline void SetState(lua_State* state) { s_state = state; }

    /// Set a logger callback.
    /// \param logger Callback which is gonna be called every time a log occurs.
    static inline void SetLogger(const std::function<void(const std::string&)>& logger) { s_logger = logger; }

    /// Getter for Lua state/stack.
    /// \return Lua state pointer.
    static inline lua_State* GetState() { return s_state; }

    /// Checks if an error is stored. Every time an error occurs it is stored until manual removal.
    /// \return Whether or not error string is empty.
    static bool HasError() { return !s_error.empty(); }

    /// Getter for the current stored error message.
    /// \return Error message.
    static const std::string& GetErrorMessage() { return s_error; }

    /// Clears error message and state.
    static void ClearError() { s_error.clear(); }

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
    static int ConvertNegativeIndex(int index) {
        if (index >= 0) {
            return index;
        }
        return GetTop() + index + 1;
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

    /// Prints element at specified index. Shows value when possible or type otherwise.
    /// \param index Index in stack to print.
    /// \return String log of element.
    static std::string StackElementToStringDump(int index) {
        if (!IsValidIndex(index)) {
            warning("Tried to print element at invalid index");
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

    /// Logs current stack to logger.
    static inline void LogStackDump() { s_logger(GetStackDump()); }

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

    /// Get moon type of element at specified index in Lua stack.
    /// \param index Index to check type.
    /// \return Moon type of element.
    static moon::LuaType GetType(int index = 1) { return static_cast<moon::LuaType>(lua_type(s_state, index)); }

    /// Gets element at specified Lua stack index as C object.
    /// \tparam R C type to cast Lua object to.
    /// \param index Index of element in stack.
    /// \return C object.
    template <typename R>
    static inline R Get(const int index = 1) {
        try {
            return std::move(moon::Marshalling::GetValue<R>(s_state, index));
        } catch (const std::exception& e) {
            error(e.what());
        }
        return {};
    }

    /// Gets global Lua variable with specified name as C object.
    /// \tparam R C object type.
    /// \param name Lua variable name.
    /// \return C object.
    template <typename R>
    static inline R Get(const char* name) {
        lua_getglobal(s_state, name);
        const R r = Get<R>(GetTop());
        lua_pop(s_state, 1);
        return r;
    }

    /// Ensures that a given LuaMap contains all desired keys. Useful, since maps obtained from lua are dynamic.
    /// \tparam T LuaMap type.
    /// \param keys Keys to check in map.
    /// \param map Map to check.
    /// \return True if map contains all keys, false otherwise.
    template <typename T>
    static bool EnsureMapKeys(const std::vector<std::string>& keys, const moon::LuaMap<T>& map) {
        return std::all_of(keys.cbegin(), keys.cend(), [&map](const std::string& key) { return map.find(key) != map.cend(); });
    }

    /// Push value to Lua stack.
    /// \tparam T Value type.
    /// \param value Value pushed to Lua stack.
    template <typename T>
    static void Push(T value) {
        moon::Marshalling::PushValue(s_state, value);
    }

    /// Push global variable to Lua stack.
    /// \tparam T Value type.
    /// \param name Variable name.
    /// \param value Value pushed to Lua stack.
    template <typename T>
    static void Push(const char* name, T value) {
        Push(value);
        SetGlobalVariable(name);
    }

    /// Recursive helper method to push multiple values to Lua stack.
    /// \tparam Args Values types.
    /// \param args Values to push to Lua stack.
    template <typename... Args>
    static void PushValues(Args&&... args) {
        (Push(std::forward<Args>(args)), ...);
    }

    /// Push a nil (null) value to Lua stack.
    static void PushNull() { lua_pushnil(s_state); }

    /// Pushes a new empty table/map to stack.
    static void PushTable() { lua_newtable(s_state); }

    /// Tries to pop specified number of elements from stack. Logs an error if top is reached not popping any more.
    /// \param nrOfElements Number of elements to pop from Lua stack.
    static void Pop(int nrOfElements = 1) {
        while (nrOfElements > 0) {
            if (GetTop() <= 0) {
                warning("Tried to pop stack but was empty already");
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
    static void RegisterClass(const char* nameSpace = nullptr) {
        moon::LuaClass<T>::Register(s_state, nameSpace);
    }

    /// Registers and exposes C++ function to Lua.
    /// \tparam Func Function type.
    /// \param name Function name to register in Lua.
    /// \param func Function to register.
    template <typename Func>
    static void RegisterFunction(const std::string& name, Func&& func) {
        auto deducedFunc = std::function{std::forward<Func>(func)};
        moon::Marshalling::PushValue(s_state, new moon::InvokableSTLFunction(deducedFunc), moon::LUA_INVOKABLE_HOLDER_META_NAME);
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

    /// Sets top of stack as a global variable.
    /// \param name Name of the variable to set.
    static void SetGlobalVariable(const char* name) { lua_setglobal(s_state, name); }

    /// Cleans/nulls a global variable.
    /// \param name Variable to clean.
    static void CleanGlobalVariable(const char* name) {
        PushNull();
        SetGlobalVariable(name);
    }

    /// Make moon object directly in Lua stack from C object. Stack is immediately popped, since the reference is stored.
    /// \tparam T C object type.
    /// \param value C object.
    /// \return Newly created moon object.
    template <typename T>
    static moon::Object MakeObject(T&& value) {
        Push(value);
        return std::move(moon::Object::CreateAndPop(s_state));  // Maybe a superfluous move... Can't be bothered.
    }

    /// Creates and stores a new ref of element at provided index.
    /// \param index Index of element to create ref. Defaults to top of stack.
    /// \return A new moon Object.
    static moon::Object MakeObjectFromIndex(int index = -1) { return {s_state, index}; }

private:
    /// Checks for lua status and returns if ok or not.
    /// \param status Status code obtained from lua function.
    /// \param errMessage Default error message to print if none is obtained from lua stack.
    /// \return Whether or not no error occurred.
    static bool checkStatus(int status, const char* errMessage = "") {
        if (status != LUA_OK) {
            const auto* msg = Get<const char*>(-1);
            Pop();
            error(msg != nullptr ? msg : errMessage);
            return false;
        }
        return true;
    }

    /// Logs a warning message.
    static void warning(const std::string& message) { s_logger("Moon :: WARNING: " + message); }

    /// Logs and stores an error message.
    static void error(const std::string& message) {
        s_error = message;
        s_logger("Moon :: ERROR: " + message);
    }

    /// Lua state with static storage.
    inline static lua_State* s_state{nullptr};
    /// Logger callback to be defined by client.
    inline static std::function<void(const std::string&)> s_logger;
    /// Global error message. This is filled every time an error occurs, whether directly or by reporters. Must be manually removed.
    inline static std::string s_error;
};

#endif
