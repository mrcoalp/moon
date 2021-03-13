#pragma once

namespace moon::meta {
template <typename T, typename Ret = T>
using is_bool_t = std::enable_if_t<std::is_same_v<std::decay_t<T>, bool>, Ret>;

template <typename T>
constexpr bool is_integral_v = std::is_integral_v<std::decay_t<T>> && !std::is_same_v<std::decay_t<T>, bool>;

template <typename T, typename Ret = T>
using is_integral_t = std::enable_if_t<is_integral_v<T>, Ret>;

template <typename T, typename Ret = T>
using is_floating_point_t = std::enable_if_t<std::is_floating_point_v<std::decay_t<T>>, Ret>;

template <typename T>
constexpr bool is_string_v = std::is_same_v<std::decay_t<T>, std::string>;

template <typename T, typename Ret = T>
using is_string_t = std::enable_if_t<is_string_v<T>, Ret>;

template <typename T>
constexpr bool is_c_string_v = std::is_same_v<std::decay_t<T>, const char*>;

template <typename T, typename Ret = T>
using is_c_string_t = std::enable_if_t<is_c_string_v<T>, Ret>;

template <typename T>
constexpr bool is_basic_string_v = is_string_v<T> || is_c_string_v<T>;

template <typename T, typename Ret = T>
using is_basic_string_t = std::enable_if_t<is_basic_string_v<T>, Ret>;

template <typename T, typename Ret = T>
using is_pointer_t = std::enable_if_t<std::is_pointer_v<T> && !std::is_same_v<T, const char*>, Ret>;

namespace meta_detail {
template <typename T>
struct vector : std::false_type {};

template <typename T>
struct vector<std::vector<T>> : std::true_type {};
}  // namespace meta_detail

template <typename T, typename Ret = T>
using is_vector_t = std::enable_if_t<meta_detail::vector<std::decay_t<T>>::value, Ret>;

namespace meta_detail {
template <typename T>
struct map : std::false_type {};

template <typename T>
struct map<std::unordered_map<std::string, T>> : std::true_type {};

template <typename T>
struct map<std::map<std::string, T>> : std::true_type {};
}  // namespace meta_detail

template <typename T, typename Ret = T>
using is_map_t = std::enable_if_t<meta_detail::map<std::decay_t<T>>::value, Ret>;

namespace meta_detail {
template <typename T>
struct function : std::false_type {};

template <typename T>
struct function<std::function<T>> : std::true_type {};
}  // namespace meta_detail

template <typename T, typename Ret = T>
using is_function_t = std::enable_if_t<meta_detail::function<std::decay_t<T>>::value, Ret>;

template <typename T, typename Ret = T>
using is_not_function_t = std::enable_if_t<!meta_detail::function<std::decay_t<T>>::value, Ret>;

namespace meta_detail {
template <typename>
struct tuple : std::false_type {
    /// Counts types contained in tuple. 1 when non tuple.
    static const size_t type_count{1};
};

template <typename... T>
struct tuple<std::tuple<T...>> : std::true_type {
    /// Counts types contained in tuple. Tuple size when tuple.
    static const size_t type_count{std::tuple_size_v<std::tuple<T...>>};
};
}  // namespace meta_detail

template <typename T>
constexpr bool is_tuple_v = meta_detail::tuple<std::decay_t<T>>::value;

template <typename T, typename Ret = T>
using is_tuple_t = std::enable_if_t<is_tuple_v<T>, Ret>;

template <typename T, typename... Ts>
constexpr bool none_is_v = !std::disjunction_v<std::is_same<T, Ts>...>;

template <typename T, typename... Ts>
constexpr bool all_are_v = std::conjunction_v<std::is_same<T, Ts>...>;

template <size_t size, typename... Ts>
constexpr bool sizeof_is_v = size == sizeof...(Ts);

template <typename... Ts>
using multi_ret_t = std::conditional_t<sizeof_is_v<1, Ts...>, std::tuple_element_t<0, std::tuple<Ts...>>, std::tuple<Ts...>>;

/// Requires single return type and that that type is void.
template <typename... Ts>
using call_void_t = std::enable_if_t<meta::sizeof_is_v<1, Ts...> && meta::all_are_v<void, Ts...>, void>;

/// One or more non void return types. With multiple return types, convert type to tuple.
template <typename... Ts>
using call_return_t = std::enable_if_t<meta::none_is_v<void, Ts...>, meta::multi_ret_t<Ts...>>;

namespace meta_detail {
template <typename T, T... elements>
struct sum {
    static const T value{static_cast<T>(0)};  // 0 if no elements
};

template <typename T, T element, T... rest>
struct sum<T, element, rest...> {
    static const T value{element + sum<T, rest...>::value};
};
}  // namespace meta_detail

template <typename T, T... elements>
constexpr T sum_v = meta_detail::sum<T, elements...>::value;

namespace meta_detail {
template <typename... Ts>
struct lua_call_count_arg_ret {
    static const size_t value{tuple<Ts...>::type_count};
};
}  // namespace meta_detail

/// Counts number of results or arguments expected to be passed/retrieved from Lua call of function.
/// This number will vary depending if we're coupling types in tuples or not. With this method, we ensure always a valid count for user specified
/// types.
/// \example
/// \code Call<int, std::tuple<bool, size_t>, std::string>(std::tuple<int, int>, int)
/// \endcode
/// will result in 4 returns expected and 3 arguments expected.
/// \tparam Ts Types to count from.
template <typename... Ts>
constexpr size_t count_expected_v = sum_v<size_t, meta_detail::lua_call_count_arg_ret<Ts>::value...>;

namespace meta_detail {
template <typename T, typename = void>
struct is_callable : std::is_function<std::remove_pointer_t<T>> {};

template <typename T>
struct is_callable<T, std::enable_if_t<std::is_final_v<std::decay_t<T>> && std::is_class_v<std::decay_t<T>> &&
                                       std::is_same_v<decltype(void(&T::operator())), void>>> {};

template <typename T>
struct is_callable<
    T, std::enable_if_t<!std::is_final_v<std::decay_t<T>> && std::is_class_v<std::decay_t<T>> && std::is_destructible_v<std::decay_t<T>>>> {
    struct F {
        void operator()(){};
    };
    struct Derived : T, F {};
    template <typename U, U>
    struct Check;

    template <typename V>
    static std::false_type test(Check<void (F::*)(), &V::operator()>*) {
        return {};
    }

    template <typename>
    static std::true_type test(...) {
        return {};
    }

    static constexpr bool value = std::is_same_v<decltype(test<Derived>(0)), std::true_type>;
};

template <typename T>
struct is_callable<
    T, std::enable_if_t<!std::is_final_v<std::decay_t<T>> && std::is_class_v<std::decay_t<T>> && !std::is_destructible_v<std::decay_t<T>>>> {
    struct F {
        void operator()(){};
    };
    struct Derived : T, F {
        ~Derived() = delete;
    };
    template <typename U, U>
    struct Check;

    template <typename V>
    static std::false_type test(Check<void (F::*)(), &V::operator()>*) {
        return {};
    }

    template <typename>
    static std::true_type test(...) {
        return {};
    }

    static constexpr bool value = std::is_same_v<decltype(test<Derived>(0)), std::true_type>;
};
}  // namespace meta_detail

template <typename T>
constexpr bool is_callable_v = meta_detail::is_callable<std::decay_t<T>>::value;

template <typename T>
using convert_to_tuple_t = std::conditional_t<is_tuple_v<T>, T, std::tuple<T>>;

namespace meta_detail {
template <typename T>
decltype(auto) force_tuple(T&& value) {
    if constexpr (is_tuple_v<T>) {
        return std::forward<T>(value);
    } else {
        return std::tuple<T>(std::forward<T>(value));
    }
}
}  // namespace meta_detail

template <typename... T>
decltype(auto) concat_tuple(T&&... value) {
    return std::tuple_cat(meta_detail::force_tuple(std::forward<T>(value))...);
}
}  // namespace moon::meta
