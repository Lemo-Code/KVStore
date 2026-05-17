#ifndef LSTL_MEMORY_SPAN_REGISTRY_H
#define LSTL_MEMORY_SPAN_REGISTRY_H

#include "freelist.h"
#include "malloc_alloc.h"
#include "span.h"

namespace lstl {
namespace detail {

struct SpanRegistry {
  SpanHeader* head;
  size_t mapped_bytes;

  SpanRegistry() : head(0), mapped_bytes(0) {}

  void register_span(SpanHeader* span) {
    span->next = head;
    head = span;
    mapped_bytes += span->alloc_size;
  }

  void unregister_span(SpanHeader* span) {
    SpanHeader** cur = &head;
    while (*cur) {
      if (*cur == span) {
        *cur = span->next;
        mapped_bytes -= span->alloc_size;
        return;
      }
      cur = &(*cur)->next;
    }
  }

  void detach_freelist_nodes(SpanHeader* span, FreeNode** free_lists, size_t count) {
    const char* begin = span_user_base(span);
    const char* end = static_cast<const char*>(span->alloc_base) + span->alloc_size;
    for (size_t i = 0; i < count; ++i) {
      FreeNode** cur = &free_lists[i];
      while (*cur) {
        const char* node = reinterpret_cast<const char*>(*cur);
        if (node >= begin && node < end) {
          *cur = (*cur)->next;
        } else {
          cur = &(*cur)->next;
        }
      }
    }
  }

  size_t purge_idle(SpanHeader* active_bump_span, FreeNode** free_lists, size_t list_count) {
    size_t released = 0;
    SpanHeader** cur = &head;
    while (*cur) {
      SpanHeader* span = *cur;
      const bool bump_active = (span->flags & 1u) != 0 || span == active_bump_span;
      if (span->free_count == span->block_count && span->block_count > 0 && !bump_active) {
        detach_freelist_nodes(span, free_lists, list_count);
        *cur = span->next;
        mapped_bytes -= span->alloc_size;
        released += span->alloc_size;
        malloc_alloc_t::deallocate(span->alloc_base, 0);
      } else {
        cur = &span->next;
      }
    }
    return released;
  }
};

inline SpanRegistry& span_registry() {
  static SpanRegistry reg;
  return reg;
}

}  // namespace detail
}  // namespace lstl

#endif  // LSTL_MEMORY_SPAN_REGISTRY_H
