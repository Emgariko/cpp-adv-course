#pragma once

#include "variant_constraints.h"
#include <array>
#include <functional>
#include <memory>
#include <type_traits>

template <typename... Ts>
struct variant;

struct bad_variant_access : std::exception {
  bad_variant_access() noexcept = default;

  explicit bad_variant_access(const char* str) noexcept : str(str) {}

  const char* what() const noexcept override {
    return str;
  }

private:
  const char* str = "bad variant access";
};

inline constexpr size_t variant_npos = -1;

namespace details {
template <size_t N, typename... Rest>
struct nth_type {
  using type = void;
};

template <typename T0, typename... Rest>
struct nth_type<0, T0, Rest...> {
  using type = T0;
};

template <size_t N, typename T0, typename... Rest>
struct nth_type<N, T0, Rest...> {
  using type = typename nth_type<N - 1, Rest...>::type;
};

template <size_t N, typename... Ts>
using at_t = typename nth_type<N, Ts...>::type;
} // namespace details

template <typename T>
struct variant_size;

template <typename... Ts>
struct variant_size<variant<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

template <typename... Ts>
struct variant_size<const variant<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

template <typename... Ts>
struct variant_size<volatile variant<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

template <typename... Ts>
struct variant_size<const volatile variant<Ts...>> : std::integral_constant<size_t, sizeof...(Ts)> {};

template <class T>
inline constexpr size_t variant_size_v = variant_size<T>::value;

template <size_t N, typename T>
struct variant_alternative;

template <size_t N, typename... Ts>
struct variant_alternative<N, variant<Ts...>> {
  using type = details::at_t<N, Ts...>;
};

template <size_t N, typename... Ts>
struct variant_alternative<N, const variant<Ts...>> {
  using type = const details::at_t<N, Ts...>;
};

template <size_t N, typename... Ts>
struct variant_alternative<N, volatile variant<Ts...>> {
  using type = volatile details::at_t<N, Ts...>;
};

template <size_t N, typename... Ts>
struct variant_alternative<N, const volatile variant<Ts...>> {
  using type = const volatile details::at_t<N, Ts...>;
};

template <size_t N, typename T>
using variant_alternative_t = typename variant_alternative<N, T>::type;

namespace details {
template <size_t N, typename T, typename... Ts>
struct overload_set;

template <size_t N, typename T>
struct type_info {
  static const size_t index = N;
  using type = T;
};

template <size_t N, typename T, typename U>
struct overload_set<N, T, U> {
  static type_info<N, U> overload(U) requires ValidConversion<T, U>;
};

template <size_t N, typename T, typename U, typename... Ts>
struct overload_set<N, T, U, Ts...> : overload_set<N + 1, T, Ts...> {
  using overload_set<N + 1, T, Ts...>::overload;

  static type_info<N, U> overload(U) requires ValidConversion<T, U>;
};

template <typename T, typename... Ts>
using chosen_overload_t = decltype(overload_set<0, T, Ts...>::overload(std::declval<T>()));

template <typename T, typename... Ts>
struct occurrences_count;

template <typename T>
struct occurrences_count<T> {
  constexpr static const size_t value = 0;
};

template <typename T, typename U>
struct occurrences_count<T, U> {
  constexpr static const size_t value = static_cast<size_t>(std::is_same_v<T, U>);
};

template <typename T, typename U, typename... Ts>
struct occurrences_count<T, U, Ts...> {
  constexpr static const size_t value =
      static_cast<size_t>(std::is_same_v<T, U>) + occurrences_count<T, Ts...>::value;
};

template <typename T, typename... Ts>
constexpr inline bool occurs_only_once_v = occurrences_count<T, Ts...>::value == 1;

template <typename T, size_t N, typename... Ts>
struct find_in_pack;

template <typename T, size_t N>
struct find_in_pack<T, N> {
  constexpr static const size_t value = -1;
};

template <typename T, size_t N, typename U, typename... Ts>
struct find_in_pack<T, N, U, Ts...> {
  constexpr static const size_t value = (std::is_same_v<T, U>) ? N : (find_in_pack<T, N + 1, Ts...>::value);
};

template <typename T, typename... Ts>
inline constexpr size_t find_in_pack_v = find_in_pack<T, 0, Ts...>::value;
} // namespace details

template <typename T, typename... Ts>
constexpr bool holds_alternative(const variant<Ts...>& v) noexcept
    requires(details::occurs_only_once_v<T, Ts...>) {
  return v.index() == details::find_in_pack_v<T, Ts...>;
}

template <size_t N, typename... Ts>
constexpr variant_alternative_t<N, variant<Ts...>>& get(variant<Ts...>& v) {
  if (N == v.index()) {
    return v.val.get(in_place_index<N>);
  } else {
    throw bad_variant_access("variant stores alternative with another index");
  }
}

template <size_t N, typename... Ts>
constexpr const variant_alternative_t<N, variant<Ts...>>& get(const variant<Ts...>& v) {
  if (N == v.index()) {
    return v.val.get(in_place_index<N>);
  } else {
    throw bad_variant_access("variant stores alternative with another index");
  }
}

template <size_t N, typename... Ts>
constexpr variant_alternative_t<N, variant<Ts...>>&& get(variant<Ts...>&& v) {
  return std::move(get<N>(v));
}

template <size_t N, typename... Ts>
constexpr const variant_alternative_t<N, variant<Ts...>>&& get(const variant<Ts...>&& v) {
  return std::move(get<N>(v));
}

template <typename T, typename... Ts, size_t index = details::find_in_pack_v<T, Ts...>>
constexpr T& get(variant<Ts...>& v) {
  if (holds_alternative<T>(v)) {
    return v.val.get(in_place_index<index>);
  } else {
    throw bad_variant_access("variant stores another alternative");
  }
}

template <typename T, typename... Ts, size_t index = details::find_in_pack_v<T, Ts...>>
constexpr const T& get(const variant<Ts...>& v) {
  if (holds_alternative<T>(v)) {
    return v.val.get(in_place_index<index>);
  } else {
    throw bad_variant_access("variant stores another alternative");
  }
}

template <typename T, typename... Ts, size_t index = details::find_in_pack_v<T, Ts...>>
constexpr T&& get(variant<Ts...>&& v) {
  return std::move(get<T>(v));
}

template <typename T, typename... Ts, size_t index = details::find_in_pack_v<T, Ts...>>
constexpr const T&& get(const variant<Ts...>&& v) {
  return std::move(get<T>(v));
}

template <std::size_t N, typename... Ts>
constexpr std::add_pointer_t<variant_alternative_t<N, variant<Ts...>>> get_if(variant<Ts...>* pv) noexcept {
  if (pv == nullptr) {
    return nullptr;
  }
  if (N == pv->index()) {
    return std::addressof(get<N>(*pv));
  } else {
    return nullptr;
  }
}

template <std::size_t N, typename... Ts>
constexpr std::add_pointer_t<const variant_alternative_t<N, variant<Ts...>>>
get_if(const variant<Ts...>* pv) noexcept {
  if (pv == nullptr) {
    return nullptr;
  }
  if (N == pv->index()) {
    return std::addressof(pv->val.get(in_place_index<N>));
  } else {
    return nullptr;
  }
}

template <typename T, typename... Ts, size_t N = details::find_in_pack_v<T, Ts...>>
constexpr std::add_pointer_t<T> get_if(variant<Ts...>* pv) noexcept {
  return get_if<N>(pv);
}

template <typename T, typename... Ts, size_t N = details::find_in_pack_v<T, Ts...>>
constexpr std::add_pointer_t<const T> get_if(const variant<Ts...>* pv) noexcept {
  return get_if<N>(pv);
}

namespace details {
template <typename... Ts>
constexpr std::array<std::common_type_t<Ts...>, sizeof...(Ts)> make_array(Ts&&... t) noexcept {
  return {std::forward<Ts>(t)...};
}

template <typename F, typename... Vs, size_t... Inds>
constexpr auto gen_fmatrix_impl(std::index_sequence<Inds...>) noexcept {
  return [](F f, Vs... vs) { return std::invoke(std::forward<F>(f), get<Inds>(std::forward<Vs>(vs))...); };
}

template <typename F, size_t... Inds>
constexpr auto gen_fmatrix_index_impl(std::index_sequence<Inds...>) noexcept {
  return [](F f) { return std::invoke(std::forward<F>(f), (std::integral_constant<size_t, Inds>())...); };
}

template <typename F, typename... Vs, size_t... FixedInds, size_t... NextInds, typename... RestInds>
constexpr auto gen_fmatrix_impl(std::index_sequence<FixedInds...>, std::index_sequence<NextInds...>,
                                RestInds... rInds) noexcept {
  return make_array(gen_fmatrix_impl<F, Vs...>(std::index_sequence<FixedInds..., NextInds>(), rInds...)...);
};

template <typename F, size_t... FixedInds, size_t... NextInds, typename... RestInds>
constexpr auto gen_fmatrix_index_impl(std::index_sequence<FixedInds...>, std::index_sequence<NextInds...>,
                                      RestInds... rInds) noexcept {
  return make_array(gen_fmatrix_index_impl<F>(std::index_sequence<FixedInds..., NextInds>(), rInds...)...);
};

template <typename F, typename... Vs>
constexpr auto gen_fmatrix() noexcept {
  return gen_fmatrix_impl<F, Vs...>(std::index_sequence<>(),
                                    std::make_index_sequence<variant_size_v<std::remove_reference_t<Vs>>>()...);
}

template <typename F, typename... Vs>
constexpr auto gen_fmatrix_index() noexcept {
  return gen_fmatrix_index_impl<F>(std::index_sequence<>(),
                                   std::make_index_sequence<variant_size_v<std::remove_reference_t<Vs>>>()...);
}

template <typename T>
constexpr T& at_impl(T& matrix) noexcept {
  return matrix;
}

template <typename T, typename... Inds>
constexpr auto& at_impl(T& matrix, size_t i, Inds... inds) noexcept {
  return at_impl(matrix[i], inds...);
}

template <typename T, typename... Inds>
constexpr auto& at(T& matrix, Inds... inds) noexcept {
  return at_impl(matrix, inds...);
}

template <typename F, typename... Vs>
inline constexpr auto fmatrix = gen_fmatrix<F&&, Vs&&...>();

template <typename F, typename... Vs>
inline constexpr auto fmatrix_index = gen_fmatrix_index<F&&, Vs&&...>();

template <typename F, typename... Vs>
constexpr decltype(auto) visit_index(F&& vis, Vs&&... variants) {
  return at(fmatrix_index<F, Vs...>, variants.index()...)(std::forward<F>(vis));
}
} // namespace details

template <typename F, typename... Vs>
constexpr decltype(auto) visit(F&& vis, Vs&&... variants) {
  if ((variants.valueless_by_exception() || ...)) {
    throw bad_variant_access();
  }
  return at(details::fmatrix<F, Vs...>, variants.index()...)(std::forward<F>(vis), std::forward<Vs>(variants)...);
}
