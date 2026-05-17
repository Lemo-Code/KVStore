#ifndef LSTL_MEMORY_FREELIST_H
#define LSTL_MEMORY_FREELIST_H

namespace lstl {
namespace detail {

union FreeNode {
  FreeNode* next;
};

inline void freelist_push(FreeNode** head, FreeNode* node) {
  node->next = *head;
  *head = node;
}

inline FreeNode* freelist_pop(FreeNode** head) {
  FreeNode* node = *head;
  if (node) {
    *head = node->next;
  }
  return node;
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_FREELIST_H
