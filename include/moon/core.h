#pragma once

#include "invokable.h"

namespace moon {
class Core {
public:
    /// Options enum for FieldHandler getter mode. When recursively getting nested props in table, those can be created if nonexistent or not.
    enum FieldMode { None = 0x00, Create = 0x01 };

    /// Handles table field get and set methods. Abstracts both global and nested tables.
    /// \tparam first Whether or not field to handle is global.
    /// \tparam Key Key type to push/set. Integral or string.
    template <bool first, typename Key>
    struct FieldHandler {};

    template <typename Key>
    struct FieldHandler<false, Key> {
        /// Pushes field to Lua stack. When create mode is active, creates a new table with provided key.
        /// \tparam mode Insertion mode. Whether or not to create new fields if null.
        /// \param L Lua state.
        /// \param index Index, in stack, of table to get field.
        /// \param key Key to search in table.
        /// \return Number of elements that need to be popped. If push field fails, no elements should be popped.
        template <FieldMode mode = FieldMode::None>
        int Get(lua_State* L, int index, Key&& key) {
            bool pop = Stack::PushField(L, index, std::forward<Key>(key));
            if constexpr (mode & FieldMode::Create) {
                if (lua_isnil(L, -1)) {
                    lua_pop(L, 1);
                    lua_newtable(L);
                    Stack::SetField(L, -2, std::forward<Key>(key));
                    Stack::PushField(L, -1, std::forward<Key>(key));
                }
            }
            return (int)pop;
        }

        /// Sets a new field in a Lua table. If fails to set, returns 1, since value that was previously pushed to set table must be popped.
        /// \param L Lua state.
        /// \param index Index of table to set field.
        /// \param key Key of field to set.
        /// \return Number of elements that must be popped from stack. 0 if success.
        int Set(lua_State* L, int index, Key&& key) { return Stack::SetField(L, index, std::forward<Key>(key)) ? 0 : 1; }
    };

    template <typename Key>
    struct FieldHandler<true, Key> {
        /// Pushes a new global to stack. If create mode is active, creates a new global, if null.
        /// \tparam mode Insertion mode. Whether or not to create new global if null.
        /// \param L Lua state.
        /// \param key Key to create global with. If integral, value at key index is pushed to top of stack.
        /// \return Number of elements to be popped from stack.
        template <FieldMode mode = FieldMode::None>
        int Get(lua_State* L, int, Key&& key) {
            if constexpr (meta::is_basic_string_v<Key>) {
                lua_getglobal(L, &key[0]);
                if constexpr (mode & FieldMode::Create) {
                    if (lua_isnil(L, -1)) {
                        lua_pop(L, 1);
                        lua_newtable(L);
                        lua_setglobal(L, &key[0]);
                        lua_getglobal(L, &key[0]);
                    }
                }
            } else if constexpr (meta::is_integral_v<Key>) {
                lua_pushvalue(L, std::forward<Key>(key));
            }
            return 1;
        }

        /// Sets a new global with the value at top of stack.
        /// \param L Lua state.
        /// \param key Key (name) to set global. Must be string.
        /// \return Number of elements to pop from stack. Always 0.
        int Set(lua_State* L, int, Key&& key) {
            static_assert(meta::is_basic_string_v<Key>, "setting a global directly by stack index is forbidden");
            lua_setglobal(L, &key[0]);
            return 0;
        }
    };

    template <bool global = true, typename... Keys>
    static LuaType GetType(lua_State* L, Keys&&... keys) {
        return type<global>(L, std::forward<Keys>(keys)...);
    }

    template <typename T, bool global = true, typename... Keys>
    static bool Check(lua_State* L, Keys&&... keys) {
        return check<global, T>(L, std::forward<Keys>(keys)...);
    }

    template <bool global = true, typename... Keys>
    static void Clean(lua_State* L, Keys&&... keys) {
        clean<global>(L, std::forward<Keys>(keys)...);
    }

    template <typename... Rets, typename... Keys>
    static decltype(auto) Get(lua_State* L, Keys&&... keys) {
        static_assert(sizeof...(Rets) == sizeof...(Keys), "number of returns and keys must match");
        return meta::multi_ret_t<std::decay_t<Rets>...>(getMaybeTuple<std::decay_t<Rets>>(L, std::forward<Keys>(keys))...);
    }

    template <typename Ret, bool global = true, typename... Keys>
    static decltype(auto) GetNested(lua_State* L, Keys&&... keys) {
        return get<global, std::decay_t<Ret>>(L, std::forward<Keys>(keys)...);
    }

    template <typename... Pairs>
    static void Set(lua_State* L, Pairs&&... pairs) {
        static_assert(sizeof...(Pairs) % 2 == 0, "pushing globals only works with name/value pairs");
        setPairs(std::make_index_sequence<sizeof...(Pairs) / 2>{}, L, std::forward_as_tuple(std::forward<Pairs>(pairs)...));
    }

    template <bool global = true, typename... Args>
    static void SetNested(lua_State* L, Args&&... args) {
        static_assert(sizeof...(Args) > 1, "at least one key and one value are necessary");
        set<global>(L, std::forward<Args>(args)...);
    }

    template <typename T>
    static void PushUserData(lua_State* L, T* value, const char* metatable) {
        auto** a = static_cast<T**>(lua_newuserdata(L, sizeof(T*)));  // Create userdata
        *a = value;
        luaL_getmetatable(L, metatable);
        lua_setmetatable(L, -2);
    }

    template <typename Func>
    static void PushFunction(lua_State* L, Func&& func) {
        auto deduced = std::function{std::forward<Func>(func)};
        PushUserData(L, new InvokableSTLFunction(deduced), LUA_INVOKABLE_HOLDER_META_NAME);
    }

    template <typename T>
    static void Push(lua_State* L, T&& value) {
        if constexpr (!meta::is_moon_reference_v<T> && meta::is_callable_v<T>) {
            PushFunction(L, std::forward<T>(value));
        } else {
            Stack::PushValue(L, std::forward<T>(value));
        }
    }

    template <bool first, typename Key, typename... Keys>
    static void PushField(lua_State* L, Key&& key, Keys&&... keys) {
        FieldHandler<first, Key>{}.template Get<FieldMode::None>(L, -1, std::forward<Key>(key));
        if constexpr (sizeof...(Keys) > 0) {
            PushField<false>(L, std::forward<Keys>(keys)...);
            lua_remove(L, -2);
        }
    }

    template <typename... Rets>
    static decltype(auto) DefaultReturnWithError(std::string&& message) {
        return Stack::DefaultReturnWithError<meta::multi_ret_t<Rets...>>(std::forward<std::string>(message));
    }

    template <typename... Rets, typename... Args>
    static meta::call_void_t<Rets...> Call(lua_State* L, Args&&... args) {
        (Push(L, std::forward<Args>(args)), ...);
        auto check = Stack::CallFunctionWithErrorCheck(L, meta::count_expected_v<Args...>, 0);
        if (check.has_value()) {
            return DefaultReturnWithError<Rets...>(std::forward<std::string>(check.value()));
        }
    }

    template <typename... Rets, typename... Args>
    static meta::call_return_t<Rets...> Call(lua_State* L, Args&&... args) {
        (Push(L, std::forward<Args>(args)), ...);
        auto check = Stack::CallFunctionWithErrorCheck(L, meta::count_expected_v<Args...>, meta::count_expected_v<Rets...>);
        if (check.has_value()) {
            return DefaultReturnWithError<Rets...>(std::forward<std::string>(check.value()));
        }
        Stack::PopGuard guard{L, meta::count_expected_v<Rets...>};
        return Stack::GetValue<meta::multi_ret_t<Rets...>>(L, -1);
    }

private:
    template <bool first, typename Key, typename... Keys>
    static decltype(auto) type(lua_State* L, Key&& key, Keys&&... keys) {
        Stack::PopGuard guard{L, FieldHandler<first, Key>{}.Get(L, -1, std::forward<Key>(key))};
        if constexpr (meta::sizeof_is_v<0, Keys...>) {
            return static_cast<LuaType>(lua_type(L, -1));
        } else {
            return type<false>(L, std::forward<Keys>(keys)...);
        }
    }

    template <bool first, typename T, typename Key, typename... Keys>
    static decltype(auto) check(lua_State* L, Key&& key, Keys&&... keys) {
        Stack::PopGuard guard{L, FieldHandler<first, Key>{}.Get(L, -1, std::forward<Key>(key))};
        if constexpr (meta::sizeof_is_v<0, Keys...>) {
            return Stack::CheckValue<T>(L, -1);
        } else {
            return check<false, T>(L, std::forward<Keys>(keys)...);
        }
    }

    template <bool first, typename Key, typename... Keys>
    static void clean(lua_State* L, Key&& key, Keys&&... keys) {
        if constexpr (meta::sizeof_is_v<0, Keys...>) {
            lua_pushnil(L);
            Stack::PopGuard guard{L, FieldHandler<first, Key>{}.Set(L, -2, std::forward<Key>(key))};
        } else {
            Stack::PopGuard guard{L, FieldHandler<first, Key>{}.Get(L, -1, std::forward<Key>(key))};
            clean<false>(L, std::forward<Keys>(keys)...);
        }
    }

    template <bool first, typename Ret, typename Key, typename... Keys>
    static decltype(auto) get(lua_State* L, Key&& key, Keys&&... keys)
        Stack::PopGuard guard{L, FieldHandler<first, Key>{}.Get(L, -1, std::forward<Key>(key))};
        if constexpr (meta::sizeof_is_v<0, Keys...>) {
            return Stack::GetValue<Ret>(L, -1);
        } else {
            return get<false, Ret>(L, std::forward<Keys>(keys)...);
        }
    }

    template <typename Ret, typename Key>
    static decltype(auto) getMaybeTuple(lua_State* L, Key&& key) {
        if constexpr (meta::is_tuple_v<Ret>) {
            return Stack::GetValue<Ret>(L, std::forward<Key>(key));
        } else {
            return get<true, Ret>(L, std::forward<Key>(key));
        }
    }

    template <bool first, typename Key, typename... Args>
    static void set(lua_State* L, Key&& key, Args&&... args) {
        if constexpr (meta::sizeof_is_v<1, Args...>) {
            Push(L, std::forward<Args>(args)...);
            Stack::PopGuard guard{L, FieldHandler<first, Key>{}.Set(L, -2, std::forward<Key>(key))};
        } else {
            Stack::PopGuard guard{L, FieldHandler<first, Key>{}.template Get<FieldMode::Create>(L, -1, std::forward<Key>(key))};
            set<false>(L, std::forward<Args>(args)...);
        }
    }

    template <size_t... indices, typename PairsTuple>
    static void setPairs(std::index_sequence<indices...>, lua_State* L, PairsTuple&& pairs) {
        (set<true>(L, std::get<indices * 2>(std::forward<PairsTuple>(pairs)), std::get<indices * 2 + 1>(std::forward<PairsTuple>(pairs))), ...);
    }
};
}  // namespace moon
