#ifndef LSTL_SLIST_NODE_H
#define LSTL_SLIST_NODE_H

#include "utility.h"

namespace lstl {
namespace detail {

struct slist_node_base {
  slist_node_base* next;

  static void insert_after(slist_node_base* pos, slist_node_base* new_node) {
    new_node->next = pos->next;
    pos->next = new_node;
  }

  static void unlink_after(slist_node_base* pos) {
    slist_node_base* next = pos->next;
    if (next) {
      pos->next = next->next;
    }
  }
};

template <typename T>
struct slist_node : slist_node_base {
  T data;

  slist_node() { next = 0; }
  explicit slist_node(const T& value) : data(value) { next = 0; }
  explicit slist_node(T&& value) : data(move(value)) { next = 0; }
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_SLIST_NODE_H
