#include "treap.h"

details::map_element_base::map_element_base() noexcept = default;

bool details::map_element_base::is_left_son() noexcept {
  return par->left == this;
}

bool details::map_element_base::is_right_son() noexcept {
  return par->right == this;
}

void details::map_element_base::adopt(map_element_base* new_parent) noexcept {
  par = new_parent;
}
