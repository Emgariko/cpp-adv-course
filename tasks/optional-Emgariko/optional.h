#pragma once

#include <type_traits>
#include <utility>

struct nullopt_t {
  explicit constexpr nullopt_t(int) {}
};

inline constexpr nullopt_t nullopt{0};

struct in_place_t {
  explicit in_place_t() = default;
};
inline constexpr in_place_t in_place{};

namespace details {
struct dummy_t {};

template <typename T, bool TriviallyDestructible>
struct destructor_base {
  constexpr destructor_base() noexcept : dummy() {}
  constexpr destructor_base(nullopt_t) noexcept : dummy() {}
  template <typename... Args>
  constexpr explicit destructor_base(in_place_t, Args&&... args)
      : active(true), val(std::forward<Args>(args)...) {}
  constexpr destructor_base(const T& val) : active(true), val(val) {}
  constexpr destructor_base(T&& val) : active(true), val(std::move(val)) {}

  constexpr void reset() {
    if (active) {
      val.~T();
      active = false;
    }
  }

  ~destructor_base() = default;

  bool active{false};
  union {
    dummy_t dummy;
    T val;
  };
};

template <typename T>
struct destructor_base<T, false> {
  constexpr destructor_base() noexcept : dummy() {}
  constexpr destructor_base(nullopt_t) noexcept : dummy() {}
  template <typename... Args>
  constexpr explicit destructor_base(in_place_t, Args&&... args)
      : active(true), val(std::forward<Args>(args)...) {}
  constexpr destructor_base(const T& val) : active(true), val(val) {}
  constexpr destructor_base(T&& val) : active(true), val(std::move(val)) {}

  constexpr void reset() {
    if (active) {
      val.~T();
      active = false;
    }
  }

  ~destructor_base() {
    reset();
  }

protected:
  bool active{false};
  union {
    dummy_t dummy;
    T val;
  };
};

template <typename T, bool TriviallyCopyable>
struct copy_move_base
    : destructor_base<T, std::is_trivially_destructible_v<T>> {
  using base = destructor_base<T, std::is_trivially_destructible_v<T>>;
  using base::base;

  constexpr copy_move_base() = default;

  constexpr copy_move_base(const copy_move_base& other) {
    if (other.active) {
      new (&this->val) T(other.val);
    }
    this->active = other.active;
  }
  constexpr copy_move_base(copy_move_base&& other) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    if (other.active) {
      new (&this->val) T(std::move(other.val));
    }
    this->active = other.active;
  }

  constexpr copy_move_base& operator=(const copy_move_base& other) {
    if (!this->active && !other.active) {
      // do nothing
    } else if (this->active && !other.active) {
      this->reset();
    } else if (other.active) {
      if (this->active) {
        this->val = other.val;
      } else {
        new (&this->val) T(other.val);
      }
    }
    this->active = other.active;
    return *this;
  }
  constexpr copy_move_base& operator=(copy_move_base&& other) noexcept(
      std::is_nothrow_move_assignable_v<T>&&
          std::is_nothrow_move_constructible_v<T>) {
    if (!this->active && !other.active) {
      // do nothing
    } else if (this->active && !other.active) {
      this->reset();
    } else if (other.active) {
      if (this->active) {
        this->val = std::move(other.val);
      } else {
        new (&this->val) T(std::move(other.val));
      }
    }
    this->active = other.active;
    return *this;
  }
};

template <typename T>
struct copy_move_base<T, true>
    : destructor_base<T, std::is_trivially_destructible_v<T>> {
  using base = destructor_base<T, std::is_trivially_destructible_v<T>>;
  using base::base;
};

template <bool enable>
struct enable_copy_ctor {};

template <>
struct enable_copy_ctor<false> {
  constexpr enable_copy_ctor() = default;
  constexpr enable_copy_ctor(const enable_copy_ctor&) = delete;
  constexpr enable_copy_ctor(enable_copy_ctor&&) = default;
  constexpr enable_copy_ctor& operator=(const enable_copy_ctor&) = default;
  constexpr enable_copy_ctor& operator=(enable_copy_ctor&&) = default;
};

template <bool enable>
struct enable_move_ctor {};

template <>
struct enable_move_ctor<false> {
  constexpr enable_move_ctor() = default;
  constexpr enable_move_ctor(const enable_move_ctor&) = default;
  constexpr enable_move_ctor(enable_move_ctor&&) = delete;
  constexpr enable_move_ctor& operator=(const enable_move_ctor&) = default;
  constexpr enable_move_ctor& operator=(enable_move_ctor&&) = default;
};

template <bool enable>
struct enable_copy_assign {};

template <>
struct enable_copy_assign<false> {
  constexpr enable_copy_assign() = default;
  constexpr enable_copy_assign(const enable_copy_assign&) = default;
  constexpr enable_copy_assign(enable_copy_assign&&) = default;
  constexpr enable_copy_assign& operator=(const enable_copy_assign&) = delete;
  constexpr enable_copy_assign& operator=(enable_copy_assign&&) = default;
};

template <bool enable>
struct enable_move_assign {};

template <>
struct enable_move_assign<false> {
  constexpr enable_move_assign() = default;
  constexpr enable_move_assign(const enable_move_assign&) = default;
  constexpr enable_move_assign(enable_move_assign&&) = default;
  constexpr enable_move_assign& operator=(const enable_move_assign&) = default;
  constexpr enable_move_assign& operator=(enable_move_assign&&) = delete;
};
} // namespace details
template <typename T>
class optional : details::copy_move_base<T, std::is_trivially_copyable_v<T>>,
                 details::enable_copy_ctor<std::is_copy_constructible_v<T>>,
                 details::enable_move_ctor<std::is_move_constructible_v<T>>,
                 details::enable_copy_assign<std::is_copy_assignable_v<T>>,
                 details::enable_move_assign<std::is_move_assignable_v<T>> {

  using base = details::copy_move_base<T, std::is_trivially_copyable_v<T>>;
  using base::base;

public:
  constexpr optional() = default;
  constexpr optional(const optional&) = default;
  constexpr optional(optional&&) = default;

  constexpr optional(nullopt_t nullopt) : base(nullopt) {}
  template <typename... Args>
  constexpr explicit optional(in_place_t in_place, Args&&... args)
      : base(in_place, std::forward<Args>(args)...) {}

  constexpr optional& operator=(const optional&) = default;
  constexpr optional& operator=(optional&&) = default;

  optional& operator=(nullopt_t) noexcept {
    if (this->active) {
      reset();
    }
    return *this;
  }

  constexpr explicit operator bool() const noexcept {
    return this->active;
  }

  constexpr T* operator->() noexcept {
    return &this->val;
  }
  constexpr T const* operator->() const noexcept {
    return &this->val;
  }

  constexpr T& operator*() noexcept {
    return this->val;
  }
  constexpr T const& operator*() const noexcept {
    return this->val;
  }

  using base::reset;

  template <typename... Args>
  void emplace(Args&&... args) {
    reset();
    new (&this->val) T(std::forward<Args>(args)...);
    this->active = true;
  }

  template <typename T_>
  friend constexpr bool operator==(optional<T_> const&, optional<T_> const&);
  template <typename T_>
  friend constexpr bool operator!=(optional<T_> const&, optional<T_> const&);
  template <typename T_>
  friend constexpr bool operator<(optional<T_> const&, optional<T_> const&);
  template <typename T_>
  friend constexpr bool operator<=(optional<T_> const&, optional<T_> const&);
  template <typename T_>
  friend constexpr bool operator>(optional<T_> const&, optional<T_> const&);
  template <typename T_>
  friend constexpr bool operator>=(optional<T_> const&, optional<T_> const&);
};

template <typename T>
constexpr bool operator==(optional<T> const& a, optional<T> const& b) {
  if (!a.active) {
    return !b.active;
  } else {
    if (!b.active) {
      return false;
    } else {
      return *a == *b;
    }
  }
}

template <typename T>
constexpr bool operator!=(optional<T> const& a, optional<T> const& b) {
  if (!a.active) {
    return b.active;
  } else {
    if (!b.active) {
      return true;
    } else {
      return *a != *b;
    }
  }
}

template <typename T>
constexpr bool operator<(optional<T> const& a, optional<T> const& b) {
  if (!b.active) {
    return false;
  }
  if (!a.active) {
    return true;
  }
  return *a < *b;
}

template <typename T>
constexpr bool operator<=(optional<T> const& a, optional<T> const& b) {
  if (!a.active) {
    return true;
  }
  if (!b.active) {
    return false;
  }
  return *a <= *b;
}

template <typename T>
constexpr bool operator>(optional<T> const& a, optional<T> const& b) {
  if (!a.active) {
    return false;
  }
  if (!b.active) {
    return true;
  }
  return *a > *b;
}

template <typename T>
constexpr bool operator>=(optional<T> const& a, optional<T> const& b) {
  if (!b.active) {
    return true;
  }
  if (!a.active) {
    return false;
  }
  return *a >= *b;
}
