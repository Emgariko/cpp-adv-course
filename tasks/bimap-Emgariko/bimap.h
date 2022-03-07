#pragma once

#include "treap.h"
#include <cstdint>
#include <utility>

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
struct bimap {
  using left_t = Left;
  using right_t = Right;

  using map_element_base = details::map_element_base;
  using left_tag = details::left_tag;
  using right_tag = details::right_tag;

  using node_t = details::bimap_node<Left, Right>;
  using left_node_t = details::map_element<Left, details::left_tag>;
  using right_node_t = details::map_element<Right, details::right_tag>;
  using left_treap_t = details::treap<left_t, details::left_tag, CompareLeft>;
  using right_treap_t =
      details::treap<right_t, details::right_tag, CompareRight>;

  template <typename value, typename Tag>
  struct iterator {
    iterator() = delete;

    explicit iterator(const map_element_base* ptr_) noexcept :
          data(const_cast<map_element_base*>(ptr_)) {}

    value const &operator *() const noexcept {
      using to_t = std::conditional_t
          <std::is_same_v<Tag, left_tag>, left_node_t, right_node_t>;
      return static_cast<to_t*>(data)->val;
    }

    value const *operator->() const noexcept {
      return &**this;
    }

    iterator &operator++() noexcept { // ++it
      if (data->right) {
        data = data->right;
        while (data->left) {
          data = data->left;
        }
      } else if (data->is_left_son()) {
        data = data->par;
      } else if(data->is_right_son()) {
        while (data->is_right_son()) {
          data = data->par;
        }
        data = data->par;
      }
      return *this;
    }

    iterator operator++(int) noexcept { // it++
      iterator res = iterator(data);
      ++(*this);
      return res;
    }

    iterator &operator--() noexcept {
      if (data->left) {
        data = data->left;
        while (data->right) {
          data = data->right;
        }
      } else if (data->is_right_son()) {
        data = data->par;
      } else {
        while (data->is_left_son()) {
          data = data->par;
        }
        data = data->par;
      }
      return *this;
    }

    iterator operator--(int) noexcept {
      iterator res = iterator(data);
      --(*this);
      return res;
    }

    using other_tag = std::conditional_t
        <std::is_same_v<Tag, left_tag>, right_tag, left_tag>;
    using other_t = std::conditional_t
        <std::is_same_v<Tag, left_tag>, right_t, left_t>;
    using other_it = iterator<other_t, other_tag>;

    other_it flip() const noexcept {
      map_element_base* ptr;
      if (data->par == nullptr) { // fake node
        ptr = data->right;
      } else {
        using from_t = std::conditional_t
            <std::is_same_v<Tag, left_tag>, left_node_t, right_node_t>;
        using to_t = std::conditional_t
            <std::is_same_v<Tag, right_tag>, left_node_t, right_node_t>;
        ptr = static_cast<to_t*>(
            static_cast<node_t*>(static_cast<from_t*>(data)));
      }
      return other_it(ptr);
    }

    bool operator==(iterator const &other) const noexcept {
      return data == other.data;
    }

    bool operator!=(iterator const &other) const noexcept {
      return !(*this == other);
    }

    friend bimap;
  private:
    map_element_base* data;
  };

  using left_iterator = iterator<left_t, left_tag>;
  using right_iterator = iterator<right_t, right_tag>;

  bimap(CompareLeft compare_left = CompareLeft(),
        CompareRight compare_right = CompareRight()) noexcept
      : left_treap(std::move(compare_left)),
        right_treap(std::move(compare_right)) {
    left_treap.fake.right = &right_treap.fake;
    right_treap.fake.right = &left_treap.fake;
  }

  bimap(bimap const &other)
      : left_treap(static_cast<CompareLeft const&>(other.left_treap)),
        right_treap(static_cast<CompareRight const&>(other.right_treap)) {
    left_treap.fake.right = &right_treap.fake;
    right_treap.fake.right = &left_treap.fake;
    for (left_iterator lit = other.begin_left(); lit != other.end_left(); ++lit) {
      insert(*lit, *lit.flip());
    }
  }

  bimap(bimap &&other) noexcept
      : left_treap(std::move(other.left_treap)),
        right_treap(std::move(other.right_treap)), sz(other.sz)  {}

  bimap &operator=(bimap const &other) {
    if (this == &other) {
      return *this;
    }
    bimap tmp(other);
    swap(tmp);
    return *this;
  }

  bimap &operator=(bimap &&other) noexcept {
    if (this == &other) {
      return *this;
    }
    swap(other);
    other.erase_left(begin_left(), end_left());
    return *this;
  }

  ~bimap() {
    erase_left(begin_left(), end_left());
  }

  void swap(bimap& other) noexcept {
    left_treap.swap(other.left_treap);
    right_treap.swap(other.right_treap);
    std::swap(sz, other.sz);
  }

  template <typename left_t_ = left_t, typename right_t_ = right_t>
  left_iterator insert(left_t_&&left, right_t_&&right) {
    if (left_treap.find(left) != nullptr) {
      return end_left();
    }
    if (right_treap.find(right) != nullptr) {
      return end_left();
    }
    auto* node =
        new node_t(std::forward<left_t_>(left), std::forward<right_t_>(right));
    map_element_base* l_ptr = left_treap.insert(*node);
    right_treap.insert(*node);
    sz++;
    return left_iterator(l_ptr);
  }

  left_iterator erase_left(left_iterator it) noexcept {
    left_iterator copy(it.data);
    ++copy;
    node_t* ptr = left_base_double_downcast(it.data);
    left_treap.erase(it.data);
    right_treap.erase(node_right_upcast(ptr));
    delete ptr;
    sz--;
    return copy;
  }

  bool erase_left(left_t const &left) noexcept {
    left_iterator it = find_left(left);
    if (it == end_left()) {
      return false;
    } else {
      erase_left(it);
      return true;
    }
  }

  right_iterator erase_right(right_iterator it) noexcept {
    right_iterator copy(it.data);
    ++copy;
    node_t* ptr = right_base_double_downcast(it.data);
    left_treap.erase(node_left_upcast(ptr));
    right_treap.erase(it.data);
    delete ptr;
    sz--;
    return copy;
  }

  bool erase_right(right_t const &right) noexcept {
    right_iterator it = find_right(right);
    if (it == end_right()) {
      return false;
    } else {
      erase_right(it);
      return true;
    }
  }

  left_iterator erase_left(left_iterator first, left_iterator last) noexcept {
    while (first != last) {
      erase_left(first++);
    }
    return first;
  }

  right_iterator erase_right(right_iterator first, right_iterator last) noexcept {
    while (first != last) {
      erase_right(first++);
    }
    return first;
  }

  left_iterator find_left(left_t const &left) const noexcept {
    map_element_base* ptr = left_treap.find(left);
    return ptr == nullptr ? end_left() : left_iterator(ptr);
  }

  right_iterator find_right(right_t const &right) const noexcept {
    map_element_base* ptr = right_treap.find(right);
    return ptr == nullptr ? end_right() : right_iterator(ptr);
  }

  right_t const &at_left(left_t const &key) const {
    left_iterator it = find_left(key);
    if (it != end_left()) {
      return *it.flip();
    } else {
      throw std::out_of_range("no such element");
    }
  }

  left_t const &at_right(right_t const &key) const {
    right_iterator it = find_right(key);
    if (it != end_right()) {
      return *it.flip();
    } else {
      throw std::out_of_range("no such element");
    }
  }

  template <typename = std::enable_if<std::is_default_constructible_v<Right>>>
  right_t const &at_left_or_default(left_t const &key) noexcept {
    left_iterator lit = find_left(key);
    if (lit != end_left()) {
      return *lit.flip();
    } else {
      right_t dflt_r = right_t();
      right_iterator rit = find_right(dflt_r);
      if (rit != end_right()) {
        left_iterator lit1 = rit.flip();
        left_treap.erase(lit1.data);
        left_base_downcast(lit1.data)->val = key;
        left_treap.insert(*left_base_downcast(lit1.data));
        return *rit;
      } else {
        return *insert(key, std::move(dflt_r)).flip();
      }
    }
  }

  template <typename = std::enable_if<std::is_default_constructible_v<Left>>>
  left_t const &at_right_or_default(right_t const &key) noexcept {
    right_iterator rit = find_right(key);
    if (rit != end_right()) {
      return *rit.flip();
    } else {
      left_t dflt_l = left_t();
      left_iterator lit = find_left(dflt_l);
      if (lit != end_left()) {
        right_iterator rit1= lit.flip();
        right_treap.erase(rit1.data);
        right_base_downcast(rit1.data)->val = key;
        right_treap.insert(*right_base_downcast(rit1.data));
        return *lit;
      } else {
        return *insert(std::move(dflt_l), key);
      }
    }
  }

  left_iterator lower_bound_left(const left_t &left) const noexcept {
    return left_iterator(left_treap.lower_bound(left));
  }
  left_iterator upper_bound_left(const left_t &left) const noexcept {
    return left_iterator(left_treap.upper_bound(left));
  }

  right_iterator lower_bound_right(const right_t &right) const noexcept {
    return right_iterator(right_treap.lower_bound(right));
  }

  right_iterator upper_bound_right(const right_t &right) const noexcept {
    return right_iterator(right_treap.upper_bound(right));
  }

  left_iterator begin_left() const noexcept {
    return left_iterator(left_treap.min());
  }

  left_iterator end_left() const noexcept {
    return left_iterator(&left_treap.fake);
  }

  right_iterator begin_right() const noexcept {
    return right_iterator(right_treap.min());
  }

  right_iterator end_right() const noexcept {
    return right_iterator(&right_treap.fake);
  }

  bool empty() const noexcept {
    return sz == 0;
  }

  std::size_t size() const noexcept {
    return sz;
  }

  friend bool operator==(bimap const &a, bimap const &b) noexcept {
    if (a.sz != b.sz) {
      return false;
    }
    left_iterator a_lit = a.begin_left(), b_lit = b.begin_left(), a_end = a.end_left();
    while (a_lit != a_end) {
      if (!(a.left_treap.equal(*a_lit, *b_lit) &&
            a.right_treap.equal(*a_lit.flip(), *b_lit.flip()))) {
        return false;
      }
      a_lit++, b_lit++;
    }
    return true;
  }
  friend bool operator!=(bimap const &a, bimap const &b) noexcept {
    return !(a == b);
  }

private:
  size_t sz{0};
  left_treap_t left_treap;
  right_treap_t right_treap;

  static right_node_t* node_right_upcast(node_t* node) noexcept {
    return static_cast<right_node_t*>(node);
  }

  static left_node_t* node_left_upcast(node_t* node) noexcept {
    return static_cast<left_node_t*>(node);
  }

  static left_node_t* left_base_downcast(map_element_base* left) noexcept {
    return static_cast<left_node_t*>(left);
  }

  static right_node_t* right_base_downcast(map_element_base* right) noexcept {
    return static_cast<right_node_t*>(right);
  }

  static node_t* left_base_double_downcast(map_element_base* left) noexcept {
    return static_cast<node_t*>(static_cast<left_node_t*>(left));
  }

  static node_t* right_base_double_downcast(map_element_base* right) noexcept {
    return static_cast<node_t*>(static_cast<right_node_t*>(right));
  }
};
