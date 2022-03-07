#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <type_traits>

template <typename T>
class shared_ptr;

template <typename T>
class weak_ptr;

namespace shared_ptr_details {
class control_block {
public:
  control_block();

  void inc_strong();

  void inc_weak();

  void dec_strong();

  void dec_weak();

  template <typename T_>
  friend class ::shared_ptr;
  template <typename T_>
  friend class ::weak_ptr;

  virtual void delete_data() = 0;
  virtual ~control_block() = default;

private:
  size_t strong_ref{0};
  size_t weak_ref{0};
};

template <typename T, typename D>
class ptr_block : public control_block, D {
public:
  T* ptr;

  explicit ptr_block(T* ptr_, D deleter) : ptr{ptr_}, D(std::move(deleter)) {
    inc_strong();
  }

  T* get() {
    return ptr;
  }

  void delete_data() override {
    static_cast<D&> (*this)(ptr);
  }
};

template <typename T>
class obj_block : public control_block {
public:
  std::aligned_storage_t<sizeof(T), alignof(T)> obj;

  template <typename... Args>
  explicit obj_block(Args&&... args) {
    new (&obj) T(std::forward<Args>(args)...);
    inc_strong();
  }

  T* get() {
    return reinterpret_cast<T*>(&obj);
  }

  void delete_data() override {
    get()->~T();
  }
};
}

template <typename T>
class shared_ptr {
public:
  shared_ptr() noexcept = default;
  shared_ptr(std::nullptr_t) noexcept : block_ptr(nullptr), ptr(nullptr) {}

  template <typename T_, typename D = std::default_delete<T_>,
            std::enable_if_t<std::is_convertible_v<T_, T>, bool> = true>
  explicit shared_ptr(T_* ptr, D&& deleter = D()) : ptr(ptr) {
    try {
      block_ptr = new shared_ptr_details::ptr_block<T_, D>(
          ptr, std::forward<D>(deleter));
    } catch (...) {
      delete ptr;
      throw;
    }
  }

  shared_ptr(const shared_ptr& other) noexcept
      : block_ptr(other.block_ptr), ptr(other.ptr) {
    inc();
  }

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& other, T* ptr) noexcept
      : block_ptr(other.block_ptr), ptr(ptr) {
    inc();
  }

  template <typename T_,
            std::enable_if_t<std::is_convertible_v<T_, T>, bool> = true>
  shared_ptr(const shared_ptr<T_>& other)
      : block_ptr(other.block_ptr), ptr(other.ptr) {
    inc();
  }

  shared_ptr(shared_ptr&& other) noexcept {
    swap(other);
  }

  shared_ptr& operator=(const shared_ptr& other) {
    if (this == &other) {
      return *this;
    }
    shared_ptr(other).swap(*this);
    return *this;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    shared_ptr(std::move(other)).swap(*this);
    return *this;
  }

  T* get() const noexcept {
    return ptr ? ptr : nullptr;
  }

  operator bool() const noexcept {
    return get() != nullptr;
  }

  T& operator*() const noexcept {
    return *get();
  }

  T* operator->() const noexcept {
    return get();
  }

  std::size_t use_count() const noexcept {
    return block_ptr ? block_ptr->strong_ref : 0;
  }

  void reset() noexcept {
    shared_ptr().swap(*this);
  }

  template <typename T_, typename D = std::default_delete<T_>,
            std::enable_if_t<std::is_convertible_v<T_, T>, bool> = true>
  void reset(T_* new_ptr, D&& deleter = D()) {
    shared_ptr<T> ptr_(new_ptr, std::forward<D>(deleter));
    swap(ptr_);
  }

  void swap(shared_ptr<T>& x) {
    std::swap(x.block_ptr, block_ptr);
    std::swap(ptr, x.ptr);
  }

  ~shared_ptr() {
    if (block_ptr) {
      block_ptr->dec_strong();
    }
  }

  template <typename T_>
  friend class shared_ptr;
  template <typename U>
  friend class weak_ptr;
  template<class T_, class... Args>
  friend shared_ptr<T_> make_shared(Args&&... args);

private:
  shared_ptr(shared_ptr_details::control_block* block_ptr_, T* ptr)
      : block_ptr(block_ptr_), ptr(ptr) {}

  void inc() {
    if (block_ptr) {
      block_ptr->inc_strong();
    }
  }

  shared_ptr_details::control_block* block_ptr{nullptr};
  T* ptr{nullptr};
};

template <typename T, typename T_>
bool operator==(const shared_ptr<T>& a, const shared_ptr<T_>& b) {
  return a.get() == b.get();
}

template <typename T, typename T_>
bool operator!=(const shared_ptr<T>& a, const shared_ptr<T_>& b) {
  return !(a == b);
}

template <typename T>
bool operator==(const shared_ptr<T>& a, const std::nullptr_t& b) {
  return a.get() == b;
}

template <typename T>
bool operator!=(const shared_ptr<T>& a, const std::nullptr_t& b) {
  return !(a == b);
}

template <typename T>
bool operator==(const std::nullptr_t& a, const shared_ptr<T>& b) {
  return b == a;
}

template <typename T>
bool operator!=(const std::nullptr_t& a, const shared_ptr<T>& b) {
  return !(b == a);
}

template <typename T>
class weak_ptr {
public:
  weak_ptr() noexcept = default;
  weak_ptr(const shared_ptr<T>& other) noexcept
      : block_ptr(other.block_ptr), ptr(other.ptr) {
    inc();
  }
  weak_ptr(const weak_ptr<T>& other) noexcept
      : block_ptr(other.block_ptr), ptr(other.ptr) {
    inc();
  }

  weak_ptr(weak_ptr<T>&& other) noexcept {
    swap(other);
  }

  weak_ptr& operator=(const weak_ptr<T>& other) noexcept {
    if (this == &other) {
      return *this;
    }
    weak_ptr(other).swap(*this);
    return *this;
  }

  weak_ptr& operator=(weak_ptr<T>&& other) noexcept {
    if (this == &other) {
      return *this;
    }
    weak_ptr(std::move(other)).swap(*this);
    return *this;
  }

  shared_ptr<T> lock() const noexcept {
    if (!block_ptr || block_ptr->strong_ref == 0) {
      return shared_ptr<T>();
    } else {
      block_ptr->inc_strong();
      return shared_ptr<T>(block_ptr, ptr);
    }
  }

  void swap(weak_ptr& other) {
    std::swap(block_ptr, other.block_ptr);
    std::swap(ptr, other.ptr);
  }

  ~weak_ptr() {
    if (block_ptr) {
      block_ptr->dec_weak();
    }
  }
private:
  void inc() {
    if (block_ptr) {
      block_ptr->inc_weak();
    }
  }

  shared_ptr_details::control_block* block_ptr{nullptr};
  T* ptr{nullptr};
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  auto* control_block =
      new shared_ptr_details::obj_block<T>(std::forward<Args>(args)...);
  return shared_ptr<T>(control_block, control_block->get());
}
