#include "shared-ptr.h"

shared_ptr_details::control_block::control_block()  = default;

void shared_ptr_details::control_block::inc_strong() {
  strong_ref++;
  weak_ref++;
}
void shared_ptr_details::control_block::inc_weak() {
  weak_ref++;
}
void shared_ptr_details::control_block::dec_strong() {
  --strong_ref;
  if (strong_ref == 0) {
    delete_data();
  }
  dec_weak();
}
void shared_ptr_details::control_block::dec_weak() {
  --weak_ref;
  if (weak_ref == 0) {
    delete this;
  }
}
