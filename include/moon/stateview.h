#pragma once

#include "lookupproxy.h"

class Moon;

namespace moon {
/// Abstract view of global Lua state, with simple access to values.
class StateView {
public:
    /// Compile time boolean that defines this class, used in lookup proxy, to be interpreted as pushing only global Lua variables.
    static constexpr bool global = true;

    /// Singleton instance getter.
    /// \return StateView singleton.
    static StateView& Instance() {
        static StateView instance;
        return instance;
    }

    StateView(const StateView&) = delete;

    StateView(StateView&&) = delete;

    /// Pushing this to Lua stack has no effect and no value should be popped, hence the return 0. Used to generalize proxy and define globals.
    /// \return Number of elements to be popped from Lua stack.
    [[nodiscard]] int Push() const { return 0; }

    /// Getter for Lua state.
    /// \return Lua state.
    [[nodiscard]] lua_State* GetState() const { return m_state; }

    void operator=(const StateView&) = delete;

    void operator=(StateView&&) = delete;

    /// Access nested values in Lua global table.
    /// \tparam Key Key type, either string or integral.
    /// \param key Key to lookup.
    /// \return Lookup proxy class.
    template <typename Key>
    auto operator[](Key&& key) const {
        return LookupProxy<StateView, Key>{this, std::forward<Key>(key)};
    }

private:
    /// ctor
    StateView() = default;

    /// Setter for Lua state pointer.
    /// \param L Lua state.
    void initialize(lua_State* L) { m_state = L; }

    /// Lua state pointer.
    lua_State* m_state{nullptr};

    /// Moon class should be the only capable of setting the Lua state pointer.
    friend class ::Moon;
};
}  // namespace moon
