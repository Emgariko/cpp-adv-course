#pragma once

#include <functional>
#include <random>
#include <utility>

template <typename Left, typename Right, typename CompareLeft,
          typename CompareRight>
struct bimap;

namespace details {

thread_local inline std::mt19937 rnd{};

struct left_tag;
struct right_tag;

struct map_element_base {
  map_element_base() noexcept;

  bool is_left_son() noexcept;

  bool is_right_son() noexcept;

  void adopt(map_element_base* new_parent) noexcept;

  template <typename Left, typename Right, typename CompareLeft,
            typename CompareRight>
  friend struct ::bimap;

  template <typename T_, typename Tag_, typename Comparator>
  friend struct treap;

private:
  map_element_base* par{nullptr};
  map_element_base* left{nullptr};
  map_element_base* right{nullptr};
};

template <typename T, typename Tag>
struct map_element : map_element_base {
  map_element() noexcept = default;

  explicit map_element(T val_) noexcept
      : val(std::move(val_)), prior(rnd()) {}

  ~map_element() = default;

  map_element(map_element const&) = delete;
  map_element& operator=(map_element const&) = delete;

  template <typename Left, typename Right, typename CompareLeft,
            typename CompareRight>
  friend struct ::bimap;

  template <typename T_, typename Tag_, typename Comparator>
  friend struct treap;

private:
  T val;
  uint32_t prior{static_cast<uint32_t>(-1)};
};

template <typename K, typename V>
struct bimap_node : map_element<K, left_tag>, map_element<V, right_tag> {
  bimap_node() noexcept = default;

  template <typename K_, typename V_>
  bimap_node(K_&& left, V_&& right) noexcept
      : map_element<K, left_tag>(std::forward<K_>(left)),
        map_element<V, right_tag>(std::forward<V_>(right)) {}
};

template <typename T, typename Tag, typename Comparator>
struct treap : Comparator {
  using treap_element_t = map_element<T, Tag>;

  treap() noexcept = default;
  explicit treap(const Comparator& cmp_) : Comparator(cmp_) {}
  explicit treap(Comparator&& cmp_) noexcept : Comparator(std::move(cmp_)) {}
  treap(const treap&) = delete;
  treap(treap&& other) noexcept {
    swap(other);
  }
  treap& operator=(const treap&) = delete;
  treap& operator=(treap&&) = delete;
  ~treap() = default;

  bool empty() const noexcept {
    return fake.left == nullptr;
  }

  static void swap_fake(map_element_base& a, map_element_base& b) noexcept {
    std::swap(a.right, b.right);
    std::swap(a.right->right, b.right->right);
    std::swap(a.left, b.left);
    if (a.left != nullptr) {
      a.left->par = &a;
    }
    if (b.left != nullptr) {
      b.left->par = &b;
    }
  }

  void swap(treap& other) noexcept {
    swap_fake(fake, other.fake);
    std::swap(get_cmp(), other.get_cmp());
  }

  map_element_base* insert(treap_element_t& node) noexcept {
    if (empty()) {
      fake.left = &to_base(node);
    } else {
      auto [l, r] =
          split(node.val, to_derived_ptr(fake.left));
      treap_element_t* ptr = merge(l, &node);
      fake.left = merge(ptr, r);
    }
    fake.left->par = &fake;
    return &node;
  }

  treap_element_t* find(T const& val) const noexcept {
    return find(val, to_derived_ptr(fake.left));
  }

  map_element_base const* min() const noexcept {
    return min(&fake);
  }

  bool erase(T const& val) noexcept {
    return erase_in_subtree(val, to_derived_ptr(fake.left));
  }

  bool erase_in_subtree(T const& val, treap_element_t* t) noexcept {
    if (t == nullptr) {
      return false;
    }
    if (less(t->val, val)) {
      return erase_in_subtree(val, t->right);
    } else if (greater_or_equal(t->val, val)) {
      erase(t);
      return true;
    } else {
      return erase_in_subtree(val, t->left);
    }
  }

  void erase(map_element_base* t) noexcept {
    treap_element_t* res =
        merge(to_derived_ptr(t->left), to_derived_ptr(t->right));
    if (t == fake.left) {
      fake.left = res;
    }
    if (res != nullptr) {
      res->adopt(t->par);
    }
    if (t->is_right_son()) {
      t->par->right = res;
    } else {
      t->par->left = res;
    }
    t->left = t->right = t->par = nullptr;
  }

  map_element_base const* lower_bound(const T& x) const noexcept {
    return bound(x, [this](const T& a, const T& b)
                 { return greater(a, b); });
  }

  map_element_base const* upper_bound(const T& x) const noexcept {
    return bound(x, [this](const T& a, const T& b)
                 { return greater_or_equal(a, b); });
  }

  bool less(const T& a, const T& b) const noexcept {
    return cmp(a, b);
  }

  bool greater(const T&a, const T&b) const noexcept {
    return cmp(b, a);
  }

  bool less_or_equal(const T& a, const T& b) const noexcept {
    return !cmp(b, a);
  }

  bool greater_or_equal(const T& a, const T& b) const noexcept {
    return !cmp(a, b);
  }

  bool equal(const T& a, const T& b) const noexcept {
    return less_or_equal(a, b) && greater_or_equal(a, b);
  }

  template <typename Left, typename Right, typename CompareLeft,
            typename CompareRight>
  friend struct ::bimap;

private:
  map_element_base fake;

  Comparator& get_cmp() {
    return static_cast<Comparator&>(*this);
  }

  Comparator const& get_cmp() const {
    return static_cast<Comparator const&>(*this);
  }

  bool cmp(const T& a, const T& b) const noexcept {
    return get_cmp()(a, b);
  }

  static map_element_base& to_base(treap_element_t& elem) noexcept {
    return static_cast<map_element_base&>(elem);
  }

  static treap_element_t* to_derived_ptr(map_element_base* base) noexcept {
    return static_cast<treap_element_t*>(base);
  }

  template <typename F>
  map_element_base const* bound(const T& x, F&& check_bound) const noexcept {
    map_element_base const* possible_answer = &fake;
    treap_element_t const* cur = to_derived_ptr(fake.left);
    while (cur != nullptr) {
      if (check_bound(x, cur->val)) {
        cur = to_derived_ptr(cur->right);
      } else {
        possible_answer = cur;
        cur = to_derived_ptr(cur->left);
      }
    }
    return possible_answer;
  }

  std::pair<treap_element_t*, treap_element_t*>
  split(T& x, treap_element_t* t) noexcept {
    if (t == nullptr) {
      return {nullptr, nullptr};
    }
    if (less(t->val, x)) {
      std::pair<map_element_base*, map_element_base*> tt =
          split(x, to_derived_ptr(t->right));
      t->right = tt.first;
      if (tt.second != nullptr) {
        tt.second->adopt(nullptr);
      }
      if (tt.first != nullptr) {
        tt.first->adopt(t);
      }
      return {t, to_derived_ptr(tt.second)};
    } else {
      std::pair<map_element_base*, map_element_base*> tt =
          split(x, to_derived_ptr(t->left));
      t->left = tt.second;
      if (tt.first != nullptr) {
        tt.first->adopt(nullptr);
      }
      if (tt.second != nullptr) {
        tt.second->adopt(t);
      }
      return {to_derived_ptr(tt.first), t};
    }
  }

  treap_element_t* merge(treap_element_t* t1, treap_element_t* t2) noexcept {
    if (t1 == nullptr) {
      return t2;
    }
    if (t2 == nullptr) {
      return t1;
    }
    if (t1->prior > t2->prior) {
      map_element_base* res = merge(to_derived_ptr(t1->right), t2);
      t1->right = res;
      if (res != nullptr) {
        res->adopt(t1);
      }
      return t1;
    } else {
      map_element_base* res = merge(t1, to_derived_ptr(t2->left));
      t2->left = res;
      if (res != nullptr) {
        res->adopt(t2);
      }
      return t2;
    }
  }

  treap_element_t* find(T const& val, treap_element_t* node) const noexcept {
    if (node == nullptr) {
      return nullptr;
    }
    if (less(node->val, val)) {
      return find(val, to_derived_ptr(node->right));
    } else if (greater_or_equal(val, node->val)) {
      return node;
    } else {
      return find(val, to_derived_ptr(node->left));
    }
  }

  static map_element_base const* min(map_element_base const* t) noexcept {
    map_element_base const* ptr = t;
    while (ptr->left != nullptr) {
      ptr = ptr->left;
    }
    return ptr;
  }
};
}
