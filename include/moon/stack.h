#pragma once

#include "logger.h"
#include "reference.h"
#include "traits.h"
#include "usertype.h"

namespace moon {
template <typename T>
class STLFunctionSpread;

/// Abstracts interaction with Lua stack per C type. Gets and pushes values to/from Lua stack and validates types.
class Stack {
public:
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

    /// Reports error message and returns void.
    /// \tparam R Return type.
    /// \param message Message to report.
    /// \return Void.
    template <typename R>
    static meta::call_void_t<R> DefaultReturnWithError(std::string&& message) {
        Logger::Error(std::forward<std::string>(message));
    }

    /// Reports error message and returns default constructed return type.
    /// \tparam R Return type.
    /// \param message Message to report.
    /// \return Default constructed return type.
    template <typename R>
    static meta::call_return_t<R> DefaultReturnWithError(std::string&& message) {
        Logger::Error(std::forward<std::string>(message));
        return {};
    }

    template <typename R>
    static meta::is_bool_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isboolean(L, index);
    }

    template <typename R>
    static meta::is_integral_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isinteger(L, index);
    }

    template <typename R>
    static meta::is_floating_point_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isnumber(L, index) && !lua_isinteger(L, index);
    }

    template <typename R>
    static meta::is_string_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isstring(L, index);
    }

    template <typename R>
    static meta::is_c_string_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isstring(L, index);
    }

    template <typename R>
    static meta::is_pointer_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isuserdata(L, index);
    }

    template <typename R>
    static meta::is_binding_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isuserdata(L, index);
    }

    template <typename R>
    static meta::is_vector_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_istable(L, index);
    }

    template <typename R>
    static meta::is_map_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_istable(L, index);
    }

    template <typename R>
    static meta::is_function_t<R, bool> CheckValue(lua_State* L, int index) {
        return lua_isfunction(L, index);
    }

    template <typename R>
    static meta::is_bool_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: boolean");
        }
        return lua_toboolean(L, index) != 0;
    }

    template <typename R>
    static meta::is_integral_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: integer");
        }
        return static_cast<R>(lua_tointeger(L, index));
    }

    template <typename R>
    static meta::is_floating_point_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: number");
        }
        return static_cast<R>(lua_tonumber(L, index));
    }

    template <typename R>
    static meta::is_string_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: string");
        }
        size_t size;
        const char* buffer = lua_tolstring(L, index, &size);
        return R{buffer, size};
    }

    template <typename R>
    static meta::is_c_string_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: string");
        }
        return lua_tostring(L, index);
    }

    template <typename R>
    static meta::is_moon_reference_t<R> GetValue(lua_State* L, int index) {
        return {L, index};
    }

    template <typename R>
    static meta::is_pointer_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: userdata");
        }
        return *static_cast<R*>(lua_touserdata(L, index));
    }

    template <typename R>
    static meta::is_binding_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: userdata");
        }
        return *GetValue<R*>(L, index);
    }

    template <typename R>
    static meta::is_vector_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: table");
        }
        index = lua_absindex(L, index);
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
    static meta::is_map_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: table");
        }
        index = lua_absindex(L, index);
        lua_pushnil(L);
        R map;
        while (lua_next(L, index) != 0) {
            map.emplace(std::move(GetValue<std::string>(L, -2)), std::move(GetValue<typename R::mapped_type>(L, lua_gettop(L))));
            lua_pop(L, 1);
        }
        return map;
    }

    template <typename R>
    static meta::is_function_t<R> GetValue(lua_State* L, int index) {
        if (!CheckValue<R>(L, index)) {
            return DefaultReturnWithError<R>("type check failed: function");
        }
        return STLFunctionSpread<R>::GetFunctor(L, index);
    }

    template <typename R>
    static meta::is_tuple_t<R> GetValue(lua_State* L, int index) {
        index = lua_absindex(L, index);
        constexpr size_t elements = std::tuple_size_v<R>;
        // Ensure we have a proper starting index, even when dealing with function call args
        int starting = index > 1 ? index - elements + 1 : index;
        if (starting <= 0) {
            return DefaultReturnWithError<R>("invalid starting index when getting tuple");
        }
        return getTupleHelper<R>(std::make_index_sequence<elements>{}, L, starting);
    }

    template <typename T>
    static meta::is_bool_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_pushboolean(L, value);
    }

    template <typename T>
    static meta::is_integral_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_pushinteger(L, value);
    }

    template <typename T>
    static meta::is_floating_point_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_pushnumber(L, value);
    }

    template <typename T>
    static meta::is_string_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_pushstring(L, value.c_str());
    }

    template <typename T>
    static meta::is_c_string_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_pushstring(L, value);
    }

    template <typename T>
    static meta::is_vector_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_newtable(L);
        unsigned index = 1;
        for (auto element : value) {
            PushValue(L, std::forward<unsigned>(index));
            PushValue(L, std::forward<decltype(element)>(element));
            lua_settable(L, -3);
            ++index;
        }
    }

    template <typename T>
    static meta::is_map_t<T, void> PushValue(lua_State* L, T&& value) {
        lua_newtable(L);
        for (auto element : value) {
            PushValue(L, std::forward<decltype(element.first)>(element.first));
            PushValue(L, std::forward<decltype(element.second)>(element.second));
            lua_settable(L, -3);
        }
    }

    template <typename T>
    static meta::is_binding_t<T, void> PushValue(lua_State* L, T* value) {
        auto** a = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, T::Binding.GetName());
        lua_setmetatable(L, -2);
    }

    template <typename T>
    static meta::is_tuple_t<T, void> PushValue(lua_State* L, T&& value) {
        pushTupleHelper(std::make_index_sequence<std::tuple_size_v<T>>{}, L, std::forward<T>(value));
    }

    static void PushValue(lua_State* L, void* value) { lua_pushlightuserdata(L, value); }

    static void PushValue(lua_State* L, const Reference& value) { value.Push(L); }  // Needs to be const ref, for now, to handle Object

    template <typename Key>
    static meta::is_integral_t<Key, bool> PushField(lua_State* L, int index, Key&& key) {
        if (lua_isnil(L, index) || !lua_istable(L, index)) {
            return DefaultReturnWithError<bool>("tried to push field in a null table or not a table");
        }
        lua_rawgeti(L, index, key);
        return true;
    }

    template <typename Key>
    static meta::is_basic_string_t<Key, bool> PushField(lua_State* L, int index, Key&& key) {
        if (lua_isnil(L, index) || !lua_istable(L, index)) {
            return DefaultReturnWithError<bool>("tried to push field in a null table or not a table");
        }
        lua_getfield(L, index, &key[0]);
        return true;
    }

    template <typename Key>
    static meta::is_integral_t<Key, bool> SetField(lua_State* L, int index, Key&& key) {
        if (lua_isnil(L, index) || !lua_istable(L, index)) {
            return DefaultReturnWithError<bool>("tried to set field in a null table or not a table");
        }
        lua_rawseti(L, index, key);
        return true;
    }

    template <typename Key>
    static meta::is_basic_string_t<Key, bool> SetField(lua_State* L, int index, Key&& key) {
        if (lua_isnil(L, index) || !lua_istable(L, index)) {
            return DefaultReturnWithError<bool>("tried to set field in a null table or not a table");
        }
        lua_setfield(L, index, &key[0]);
        return true;
    }

    static std::optional<std::string> CallFunctionWithErrorCheck(lua_State* L, int numberArgs, int numberReturns) {
        return checkErrorStatus(L, lua_pcall(L, numberArgs, numberReturns, 0));
    }

private:
    /// Tuple index based expander helper method.
    /// \tparam T Tuple type.
    /// \tparam indices Each of the tuple indices.
    /// \param L Lua stack.
    /// \param index Index in stack of first element to add to tuple.
    /// \return Desired tuple.
    template <typename T, size_t... indices>
    static T getTupleHelper(std::index_sequence<indices...>, lua_State* L, int index) {
        return std::make_tuple(GetValue<std::tuple_element_t<indices, T>>(L, index + indices)...);
    }

    /// Tuple index based expander helper method.
    /// \tparam T Tuple type.
    /// \tparam indices Each of the tuple indices.
    /// \param L Lua stack.
    /// \param value Tuple to push.
    template <typename T, size_t... indices>
    static void pushTupleHelper(std::index_sequence<indices...>, lua_State* L, T&& value) {
        (PushValue(L, std::get<indices>(std::forward<T>(value))), ...);
    }

    /// Checks for status error and retrieves error message from stack.
    /// \param L Lua state.
    /// \param status Status to check.
    /// \param errMessage Default error message if none os found in stack.
    /// \return Either null optional (when no errors occur) or error string.
    static std::optional<std::string> checkErrorStatus(lua_State* L, int status, const char* errMessage = "") {
        if (status != LUA_OK) {
            const auto* msg = GetValue<const char*>(L, -1);
            lua_pop(L, 1);
            return std::optional<std::string>{msg != nullptr ? msg : errMessage};
        }
        return std::nullopt;
    }
};
}  // namespace moon
