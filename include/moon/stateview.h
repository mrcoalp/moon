#pragma once

#include "lookupproxy.h"

class Moon;

namespace moon {
/// Abstract view of global Lua state, with simple access to values.
class StateView {
public:
    static constexpr bool global = true;

    static StateView& Instance() {
        static StateView instance;
        return instance;
    }

    StateView(const StateView&) = delete;

    StateView(StateView&&) = delete;

    [[nodiscard]] int Push() const { return 0; }

    [[nodiscard]] lua_State* GetState() const { return m_state; }

    void operator=(const StateView&) = delete;

    void operator=(StateView&&) = delete;

    template <typename Key>
    auto operator[](Key&& key) const {
        return LookupProxy<StateView, Key>{this, std::forward<Key>(key)};
    }

private:
    StateView() = default;

    void initialize(lua_State* L) { m_state = L; }

    lua_State* m_state{nullptr};

    friend class ::Moon;
};
}  // namespace moon
