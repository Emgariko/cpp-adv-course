#pragma once

#include "variant_helpers.h"
#include "variant_storage.h"
#include <utility>

template <typename... Ts>
struct variant : private details::variant_storage_t<Ts...> {
  using base = details::variant_storage_t<Ts...>;
  using zero_alt_t = details::at_t<0, Ts...>;

  constexpr variant() noexcept(std::is_nothrow_default_constructible_v<zero_alt_t>) = delete;

  constexpr variant() noexcept(std::is_nothrow_default_constructible_v<zero_alt_t>)
      requires std::is_default_constructible_v<zero_alt_t> = default;

  template <typename T, typename chosen_type_info = details::chosen_overload_t<T, Ts...>,
            typename chosen_T = typename chosen_type_info::type, size_t index = chosen_type_info::index>
  constexpr variant(T&& t) noexcept(std::is_nothrow_constructible_v<T, chosen_T>)
      requires details::ConvertibleToChosenTypeVariant<variant, T, chosen_T, Ts...>
      : base(in_place_index<index>, std::forward<T>(t)) {}

  template <typename T, typename chosen_type_info = details::chosen_overload_t<T, Ts...>,
            typename chosen_T = typename chosen_type_info::type, size_t index = chosen_type_info::index>
  constexpr variant& operator=(T&& t) noexcept(
      std::is_nothrow_assignable_v<chosen_T&, T>&& std::is_nothrow_constructible_v<chosen_T, T>)
      requires details::ConvertibleToChosenTypeVariant<variant, T, chosen_T, Ts...> {
    if (this->index() == index) {
      get<index>(*this) = std::forward<T>(t);
    } else if (std::is_nothrow_constructible_v<chosen_T, T> || !std::is_nothrow_move_constructible_v<chosen_T>) {
      emplace<index>(std::forward<T>(t));
    } else {
      emplace<index>(chosen_T(std::forward<T>(t)));
    }
    return *this;
  }

  constexpr variant(const variant& other) = delete;

  constexpr variant(const variant& other) requires(details::CopyConstructible<Ts...>)
      : base(details::uninitialized_storage_tag) {
    if (!other.valueless_by_exception()) {
      details::visit_index(
          [this, &other](auto ind_) {
            this->construct_internal_value(in_place_index<ind_()>, get<ind_()>(other));
          },
          other);
    } else {
      make_valueless();
    }
  }

  constexpr variant(const variant&) requires(details::TriviallyCopyConstructible<Ts...>) = default;

  constexpr variant& operator=(const variant& other) = delete;

  constexpr variant& operator=(const variant& other) requires(details::CopyAssignable<Ts...>) {
    if (other.valueless_by_exception()) {
      if (valueless_by_exception()) {
        return *this;
      } else {
        this->destroy_internal_value();
        make_valueless();
      }
    }
    details::visit_index(
        [this, &other](auto ind_) {
          if (index() == ind_()) {
            get<ind_()>(*this) = get<ind_()>(other);
          } else {
            using other_alt_t = details::at_t<ind_(), Ts...>;
            if (std::is_nothrow_copy_constructible_v<other_alt_t> ||
                !std::is_nothrow_move_constructible_v<other_alt_t>) {
              emplace<ind_()>(get<ind_()>(other));
            } else {
              this->operator=(variant(other));
            }
          }
        },
        other);
    return *this;
  }

  constexpr variant& operator=(const variant& other) requires(details::TriviallyCopyAssignable<Ts...>) = default;

  constexpr variant(variant&& other) = delete;

  constexpr variant(variant&& other) noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
      requires(details::MoveConstructible<Ts...>)
      : base(details::uninitialized_storage_tag) {
    if (!other.valueless_by_exception()) {
      details::visit_index(
          [this, &other](auto ind_) {
            this->construct_internal_value(in_place_index<ind_()>, get<ind_()>(std::move(other)));
          },
          other);
    } else {
      make_valueless();
    }
  }

  constexpr variant(variant&& other) noexcept((std::is_nothrow_move_constructible_v<Ts> && ...))
      requires(details::TriviallyMoveConstructible<Ts...>) = default;

  constexpr variant& operator=(variant&& other) = delete;

  constexpr variant& operator=(variant&& other) noexcept(((std::is_nothrow_move_constructible_v<Ts> &&
                                                           std::is_nothrow_move_assignable_v<Ts>)&&...))
      requires(details::MoveAssignable<Ts...>) {
    if (other.valueless_by_exception()) {
      if (valueless_by_exception()) {
        return *this;
      } else {
        this->destroy_internal_value();
        make_valueless();
        return *this;
      }
    }
    details::visit_index(
        [this, &other](auto ind_) {
          if (index() == ind_()) {
            get<ind_()>(*this) = get<ind_()>(std::move(other));
          } else {
            emplace<ind_()>(get<ind_()>(std::move(other)));
          }
        },
        other);
    return *this;
  }

  constexpr variant& operator=(variant&& other) noexcept(((std::is_nothrow_move_constructible_v<Ts> &&
                                                           std::is_nothrow_move_assignable_v<Ts>)&&...))
      requires(details::TriviallyMoveAssignable<Ts...>) = default;

  template <typename T, size_t index = details::find_in_pack_v<T, Ts...>, typename... Args>
  constexpr explicit variant(in_place_type_t<T>, Args&&... args)
      requires(std::is_constructible_v<T, Args...> && details::occurs_only_once_v<T, Ts...>)
      : base(in_place_index<index>, std::forward<Args>(args)...) {}

  template <size_t N, typename T_N = details::at_t<N, Ts...>, typename... Args>
  constexpr explicit variant(in_place_index_t<N> ind, Args&&... args)
      requires(!std::is_same_v<T_N, void> && std::is_constructible_v<T_N, Args...>)
      : base(ind, std::forward<Args>(args)...) {}

  template <std::size_t N, typename T_N = details::at_t<N, Ts...>, typename... Args>
  constexpr T_N& emplace(Args&&... args) requires(std::is_constructible_v<T_N, Args...>) {
    static_assert(N < sizeof...(Ts));

    base::destroy_internal_value();
    make_valueless();
    return base::construct_internal_value(in_place_index<N>, std::forward<Args>(args)...);
  }

  template <typename T, typename... Args, size_t index = details::find_in_pack_v<T, Ts...>>
  constexpr T& emplace(Args&&... args)
      requires(std::is_constructible_v<T, Args...> && details::occurs_only_once_v<T, Ts...>) {
    return emplace<index, T>(std::forward<Args>(args)...);
  }

  constexpr void swap(variant& other) noexcept(noexcept(((std::is_nothrow_move_constructible_v<Ts> &&
                                                          std::is_nothrow_swappable_v<Ts>)&&...))) {
    if (valueless_by_exception() && other.valueless_by_exception()) {
      return;
    }
    if (other.index() == index()) {
      details::visit_index(
          [this, &other](auto ind_) {
            using std::swap;
            swap(get<ind_()>(*this), get<ind_()>(other));
            swap(this->ind, other.ind);
          },
          other);
    } else {
      using std::swap;
      swap(*this, other);
    }
  }

  constexpr bool valueless_by_exception() const noexcept {
    return index() == variant_npos;
  }

  constexpr size_t index() const noexcept {
    return this->ind;
  }

  constexpr void make_valueless() noexcept {
    this->ind = variant_npos;
  }

  constexpr ~variant() = default;

  template <size_t N, typename... Ts_>
  friend constexpr variant_alternative_t<N, variant<Ts_...>>& get(variant<Ts_...>& v);
  template <size_t N, typename... Ts_>
  friend constexpr const variant_alternative_t<N, variant<Ts_...>>& get(const variant<Ts_...>& v);

  template <typename T, typename... Ts_, size_t index>
  friend constexpr T& get(variant<Ts_...>& v);
  template <typename T, typename... Ts_, size_t index>
  friend constexpr const T& get(const variant<Ts_...>& v);

  template <bool triviallyDestructible, typename... Ts_>
  friend struct details::variant_storage;
};

template <typename... Ts>
constexpr bool operator==(const variant<Ts...>& v, const variant<Ts...>& w) noexcept {
  if (v.index() != w.index()) {
    return false;
  }
  if (v.valueless_by_exception()) {
    return true;
  }
  return details::visit_index([&v, &w](auto ind_) -> bool { return get<ind_()>(v) == get<ind_()>(w); }, v);
}

template <typename... Ts>
constexpr bool operator!=(const variant<Ts...>& v, const variant<Ts...>& w) noexcept {
  return !(v == w);
}

template <typename... Ts>
constexpr bool operator<(const variant<Ts...>& v, const variant<Ts...>& w) noexcept {
  if (w.valueless_by_exception()) {
    return false;
  }
  if (v.valueless_by_exception()) {
    return true;
  }
  return details::visit_index(
      [&v, &w](auto ind1, auto ind2) -> bool {
        if (ind1() < ind2()) {
          return true;
        }
        if (ind1() > ind2()) {
          return false;
        }
        return get<ind1()>(v) < get<ind1()>(w);
      },
      v, w);
}

template <typename... Ts>
constexpr bool operator>(const variant<Ts...>& v, const variant<Ts...>& w) noexcept {
  return w < v;
}

template <typename... Ts>
constexpr bool operator<=(const variant<Ts...>& v, const variant<Ts...>& w) noexcept {
  return !(v > w);
}

template <typename... Ts>
constexpr bool operator>=(const variant<Ts...>& v, const variant<Ts...>& w) noexcept {
  return !(v < w);
}
