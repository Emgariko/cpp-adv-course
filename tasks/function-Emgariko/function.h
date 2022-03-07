#pragma once

#include <type_traits>
#include <stdexcept>

struct bad_function_call : std::runtime_error {
  explicit bad_function_call(const char* x);
};

struct function_impl {
  template <typename T>
  static constexpr bool
      fits_small_storage = (sizeof(T) <= sizeof(T*) &&
                            alignof(T*) % alignof(T) == 0 &&
                            std::is_nothrow_move_constructible<T>::value);
  template <typename R, typename ...Args>
  struct storage;

  template <typename R, typename... Args>
  struct type_descriptor {
    using storage = function_impl::storage<R, Args...>;

    void (*copy)(storage*, storage const*);
    void (*move)(storage*, storage*);
    R (*apply)(storage*, Args...);
    void (*destroy)(storage*);
  };

  template <typename R, typename... Args>
  struct storage {
    using data_t = std::aligned_storage_t<sizeof(void *), alignof(void *)>;

    template<typename T>
    T* get() {
      if constexpr (fits_small_storage<T>) {
        return reinterpret_cast<T*>(&small);
      } else {
        return *reinterpret_cast<T**>(&small);
      }
    }

    template<typename T>
    const T* get() const {
      if constexpr (fits_small_storage<T>) {
        return reinterpret_cast<T const*>(&small);
      } else {
        return *reinterpret_cast<T *const*>(&small);
      }
    }

    void set(void* ptr) {
      new (&small)(void*)(ptr);
    }

    template <typename T>
    void destroy() {
      if constexpr (fits_small_storage<T>) {
        get()->~T();
      } else {
        delete get();
      }
    }

    void swap(storage& other) noexcept {
      std::swap(desc, other.desc);
      std::swap(small, other.small);
    }

    type_descriptor<R, Args...> const *desc;
    data_t small;
  };

  template <typename R, typename... Args>
  static type_descriptor<R, Args...> const* get_empty_type_descriptor() {
    using storage = function_impl::storage<R, Args...>;

    static constexpr type_descriptor<R, Args...> descriptor = {
        /* copy */ [](storage* dst, storage const* /*src*/) {
          dst->desc = get_empty_type_descriptor<R, Args...>();
        },
        /* move */ [](storage* dst, storage*  /*src*/) {
           dst->desc = get_empty_type_descriptor<R, Args...>();
        },
        /* apply */ [](storage*, Args...) -> R {
          throw bad_function_call("empty function call");
        },
        /* destroy */ [](storage*) {}
    };
    return &descriptor;
  }

  template<typename T, typename = void>
  struct object_traits;

  template<typename T>
  struct object_traits<T, std::enable_if_t<fits_small_storage<T>>> {

    template <typename R, typename... Args>
    static type_descriptor<R, Args...> const* get_obj_descriptor() {
      using storage = function_impl::storage<R, Args...>;

      static constexpr type_descriptor<R, Args...> descriptor = {
          /* copy */ [](storage* dst, storage const* src) {
            new (&dst->small) T(*src->template get<T>());
            dst->desc = src->desc;
          },
          /* move */ [](storage* dst, storage* src) {
           new (&dst->small) T(std::move(*src->template get<T>()));
           dst->desc = src->desc;
           src->desc = get_empty_type_descriptor<R, Args...>();
          },
          /* apply */ [](storage* dst, Args... args) -> R {
           return (*dst->template get<T>())(std::forward<Args>(args)...);
          },
          /* destroy */ [](storage* dst) {
            dst->template get<T>()->~T();
          }
      };
      return &descriptor;
    }

    template <typename R, typename... Args>
    static void init(storage<R, Args...> & storage, T&& func) {
      new (&storage.small) T(std::move(func));
    }
  };

  template<typename T>
  struct object_traits<T, std::enable_if_t<!fits_small_storage<T>>> {
    template <typename R, typename... Args>
    static type_descriptor<R, Args...> const* get_obj_descriptor() {
      using storage = function_impl::storage<R, Args...>;

      static constexpr type_descriptor<R, Args...> descriptor = {
          /* copy */ [](storage* dst, storage const* src) {
            dst->desc = src->desc;
            dst->set(new T(*src->template get<T>()));
          },
          /* move */ [](storage* dst, storage* src) {
           dst->desc = src->desc;
           dst->set((void *)src->template get<T>());
           src->desc = get_empty_type_descriptor<R, Args...>();
          },
          /* apply */ [](storage* dst, Args... args) -> R {
           return (*dst->template get<T>())(std::forward<Args>(args)...);
          },
          /* destroy */ [](storage* dst) {
            delete dst->template get<T>();
          }
      };
      return &descriptor;
    }

    template <typename R, typename... Args>
    static void init(storage<R, Args...>& storage, T&& func) {
      storage.set(new T(std::move(func)));
    }
  };

};

template <typename T>
struct function;

template <typename R, typename... Args>
struct function<R(Args...)> {
  function() {
    storage.desc = function_impl::get_empty_type_descriptor<R, Args...>();
  }

  function(const function& other) : storage() {
    other.storage.desc->copy(&storage, &other.storage);
  }

  function(function&& other) noexcept {
    other.storage.desc->move(&storage, &other.storage);
  }

  function& operator=(const function& other) {
    if (this == &other) {
      return *this;
    }
    function tmp(other);
    swap(tmp);
    return *this;
  }

  function& operator=(function&& other) {
    if (this == &other) {
      return *this;
    }
    function tmp(std::move(other));
    swap(tmp);
    return *this;
  }

  template <typename F>
  function(F f) {
    function_impl::object_traits<F>::init(storage, std::move(f));
    storage.desc = function_impl::object_traits<F>
        ::template get_obj_descriptor<R, Args...>();
  }

  template <typename F>
  F* target() noexcept {
    if (storage.desc ==function_impl::object_traits<F>
            ::template get_obj_descriptor<R, Args...>()) {
      return storage.template get<F>();
    } else {
      return nullptr;
    }
  }

  template <typename F>
  F const* target() const noexcept {
    if (storage.desc == function_impl::object_traits<F>
        ::template get_obj_descriptor<R, Args...>()) {
      return storage.template get<F>();
    } else {
      return nullptr;
    }
  }

  operator bool() noexcept {
    return storage.desc != function_impl::get_empty_type_descriptor<R, Args...>();
  }


  R operator()(Args... args) {
    return storage.desc->apply(&storage, std::forward<Args>(args)...);
  }

  void swap(function& other) noexcept {
    storage.swap(other.storage);
  }

  ~function() {
    storage.desc->destroy(&storage);
  }

private:
  function_impl::storage<R, Args...> storage;
};