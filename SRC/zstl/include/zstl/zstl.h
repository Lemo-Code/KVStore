// zstl — Custom STL library umbrella header
//
// Include this single header to get all zstl functionality.
// Internal detail headers (detail/*) are not included.
#pragma once

// ============================================================
// Memory
// ============================================================
#include "zstl/memory/type_traits.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/functional.h"
#include "zstl/memory/pool.h"
#include "zstl/memory/allocator.h"
#include "zstl/memory/uninitialized.h"
#include "zstl/memory/smart_ptr.h"

// ============================================================
// Iterators
// ============================================================
#include "zstl/iterators/iterator_traits.h"
#include "zstl/iterators/reverse_iterator.h"
#include "zstl/iterators/move_iterator.h"
#include "zstl/iterators/insert_iterator.h"

// ============================================================
// Containers
// ============================================================
#include "zstl/containers/vector.h"
#include "zstl/containers/list.h"
#include "zstl/containers/slist.h"
#include "zstl/containers/deque.h"
#include "zstl/containers/string.h"
#include "zstl/containers/array.h"
#include "zstl/containers/bitset.h"
#include "zstl/containers/map.h"
#include "zstl/containers/set.h"
#include "zstl/containers/multimap.h"
#include "zstl/containers/multiset.h"
#include "zstl/containers/unordered_map.h"
#include "zstl/containers/unordered_set.h"
#include "zstl/containers/unordered_multimap.h"
#include "zstl/containers/unordered_multiset.h"
#include "zstl/containers/bmap.h"
#include "zstl/containers/bset.h"
#include "zstl/containers/skip_map.h"
#include "zstl/containers/skip_set.h"
#include "zstl/containers/stack.h"
#include "zstl/containers/queue.h"
#include "zstl/containers/priority_queue.h"

// ============================================================
// Algorithms
// ============================================================
#include "zstl/algorithms/sort.h"
#include "zstl/algorithms/find.h"
#include "zstl/algorithms/algorithm.h"

// ============================================================
// String
// ============================================================
#include "zstl/string/string_view.h"

// ============================================================
// Thread
// ============================================================
#include "zstl/thread/atomic.h"
#include "zstl/thread/mutex.h"
