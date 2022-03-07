#pragma once

template <typename... Ts>
struct variant;

namespace details {
struct uninitialized_storage_tag_t {};

constexpr inline uninitialized_storage_tag_t uninitialized_storage_tag;

template <bool trivially_destructible, typename... Ts>
union mega_union {
  constexpr mega_union() = default;
  constexpr mega_union(uninitialized_storage_tag_t) noexcept {}
};

template <bool trivially_destructible, typename T, typename... Rest>
union mega_union<trivially_destructible, T, Rest...> {
  constexpr mega_union() : val() {}

  constexpr mega_union(uninitialized_storage_tag_t tag) : rest(tag) {}

  template <typename... Args>
  constexpr mega_union(in_place_index_t<0>, Args&&... args) : val(std::forward<Args>(args)...) {}

  template <size_t N, typename... Args>
  constexpr mega_union(in_place_index_t<N>, Args&&... args)
      : rest(in_place_index<N - 1>, std::forward<Args>(args)...){};

  template <size_t N, typename... Args>
  constexpr decltype(auto) construct_internal_value(in_place_index_t<N>, Args&&... args) {
    return rest.construct_internal_value(in_place_index<N - 1>, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr T& construct_internal_value(in_place_index_t<0>, Args&&... args) {
    new (const_cast<std::remove_cv_t<T>*>(std::addressof(val))) T(std::forward<Args>(args)...);
    return val;
  }

  template <size_t N>
  constexpr auto& get(in_place_index_t<N>) noexcept {
    return rest.get(in_place_index<N - 1>);
  }

  constexpr auto& get(in_place_index_t<0>) noexcept {
    return val;
  }

  template <size_t N>
  constexpr const auto& get(in_place_index_t<N>) const noexcept {
    return rest.get(in_place_index<N - 1>);
  }

  constexpr const auto& get(in_place_index_t<0>) const noexcept {
    return val;
  }

  template <size_t N>
  constexpr void destroy_internal_value(in_place_index_t<N>) {}

  ~mega_union() = default;

  T val;
  mega_union<trivially_destructible, Rest...> rest;
};

template <typename T, typename... Rest>
union mega_union<false, T, Rest...> {
  constexpr mega_union() : val() {}

  constexpr mega_union(uninitialized_storage_tag_t tag) : rest(tag) {}

  template <typename... Args>
  constexpr mega_union(in_place_index_t<0>, Args&&... args) : val(std::forward<Args>(args)...) {}

  template <size_t N, typename... Args>
  constexpr mega_union(in_place_index_t<N>, Args&&... args)
      : rest(in_place_index<N - 1>, std::forward<Args>(args)...){};

  template <size_t N, typename... Args>
  constexpr decltype(auto) construct_internal_value(in_place_index_t<N>, Args&&... args) {
    return rest.construct_internal_value(in_place_index<N - 1>, std::forward<Args>(args)...);
  }

  template <typename... Args>
  constexpr T& construct_internal_value(in_place_index_t<0>, Args&&... args) {
    new (const_cast<std::remove_cv_t<T>*>(std::addressof(val))) T(std::forward<Args>(args)...);
    return val;
  }

  template <size_t N>
  constexpr auto& get(in_place_index_t<N>) noexcept {
    return rest.get(in_place_index<N - 1>);
  }

  constexpr auto& get(in_place_index_t<0>) noexcept {
    return val;
  }

  template <size_t N>
  constexpr const auto& get(in_place_index_t<N>) const noexcept {
    return rest.get(in_place_index<N - 1>);
  }

  constexpr const auto& get(in_place_index_t<0>) const noexcept {
    return val;
  }

  template <size_t N>
  constexpr void destroy_internal_value(in_place_index_t<N>) {
    rest.destroy_internal_value(in_place_index<N - 1>);
  }

  constexpr void destroy_internal_value(in_place_index_t<0>) {
    val.~T();
  }

  ~mega_union() {}

protected:
  T val;
  mega_union<false, Rest...> rest;
};

template <bool trivially_destructible, typename... Ts>
struct variant_storage {
  constexpr variant_storage() : val(), ind(0) {}

  constexpr variant_storage(uninitialized_storage_tag_t tag) : val(tag) {}

  template <size_t N, typename... Args>
  constexpr variant_storage(in_place_index_t<N> ind, Args&&... args)
      : val(ind, std::forward<Args>(args)...), ind(N) {}

  template <size_t N, typename... Args>
  decltype(auto) construct_internal_value(in_place_index_t<N> ind_, Args&&... args) {
    auto& res = val.construct_internal_value(ind_, std::forward<Args>(args)...);
    ind = N;
    return res;
  }

  constexpr void destroy_internal_value() {
    if (ind != variant_npos) {
      details::visit_index([this](auto ind_) { this->val.destroy_internal_value(in_place_index<ind_()>); },
                           *static_cast<variant<Ts...>*>(this));
    }
  }

  ~variant_storage() = default;

protected:
  mega_union<(std::is_trivially_destructible_v<Ts> && ...), Ts...> val;
  size_t ind{variant_npos};
};

template <typename... Ts>
struct variant_storage<false, Ts...> {
  constexpr variant_storage() : val(), ind(0) {}

  constexpr variant_storage(uninitialized_storage_tag_t tag) : val(tag) {}

  template <size_t N, typename... Args>
  constexpr variant_storage(in_place_index_t<N> ind, Args&&... args)
      : val(ind, std::forward<Args>(args)...), ind(N) {}

  template <size_t N, typename... Args>
  decltype(auto) construct_internal_value(in_place_index_t<N> ind_, Args&&... args) {
    auto& res = val.construct_internal_value(ind_, std::forward<Args>(args)...);
    ind = N;
    return res;
  }

  constexpr void destroy_internal_value() {
    if (ind != variant_npos) {
      details::visit_index([this](auto ind_) { this->val.destroy_internal_value(in_place_index<ind_()>); },
                           *static_cast<variant<Ts...>*>(this));
    }
  }

  constexpr ~variant_storage() {
    destroy_internal_value();
  }

protected:
  mega_union<(std::is_trivially_destructible_v<Ts> && ...), Ts...> val;
  size_t ind{variant_npos};
};

template <typename... Ts>
using variant_storage_t = variant_storage<(std::is_trivially_destructible_v<Ts> && ...), Ts...>;
} // namespace details
