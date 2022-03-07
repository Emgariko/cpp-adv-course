#pragma once

#include <iterator>

namespace intrusive {
struct default_tag;

template <typename T, typename Tag = default_tag>
struct list;

struct list_element_base {
  list_element_base() : next(this), prev(this) { }

  // list work
  void unlink() noexcept;
  void clear() noexcept;
  void insert(list_element_base& pos) noexcept;  // inserts this before pos

  ~list_element_base() {
    unlink();
  }

  template <typename T, typename Tag>
  friend struct list;
private:
  list_element_base* next;
  list_element_base* prev;
};

template <typename Tag = default_tag>
struct list_element : list_element_base {
  list_element() = default;
  ~list_element() = default;

  list_element(list_element const&) = delete;
  list_element& operator=(list_element const&) = delete;

  template<typename T, typename Tag_>
  friend struct list;
  // base -> this
  // this -> base
};

template <typename T, typename Tag>
struct list {
  static_assert(std::is_base_of_v<list_element<Tag>, T>,
                "You should derive from list_element");
  template<typename U>
  struct list_iterator {
    using iterator_category = std::bidirectional_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = std::remove_const_t<T>;
    using pointer = T*;
    using reference = T&;

    list_iterator() {
      data = nullptr;
    }

    explicit list_iterator(list_element_base* data) : data(data) {}

    explicit list_iterator(const list_element_base* data) :
          data(const_cast<list_element_base*>(data)) {}

    template <typename V, std::enable_if_t<std::is_const_v<U> || std::is_same_v<V, U>, bool> = true>
    list_iterator(const list_iterator<V>& other) : data(other.data) {}

    // Forward iterator requirements
    U& operator*() const {
      return to_U();
    }

    U* operator->() const {
      return &to_U();
    }

    list_iterator<U>& operator++() {
      data = data->next;
      return *this;
    }

    list_iterator<U> operator++(int) {
      data = data->next;
      return list_iterator<U>(data->prev);
    }

    // Bidirectional iterator requirements
    list_iterator<U>& operator--() {
      data = data->prev;
      return *this;
    }

    list_iterator<U> operator--(int) {
      data = data->prev;
      return list_iterator<U>(data->next);
    }

    bool operator==(list_iterator<U>const & other) const noexcept {
      return data == other.data;
    }

    bool operator!=(list_iterator<U>const & other) const noexcept {
      return !(*this == other);
    }

    friend list;
  private:
    list_element_base* data;

    U& to_U() const {
      return *static_cast<U*>(static_cast<list_element<Tag>*>(data));
    }
  };

public:
  using iterator = list_iterator<T>;
  using const_iterator = list_iterator<const T>;

  list() = default;
  list(list const& other) = delete;

  list(list&& other) noexcept {
    swap(other);
  }

  ~list() {
    clear();
  }
  list& operator=(list const&) = delete;

  void swap(list<T>& other) noexcept {
    std::swap(fake.next->prev, other.fake.next->prev);
    std::swap(fake.next, other.fake.next);
    std::swap(fake.prev->next, other.fake.prev->next);
    std::swap(fake.prev, other.fake.prev);
  }

  list& operator=(list&& other) noexcept {
    list<T, Tag> tmp = std::move(other);
    swap(tmp);
    return *this;
  }

  void clear() noexcept {
    fake.clear();
  }

  /*
  Поскольку вставка изменяет данные в list_element
  мы принимаем неконстантный T&.
  */
  void push_back(T& x) noexcept {
    to_base(x).insert(fake);
  }

  void pop_back() noexcept {
    fake.prev->unlink();
  }

  T& back() noexcept {
    return *static_cast<T*>(fake.prev);
  }

  T const& back() const noexcept {
    return *static_cast<T const *>(fake.prev);
  }

  void push_front(T& x) noexcept {
    to_base(x).insert(*fake.next);
  }

  void pop_front() noexcept {
    fake.next->unlink();
  }

  T& front() noexcept {
    return *static_cast<T*>(fake.next);
  }

  T const& front() const noexcept {
    return *static_cast<T const *>(fake.next);
  }

  bool empty() const noexcept {
    return fake.next == &fake;
  }

  iterator begin() noexcept {
    return iterator(fake.next);
  }

  const_iterator begin() const noexcept {
    return const_iterator(fake.next);
  }

  iterator end() noexcept {
    return iterator(&fake);
  }

  const_iterator end() const noexcept {
    return const_iterator(&fake);
  }

  iterator insert(const_iterator pos, T& x) noexcept {
    x.unlink();
    x.insert(*pos.data);
    return iterator(pos.data->prev);
  }

  iterator erase(const_iterator pos) noexcept {
    iterator res = iterator(pos.data->next);
    pos.data->unlink();
    return res;
  }

  void splice(const_iterator pos, list&, const_iterator first,
              const_iterator last) noexcept {
    list_element_base* p = first.data->prev;

    first.data->prev->next = last.data;
    first.data->prev = pos.data->prev;

    pos.data->prev->next = first.data;
    pos.data->prev = last.data->prev;

    last.data->prev->next = pos.data;
    last.data->prev = p;
  }

  static list_element_base& to_base(T& elem) noexcept {
    return static_cast<list_element_base&>(static_cast<list_element<Tag>&>(elem));
  }
private:
  list_element_base fake;
};
}