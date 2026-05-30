#ifndef KV_POOL_FREELIST_H
#define KV_POOL_FREELIST_H

namespace kv {
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

inline void freelist_push_list(FreeNode** head, FreeNode* first, FreeNode* last) {
  if (!first) {
    return;
  }
  last->next = *head;
  *head = first;
}

}  // namespace detail
}  // namespace kv

#endif  // KV_POOL_FREELIST_H
