#pragma once

template <typename... Ts>
struct variant;

struct in_place_t {
  explicit in_place_t() = default;
};

inline constexpr in_place_t in_place{};

template <class T>
struct in_place_type_t {
  explicit in_place_type_t() = default;
};

template <class T>
inline constexpr in_place_type_t<T> in_place_type{};

template <std::size_t I>
struct in_place_index_t {
  explicit in_place_index_t() = default;
};

template <std::size_t I>
inline constexpr in_place_index_t<I> in_place_index{};

namespace details {
template <typename T>
struct is_in_place_type_t {
  static constexpr bool value = false;
};

template <typename U>
struct is_in_place_type_t<in_place_type_t<U>> {
  static constexpr bool value = true;
};

template <size_t N>
struct is_in_place_type_t<in_place_index_t<N>> {
  static constexpr bool value = true;
};

template <typename T>
constexpr bool is_in_place_type_v = is_in_place_type_t<T>::value;

template <typename... Ts>
concept CopyConstructible = (std::is_copy_constructible_v<Ts> && ...);

template <typename... Ts>
concept TriviallyCopyConstructible = CopyConstructible<Ts...> &&
                                     (std::is_trivially_copy_constructible_v<Ts> && ...);

template <typename... Ts>
concept MoveConstructible = (std::is_move_constructible_v<Ts> && ...);

template <typename... Ts>
concept TriviallyMoveConstructible = MoveConstructible<Ts...> &&
                                     (std::is_trivially_move_constructible_v<Ts> && ...);

template <typename... Ts>
concept CopyAssignable = CopyConstructible<Ts...> && (std::is_copy_assignable_v<Ts> && ...);

template <typename... Ts>
concept TriviallyDestructible = (std::is_trivially_destructible_v<Ts> && ...);

template <typename... Ts>
concept TriviallyCopyAssignable = CopyAssignable<Ts...>&& TriviallyCopyConstructible<Ts...> &&
                                  (std::is_trivially_copy_assignable_v<Ts> && ...) && TriviallyDestructible<Ts...>;

template <typename... Ts>
concept MoveAssignable = MoveConstructible<Ts...> && (std::is_move_assignable_v<Ts> && ...);

template <typename... Ts>
concept TriviallyMoveAssignable = MoveAssignable<Ts...>&& TriviallyMoveConstructible<Ts...> &&
                                  (std::is_trivially_move_assignable_v<Ts> && ...) && TriviallyDestructible<Ts...>;

template <typename variant, typename T, typename T_chosen, typename... Ts>
concept ConvertibleToChosenTypeVariant = (sizeof...(Ts) > 0) &&
                                         (!is_in_place_type_v<std::remove_cvref_t<T>>)&&(
                                             !std::is_same_v<T, variant>)&&(std::is_constructible_v<T_chosen, T>);

template <typename T, typename T_i>
concept ValidConversion = requires(T&& x) {
  T_i{std::forward<T>(x)};
};
} // namespace details
