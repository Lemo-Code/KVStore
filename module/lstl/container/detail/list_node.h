#ifndef LSTL_LIST_NODE_H
#define LSTL_LIST_NODE_H

#include "utility.h"

namespace lstl {
namespace detail {

struct list_node_base {
  list_node_base* next;
  list_node_base* prev;

  // 在 pos 之前插入 new_node
  static void insert_before(list_node_base* pos, list_node_base* new_node) {
    new_node->next = pos;
    new_node->prev = pos->prev;
    pos->prev->next = new_node;
    pos->prev = new_node;
  }

  static void unlink(list_node_base* node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  static void transfer(list_node_base* first, list_node_base* last, list_node_base* position) {
    if (position != last) {
      last->prev->next = position;
      first->prev->next = last;
      position->prev->next = first;
      list_node_base* tmp = position->prev;
      position->prev = last->prev;
      last->prev = first->prev;
      first->prev = tmp;
    }
  }
};

template <typename T>
struct list_node : list_node_base {
  T data;

  list_node() {}
  explicit list_node(const T& value) : data(value) {}
  explicit list_node(T&& value) : data(move(value)) {}
};

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_LIST_NODE_H
