#include "intrusive_list.h"

void intrusive::list_element_base::unlink() noexcept {
  prev->next = this->next;
  next->prev = this->prev;
  next = prev = this;
}

void intrusive::list_element_base::clear() noexcept {
  intrusive::list_element_base* cur = this;
  do {
    intrusive::list_element_base* nxt = cur->next;
    cur->next = cur->prev = cur;
    cur = nxt;
  } while (cur != this);
}

void intrusive::list_element_base::insert(list_element_base& pos) noexcept {
  pos.prev->next = this;
  prev = pos.prev;
  next = &pos;
  pos.prev = this;
}