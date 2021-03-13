#pragma once

#include "lookupproxy.h"

namespace moon {
/// Any Lua object retrieved directly from stack and saved as reference.
class Object : public Reference {
public:
    static constexpr bool global = false;

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

    template <typename T>
    std::enable_if_t<!meta::is_moon_reference_v<T>, Object&> operator=(T&& other) {
        Unload();
        Core::Push(m_state, std::forward<T>(other));
        m_key = luaL_ref(m_state, LUA_REGISTRYINDEX);
        return *this;
    }

    inline bool operator==(const Object& other) const { return m_key == other.m_key && m_state == other.m_state; }

    inline bool operator!=(const Object& other) const { return !(*this == other); }

    /// Getter for the Lua state.
    /// \return Lua state pointer.
    [[nodiscard]] inline lua_State* GetState() const { return m_state; }

    /// Getter for the type of stored ref. Returns null if no reference was created.
    /// \return Type of ref stored.
    [[nodiscard]] LuaType GetType() const { return Reference::GetType(m_state); }

    /// Unloads reference from lua metatable and resets key.
    void Unload() { Reference::Unload(m_state); }

    /// Push value to stack. Returns number of values pushed.
    int Push() const { return Reference::Push(m_state); }

    /// Checks if Object can be converted to specified C type.
    /// \tparam T Type to check.
    /// \return Whether or not Object can be interpreted as specified C type.
    template <typename T>
    [[nodiscard]] bool Is() const {
        if (!IsLoaded()) {
            return false;
        }
        Push();
        Stack::PopGuard guard{m_state};
        return Stack::CheckValue<T>(m_state, -1);
    }

    /// Get object as specified C type.
    /// \tparam Ret Type to get.
    /// \return Object as C type.
    template <typename Ret>
    Ret As() const {
        if (!IsLoaded()) {
            return Core::DefaultReturnWithError<Ret>("tried to get value from an Object not loaded");
        }
        Push();
        Stack::PopGuard guard{m_state};
        return Core::Get<Ret>(m_state, -1);
    }

    /// Calls object if function reference.
    /// \tparam Rets Return(s) type(s).
    /// \tparam Args Arguments types.
    /// \param args Arguments to pass to function.
    /// \return Returned value(s) from Lua function.
    template <typename... Rets, typename... Args>
    decltype(auto) Call(Args&&... args) const {
        if (!IsLoaded()) {
            return Core::DefaultReturnWithError<Rets...>("tried to call an Object not loaded");
        }
        Push();
        return Core::Call<Rets...>(m_state, std::forward<Args>(args)...);
    }

    /// Tries to call object as void Lua function.
    /// \tparam Args Arguments types.
    /// \param args Arguments to pass to function.
    template <typename... Args>
    void operator()(Args&&... args) const {
        Call<void>(std::forward<Args>(args)...);
    }

    /// Implicit conversion is enabled to any C type (if possible)
    /// \tparam T Type to convert to.
    /// \return Object as C type.
    template <typename T>
    operator T() const {
        return As<T>();
    }

    /// Access properties when dealing with a table object.
    /// \tparam Key Key type to access.
    /// \param key Key to access in table.
    /// \return Lookup proxy to handle nested cases.
    template <typename Key>
    decltype(auto) operator[](Key&& key) const {
        return LookupProxy<Object, Key>{this, std::forward<Key>(key)};
    }

    /// Will save top stack element as ref and pop it, messing with stack.
    /// \param L Lua stack.
    /// \return Moon Object.
    static Object CreateAndPop(lua_State* L) { return Object(L, std::true_type{}); }

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

    /// Lua state.
    lua_State* m_state{nullptr};
};

/// Creates a lambda function (property `functor`) with specified template arguments signature that calls a moon::Object with same signature.
/// Basically a Lua function to C++ stl function converter.
/// \tparam Ret Return type of function.
/// \tparam Args Arguments type of function.
template <typename Ret, typename... Args>
class STLFunctionSpread<std::function<Ret(Args...)>> {
public:
    /// Returns functor that holds callable moon object with specified return and argument types.
    /// \param L Lua stack.
    /// \param index Index in Lua stack to get function from.
    /// \return STL function that will call moon object.
    static std::function<Ret(Args...)> GetFunctor(lua_State* L, int index) {
        Object obj(L, index);
        return [obj](Args&&... args) { return obj.Call<Ret>(std::forward<Args>(args)...); };
    }
};
}  // namespace moon
