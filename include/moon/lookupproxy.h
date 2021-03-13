#pragma once

#include "core.h"
#include "traits.h"

namespace moon {
/// Proxy to be used as descriptor to nested values in tables, both for globals or objects. Provides simple stl like access to properties.
/// \tparam Lookup Table to lookup. Can be global or referenced in object.
/// \tparam Key Key to add as descriptor. Will be converted to tuple, to support multiple.
template <typename Lookup, typename Key>
class LookupProxy {
    /// Forced tuple of Key.
    using proxy_key_t = meta::convert_to_tuple_t<Key>;

public:
    LookupProxy(const Lookup* table, Key&& key) : m_table(table), m_key(std::forward<Key>(key)) {}

    template <typename R>
    inline decltype(auto) Get() const {
        return get<R>(std::make_index_sequence<std::tuple_size_v<proxy_key_t>>{});
    }

    template <typename T>
    inline void Set(T&& value) const {
        set(std::make_index_sequence<std::tuple_size_v<proxy_key_t>>{}, std::forward<T>(value));
    }

    [[nodiscard]] inline decltype(auto) GetType() const { return type(std::make_index_sequence<std::tuple_size_v<proxy_key_t>>{}); }

    template <typename T>
    [[nodiscard]] inline bool Check() const {
        return check<T>(std::make_index_sequence<std::tuple_size_v<proxy_key_t>>{});
    }

    inline void Clean() const { clean(std::make_index_sequence<std::tuple_size_v<proxy_key_t>>{}); }

    template <typename... Rets, typename... Args>
    decltype(auto) Call(Args&&... args) const {
        return call<Rets...>(std::make_index_sequence<std::tuple_size_v<proxy_key_t>>{}, std::forward<Args>(args)...);
    }

    template <typename K>
    decltype(auto) operator[](K&& key) const {
        auto keys = meta::concat_tuple(m_key, std::forward<K>(key));
        return LookupProxy<Lookup, decltype(keys)>(m_table, std::move(keys));
    }

    template <typename T>
    operator T() const {
        return Get<T>();
    }

    template <typename T>
    void operator=(T&& value) const {
        Set(std::forward<T>(value));
    }

    template <typename... Args>
    void operator()(Args&&... args) const {
        Call<void>(std::forward<Args>(args)...);
    }

private:
    template <typename Ret, size_t... indices>
    decltype(auto) get(std::index_sequence<indices...>) const {
        Stack::PopGuard guard{m_table->GetState(), m_table->Push()};
        return Core::GetNested<Ret, Lookup::global>(m_table->GetState(), std::get<indices>(m_key)...);
    }

    template <size_t... indices, typename T>
    decltype(auto) set(std::index_sequence<indices...>, T&& value) const {
        Stack::PopGuard guard{m_table->GetState(), m_table->Push()};
        return Core::SetNested<Lookup::global>(m_table->GetState(), std::get<indices>(m_key)..., std::forward<T>(value));
    }

    template <size_t... indices>
    decltype(auto) type(std::index_sequence<indices...>) const {
        Stack::PopGuard guard{m_table->GetState(), m_table->Push()};
        return Core::GetType<Lookup::global>(m_table->GetState(), std::get<indices>(m_key)...);
    }

    template <typename T, size_t... indices>
    decltype(auto) check(std::index_sequence<indices...>) const {
        Stack::PopGuard guard{m_table->GetState(), m_table->Push()};
        return Core::Check<T, Lookup::global>(m_table->GetState(), std::get<indices>(m_key)...);
    }

    template <size_t... indices>
    decltype(auto) clean(std::index_sequence<indices...>) const {
        Stack::PopGuard guard{m_table->GetState(), m_table->Push()};
        return Core::Clean<Lookup::global>(m_table->GetState(), std::get<indices>(m_key)...);
    }

    template <typename... Rets, size_t... indices, typename... Args>
    decltype(auto) call(std::index_sequence<indices...>, Args&&... args) const {
        Stack::PopGuard guard{m_table->GetState(), m_table->Push()};
        Core::PushField<Lookup::global>(m_table->GetState(), std::get<indices>(m_key)...);
        return Core::Call<Rets...>(m_table->GetState(), std::forward<Args>(args)...);
    }

    /// Table to get state from, and push to stack.
    const Lookup* m_table;
    /// Global name.
    proxy_key_t m_key;
};

template <typename Lookup, typename Key, typename T>
inline std::enable_if_t<!meta::is_moon_reference_v<T>, bool> operator==(const LookupProxy<Lookup, Key>& left, T&& right) {
    using type = decltype(Core::Get<T>(nullptr, 0));
    return left.template Get<type>() == right;
}

template <typename Lookup, typename Key, typename T>
inline std::enable_if_t<!meta::is_moon_reference_v<T>, bool> operator!=(const LookupProxy<Lookup, Key>& left, T&& right) {
    using type = decltype(Core::Get<T>(nullptr, 0));
    return left.template Get<type>() != right;
}

template <typename Lookup, typename Key, typename T>
inline std::enable_if_t<!meta::is_moon_reference_v<T>, bool> operator==(T&& left, const LookupProxy<Lookup, Key>& right) {
    using type = decltype(Core::Get<T>(nullptr, 0));
    return right.template Get<type>() != left;
}

template <typename Lookup, typename Key, typename T>
inline std::enable_if_t<!meta::is_moon_reference_v<T>, bool> operator!=(T&& left, const LookupProxy<Lookup, Key>& right) {
    using type = decltype(Core::Get<T>(nullptr, 0));
    return right.template Get<type>() != left;
}
}  // namespace moon
