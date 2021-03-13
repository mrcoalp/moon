#pragma once

#include "stack.h"

namespace moon {
constexpr const char* LUA_INVOKABLE_HOLDER_META_NAME{"LuaInvokableHolder"};

class Invokable {
public:
    virtual ~Invokable() = default;

    virtual int Call(lua_State*) const = 0;

    static void Register(lua_State* L) {
        luaL_newmetatable(L, LUA_INVOKABLE_HOLDER_META_NAME);
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

private:
    static int call(lua_State* L) {
        void* storage = lua_touserdata(L, 1);
        auto* invokable = *static_cast<Invokable**>(storage);
        lua_remove(L, 1);
        return invokable->Call(L);
    }

    static int gc(lua_State* L) {
        void* storage = lua_touserdata(L, 1);
        auto* invokable = *static_cast<Invokable**>(storage);
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
        Stack::PushValue(L, func(std::forward<std::decay_t<Args>>(Stack::GetValue<std::decay_t<Args>>(L, indices + 1))...));
        return meta::count_expected_v<RetHelper>;
    }

    template <size_t... indices>
    static int callHelper(std::index_sequence<indices...>, const std::function<void(Args...)>& func, lua_State* L) {
        func(std::forward<std::decay_t<Args>>(Stack::GetValue<std::decay_t<Args>>(L, indices + 1))...);
        return 0;
    }
};
}  // namespace moon
