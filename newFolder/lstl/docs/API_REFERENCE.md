# lstl API Reference

## Overview

lstl (Lightweight Standard Template Library) is a header-only C++14 library providing memory management utilities and container classes. It serves as the foundation layer for the high-performance network library and cache server.

## Module Structure

```
lstl/
├── memory/          # Memory management subsystem
│   ├── type_traits.h    # Type introspection
│   ├── utility.h        # pair, swap, move, forward
│   ├── construct.h      # Object construction/destruction
│   ├── uninitialized.h  # Uninitialized memory algorithms
│   ├── allocator.h      # Standard allocator interface
│   ├── alloc.h          # malloc_alloc, default_alloc
│   ├── pool.h           # Memory pool (jemalloc-inspired)
│   ├── temporary_buffer.h # Temporary buffer utilities
│   └── functional.h     # Hash and comparison functors
└── container/       # Container classes
    ├── vector.h         # Dynamic array
    ├── list.h           # Doubly-linked list
    ├── slist.h          # Singly-linked list
    ├── deque.h          # Double-ended queue
    ├── stack.h          # Stack adapter (LIFO)
    ├── queue.h          # Queue adapter (FIFO)
    ├── priority_queue.h # Priority queue (max-heap)
    ├── map.h            # Ordered map (RB-tree)
    ├── set.h            # Ordered set (RB-tree)
    ├── multimap.h       # Ordered multimap
    ├── multiset.h       # Ordered multiset
    ├── unordered_map.h  # Hash map
    ├── unordered_set.h  # Hash set
    ├── unordered_multimap.h
    ├── unordered_multiset.h
    ├── skip_map.h       # Skip list map
    ├── skip_set.h       # Skip list set
    ├── bmap.h           # B+ tree map
    ├── bset.h           # B+ tree set
    └── detail/          # Internal implementations
```

## Memory Subsystem

### Allocators

| Class | Description | Thread-safe |
|-------|-------------|-------------|
| `allocator<T>` | Standard-conforming allocator (new/delete) | Yes |
| `malloc_alloc` | malloc/free wrapper with OOM handler | Yes |
| `default_alloc` | Pool-based multi-size-class allocator | Yes |
| `pool_single` | Single-size memory pool (no locking) | No |

### Memory Operations

| Function | Description |
|----------|-------------|
| `construct(p, args...)` | Placement-new construction |
| `destroy(p)` | Explicit destructor call |
| `destroy(first, last)` | Range destruction (no-op for trivial types) |
| `uninitialized_copy(first, last, result)` | Copy to raw memory |
| `uninitialized_fill(first, last, value)` | Fill raw memory |
| `uninitialized_fill_n(first, n, value)` | Fill n elements |
| `uninitialized_move(first, last, result)` | Move to raw memory |

### Type Traits

| Trait | Description |
|-------|-------------|
| `is_pod<T>` | POD type detection |
| `has_trivial_destructor<T>` | Trivial destructor check |
| `has_trivial_copy_constructor<T>` | Trivial copy ctor check |
| `conditional<B, T, F>` | Compile-time type selection |
| `enable_if<B, T>` | SFINAE enabler |

## Containers

### Sequence Containers

| Container | Backend | Random Access | Insert/Erase |
|-----------|---------|---------------|--------------|
| `vector<T>` | Contiguous array | O(1) | O(n) except at end |
| `list<T>` | Doubly-linked list | O(n) | O(1) |
| `slist<T>` | Singly-linked list | O(n) | O(1) at front |
| `deque<T>` | Segmented array | O(1) | O(1) at ends |

### Associative Containers

| Container | Backend | Order | Find | Insert | Erase |
|-----------|---------|-------|------|--------|-------|
| `map<K,V>` | Red-black tree | Sorted | O(log n) | O(log n) | O(log n) |
| `set<K>` | Red-black tree | Sorted | O(log n) | O(log n) | O(log n) |
| `unordered_map<K,V>` | Hash table | Unsorted | O(1) avg | O(1) avg | O(1) avg |
| `unordered_set<K>` | Hash table | Unsorted | O(1) avg | O(1) avg | O(1) avg |
| `skip_map<K,V>` | Skip list | Sorted | O(log n) exp | O(log n) exp | O(log n) exp |
| `bmap<K,V>` | B+ tree | Sorted | O(log n) | O(log n) | O(log n) |

### Container Adapters

| Adapter | Underlying | Operations |
|---------|-----------|------------|
| `stack<T>` | deque (default) | push, pop, top |
| `queue<T>` | deque (default) | push, pop, front, back |
| `priority_queue<T>` | vector (default) | push, pop, top |

## Memory Pool Architecture

```
default_alloc::allocate(n)
  ├── n ≤ 8192 → pool_impl::instance().allocate(n)
  │     ├── size_class_index(n) → idx
  │     ├── freelists_[idx].pop() → fast path (O(1))
  │     └── refill(idx) → malloc new chunk, carve into blocks
  └── n > 8192 → malloc_alloc::allocate(n)
```

### Size Class Table (28 classes)

| Range | Classes |
|-------|---------|
| 8 - 256 | 8, 16, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256 |
| 320 - 3584 | 320, 384, 448, 512, 768, 1024, 1536, 2048, 2560, 3072, 3584 |
| 4096 - 7168 | 4096, 5120, 6144, 7168 |

## Building

```bash
mkdir build && cd build
cmake .. -DLSTL_BUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

### Requirements

- C++14 compatible compiler (GCC 5+, Clang 3.4+)
- CMake 3.10+

## Usage Examples

### Vector with push_back

```cpp
#include <lstl/container/vector.h>

lstl::vector<int> v;
v.push_back(1);
v.push_back(2);
for (auto& x : v) { /* ... */ }
```

### Map with operator[]

```cpp
#include <lstl/container/map.h>

lstl::map<std::string, int> m;
m["hello"] = 42;
m["world"] = 100;
assert(m["hello"] == 42);
```

### Custom Allocator

```cpp
#include <lstl/memory/allocator.h>
#include <lstl/memory/pool.h>

// Use pool-based allocation via simple_alloc
typedef lstl::simple_alloc<MyNode, lstl::default_alloc> node_alloc;
MyNode* node = node_alloc::allocate(10);
// ... use node ...
node_alloc::deallocate(node, 10);
```

## Known Limitations

1. **B+ tree (bmap/bset)**: Insert supports only single-leaf insertion. Splits that require new internal nodes are not fully implemented. Suitable for datasets up to kOrder-1 (255) elements.
2. **RB-tree move semantics**: The `successor()` function does not correctly handle the rightmost node in single-node trees when used via iteration. Copy/move constructors use count-based iteration to work around this.
3. **No emplace operations**: Constructors with variadic args are only enabled with `__cpp_variadic_templates`.
