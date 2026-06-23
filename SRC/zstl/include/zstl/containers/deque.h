// zstl deque — segmented array (double-ended queue) with block-based storage
//
// Storage:
//   - A "map" (array of T**) points to fixed-size blocks.
//   - Block size: max(1, 512 / sizeof(T)) — typically ~512 bytes per block.
//   - Map grows dynamically as needed; starts with 8 slots, doubles when full.
//
// Complexity:
//   - push_front/back, pop_front/back: O(1) amortized
//   - insert/erase at ends: O(1) amortized
//   - insert/erase in middle: O(n) — proportional to distance to nearer end
//   - operator[], at(): O(1)
//   - Iterator arithmetic: O(1) (crosses block boundaries)
//
// Iterator invalidation:
//   - push/emplace at either end: all iterators invalidated if map reallocates.
//   - insert in middle: all iterators if map reallocation; otherwise only
//     iterators at/after the insertion point.
//   - erase in middle: all iterators at/after the erased element.
//   - pop: only the popped element's iterator.
#pragma once

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <algorithm>
#include "zstl/memory/allocator.h"
#include "zstl/memory/construct.h"
#include "zstl/memory/utility.h"
#include "zstl/memory/type_traits.h"
#include "zstl/iterators/iterator_traits.h"
#include "zstl/iterators/reverse_iterator.h"
#include "zstl/containers/detail/deque_iterator.h"

namespace zstl {

template<typename T, typename Alloc = default_alloc<T>>
class deque {
public:
    // ---- types ----
    using value_type      = T;
    using allocator_type  = Alloc;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using reference       = T&;
    using const_reference = const T&;
    using pointer         = T*;
    using const_pointer   = const T*;

    using iterator       = detail::deque_iterator<T>;
    using const_iterator = detail::deque_iterator<const T>;
    using reverse_iterator       = zstl::reverse_iterator<iterator>;
    using const_reverse_iterator = zstl::reverse_iterator<const_iterator>;

    using map_pointer    = T**;
    using block_size_t   = size_t;

    static constexpr size_t kInitialMapSize = 8;

private:
    map_pointer  map_      = nullptr;   // map of block pointers
    size_type    map_size_ = 0;         // allocated slots in map
    iterator     start_;                // first valid element
    iterator     finish_;               // one past last valid element
    [[no_unique_address]] allocator_type alloc_;

    using map_alloc_type = default_alloc<T*>;
    map_alloc_type map_alloc_;

    // ---- block size ----
    static constexpr size_t block_size() noexcept {
        return iterator::block_size();
    }

    // ---- map helpers ----
    void deallocate_map() noexcept {
        if (map_) {
            map_alloc_.deallocate(map_, map_size_);
            map_ = nullptr;
            map_size_ = 0;
        }
    }

    // Allocate a new block and return a pointer to it
    pointer allocate_block() {
        return alloc_.allocate(block_size());
    }

    void deallocate_block(pointer block) noexcept {
        alloc_.deallocate(block, block_size());
    }

    // Initialize an empty deque with one block in the middle
    void init_map(size_type num_elements) {
        size_type num_nodes = kInitialMapSize;
        size_type extra = (num_elements + block_size() - 1) / block_size();
        if (extra == 0) extra = 1;  // At least one block for empty deque
        if (num_nodes < extra + 2) {
            num_nodes = extra + 2;
        }
        map_size_ = num_nodes;
        map_ = map_alloc_.allocate(num_nodes);

        // Place blocks in the middle of the map
        map_pointer start_node = map_ + (num_nodes - extra) / 2;
        map_pointer finish_node = start_node + extra;

        // Allocate blocks
        for (map_pointer cur = start_node; cur < finish_node; ++cur) {
            *cur = allocate_block();
        }

        start_.set_node(start_node);
        start_.cur_ = start_.first_;
        finish_.set_node(finish_node - 1);
        finish_.cur_ = finish_.first_ + (num_elements % block_size());
    }

    // Ensure there is room at the back for at least one element.
    // May cause map reallocation.
    void reserve_back() {
        if (finish_.cur_ + 1 != finish_.last_) return;  // Already have room

        // Need a new block at the back
        if (finish_.node_ + 1 == map_ + map_size_) {
            // Map is full at the back; reallocate
            reserve_map_at_back();
        }
        if (*(finish_.node_ + 1) == nullptr) {
            *(finish_.node_ + 1) = allocate_block();
        }
        finish_.set_node(finish_.node_ + 1);
        finish_.cur_ = finish_.first_;
    }

    void reserve_front() {
        if (start_.cur_ != start_.first_) return;  // Already have room

        // Need a new block at the front
        if (start_.node_ == map_) {
            // Map is full at the front; reallocate
            reserve_map_at_front();
        }
        if (*(start_.node_ - 1) == nullptr) {
            *(start_.node_ - 1) = allocate_block();
        }
        start_.set_node(start_.node_ - 1);
        start_.cur_ = start_.last_;
    }

    // Add n empty slots at the back of the map
    void reserve_map_at_back(size_type n = 1) {
        if (n <= static_cast<size_type>(map_ + map_size_ - finish_.node_ - 1)) return;
        reallocate_map(map_size_ + zstl::max(map_size_, n) + 2);
    }

    // Add n empty slots at the front of the map
    void reserve_map_at_front(size_type n = 1) {
        if (n <= static_cast<size_type>(start_.node_ - map_)) return;
        reallocate_map(map_size_ + zstl::max(map_size_, n) + 2);
    }

    void reallocate_map(size_type new_map_size) {
        map_pointer new_map = map_alloc_.allocate(new_map_size);
        map_pointer new_start = new_map + (new_map_size - (finish_.node_ - start_.node_ + 1)) / 2;
        map_pointer p = new_start;

        // Copy valid block pointers and nil out the rest
        for (map_pointer q = start_.node_; q <= finish_.node_; ++q) {
            *p++ = *q;
        }
        for (size_type i = 0; i < new_map_size; ++i) {
            if (new_map + i < new_start || new_map + i >= p) {
                new_map[i] = nullptr;
            }
        }

        deallocate_map();
        map_ = new_map;
        map_size_ = new_map_size;
        start_.set_node(new_start);
        finish_.set_node(p - 1);
    }

    // Fill remaining map entries as initialized empty range
    void init_fill_map(pointer value) {
        // Fill the map slots between start_ and finish_ if any are nullptr
        // (only needed during construction)
    }

public:
    // ============================================================
    // Constructors
    // ============================================================
    deque() {
        init_map(0);
    }

    explicit deque(size_type count) {
        init_map(0);
        for (size_type i = 0; i < count; ++i) {
            push_back(T());
        }
    }

    deque(size_type count, const T& value) {
        init_map(0);
        for (size_type i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    deque(InputIterator first, InputIterator last) {
        init_map(0);
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    deque(const deque& other) {
        init_map(other.size());
        for (size_type i = 0; i < other.size(); ++i) {
            push_back(other[i]);
        }
    }

    deque(deque&& other) noexcept
        : map_(other.map_)
        , map_size_(other.map_size_)
        , start_(other.start_)
        , finish_(other.finish_) {
        other.map_ = nullptr;
        other.map_size_ = 0;
        other.init_map(0);
    }

    deque(std::initializer_list<T> il) {
        init_map(il.size());
        for (const auto& v : il) {
            push_back(v);
        }
    }

    ~deque() {
        clear();
        // Deallocate remaining blocks
        if (map_) {
            for (map_pointer p = start_.node_; p <= finish_.node_; ++p) {
                if (*p) alloc_.deallocate(*p, block_size());
            }
            deallocate_map();
        }
    }

    // ============================================================
    // Assignment
    // ============================================================
    deque& operator=(const deque& other) {
        if (this != &other) {
            clear();
            for (size_type i = 0; i < other.size(); ++i) {
                push_back(other[i]);
            }
        }
        return *this;
    }

    deque& operator=(deque&& other) noexcept {
        if (this != &other) {
            clear();
            if (map_) {
                for (map_pointer p = start_.node_; p <= finish_.node_; ++p) {
                    if (*p) alloc_.deallocate(*p, block_size());
                }
                deallocate_map();
            }
            map_ = other.map_;
            map_size_ = other.map_size_;
            start_ = other.start_;
            finish_ = other.finish_;
            other.map_ = nullptr;
            other.map_size_ = 0;
            other.init_map(0);
        }
        return *this;
    }

    deque& operator=(std::initializer_list<T> il) {
        assign(il);
        return *this;
    }

    // ============================================================
    // assign
    // ============================================================
    void assign(size_type count, const T& value) {
        clear();
        for (size_type i = 0; i < count; ++i) {
            push_back(value);
        }
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    void assign(InputIterator first, InputIterator last) {
        clear();
        for (; first != last; ++first) {
            push_back(*first);
        }
    }

    void assign(std::initializer_list<T> il) {
        clear();
        for (const auto& v : il) {
            push_back(v);
        }
    }

    // ============================================================
    // Element access
    // ============================================================
    reference operator[](size_type n) { return start_[static_cast<difference_type>(n)]; }
    const_reference operator[](size_type n) const { return start_[static_cast<difference_type>(n)]; }

    reference at(size_type n) {
        if (n >= size()) throw std::out_of_range("deque::at: index out of range");
        return (*this)[n];
    }
    const_reference at(size_type n) const {
        if (n >= size()) throw std::out_of_range("deque::at: index out of range");
        return (*this)[n];
    }

    reference front() { return *start_; }
    const_reference front() const { return *start_; }
    reference back() {
        iterator tmp = finish_;
        --tmp;
        return *tmp;
    }
    const_reference back() const {
        iterator tmp = finish_;
        --tmp;
        return *tmp;
    }

    // ============================================================
    // Iterators
    // ============================================================
    iterator begin() noexcept { return start_; }
    const_iterator begin() const noexcept { return start_; }
    const_iterator cbegin() const noexcept { return start_; }
    iterator end() noexcept { return finish_; }
    const_iterator end() const noexcept { return finish_; }
    const_iterator cend() const noexcept { return finish_; }

    reverse_iterator rbegin() noexcept { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(end()); }
    const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(end()); }
    reverse_iterator rend() noexcept { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const noexcept { return const_reverse_iterator(begin()); }
    const_reverse_iterator crend() const noexcept { return const_reverse_iterator(begin()); }

    // ============================================================
    // Capacity
    // ============================================================
    bool empty() const noexcept { return start_ == finish_; }
    size_type size() const noexcept { return static_cast<size_type>(finish_ - start_); }
    size_type max_size() const noexcept { return static_cast<size_type>(-1) / sizeof(T); }

    void shrink_to_fit() {
        // Not commonly implemented for deques; no-op for now
        // Could reallocate map to be minimal size
    }

    // ============================================================
    // push_front / emplace_front
    // ============================================================
    void push_front(const T& value) {
        emplace_front(value);
    }

    void push_front(T&& value) {
        emplace_front(zstl::move(value));
    }

    template<typename... Args>
    reference emplace_front(Args&&... args) {
        reserve_front();
        --start_.cur_;
        try {
            construct(start_.cur_, zstl::forward<Args>(args)...);
        } catch (...) {
            ++start_.cur_;
            throw;
        }
        return front();
    }

    // ============================================================
    // push_back / emplace_back
    // ============================================================
    void push_back(const T& value) {
        emplace_back(value);
    }

    void push_back(T&& value) {
        emplace_back(zstl::move(value));
    }

    template<typename... Args>
    reference emplace_back(Args&&... args) {
        reserve_back();
        try {
            construct(finish_.cur_, zstl::forward<Args>(args)...);
        } catch (...) {
            // Reserve already committed; rollback? Reserve is cheap, no-op rollback
            throw;
        }
        ++finish_.cur_;
        return back();
    }

    // ============================================================
    // pop_front / pop_back
    // ============================================================
    void pop_front() {
        destroy(start_.cur_);
        ++start_.cur_;
        if (start_.cur_ == start_.last_) {
            // Free the (now empty) block
            if (start_.node_ < finish_.node_) {
                deallocate_block(*start_.node_);
                *start_.node_ = nullptr;
            }
            start_.set_node(start_.node_ + 1);
            start_.cur_ = start_.first_;
        }
    }

    void pop_back() {
        if (finish_.cur_ == finish_.first_) {
            // Back block is empty; move to previous
            if (finish_.node_ > start_.node_) {
                deallocate_block(*finish_.node_);
                *finish_.node_ = nullptr;
            }
            finish_.set_node(finish_.node_ - 1);
            finish_.cur_ = finish_.last_;
        }
        --finish_.cur_;
        destroy(finish_.cur_);
    }

    // ============================================================
    // insert
    // ============================================================
    iterator insert(const_iterator pos, const T& value) {
        return emplace(pos, value);
    }

    iterator insert(const_iterator pos, T&& value) {
        return emplace(pos, zstl::move(value));
    }

    iterator insert(const_iterator pos, size_type count, const T& value) {
        if (count == 0) return iterator(const_cast<T*>(pos.cur_),
                                         const_cast<T**>(pos.node_));
        iterator result;
        // Insert closer to front or back for efficiency
        difference_type dist_to_front = pos - start_;
        difference_type dist_to_back = finish_ - pos;
        if (dist_to_front < dist_to_back) {
            // Push front elements, then insert
            result = start_ + dist_to_front;
            for (size_type i = 0; i < count; ++i) {
                push_front(value);
            }
            std::rotate(begin(), begin() + count, begin() + count + dist_to_front);
        } else {
            result = start_ + dist_to_front;
            for (size_type i = 0; i < count; ++i) {
                push_back(value);
            }
            std::rotate(begin() + dist_to_front, end() - count, end());
        }
        return result;
    }

    template<typename InputIterator,
             typename = enable_if_t<detail::is_input_iterator_v<InputIterator>>>
    iterator insert(const_iterator pos, InputIterator first, InputIterator last) {
        if (first == last) return iterator(const_cast<T*>(pos.cur_),
                                            const_cast<T**>(pos.node_));
        difference_type offset = pos - start_;
        size_type old_size = size();
        for (; first != last; ++first) {
            push_back(*first);
        }
        std::rotate(begin() + offset, begin() + old_size, end());
        return begin() + offset;
    }

    iterator insert(const_iterator pos, std::initializer_list<T> il) {
        return insert(pos, il.begin(), il.end());
    }

    // ============================================================
    // emplace
    // ============================================================
    template<typename... Args>
    iterator emplace(const_iterator pos, Args&&... args) {
        difference_type offset = pos - start_;
        if (offset == 0) {
            emplace_front(zstl::forward<Args>(args)...);
            return begin();
        }
        if (static_cast<size_type>(offset) == size()) {
            emplace_back(zstl::forward<Args>(args)...);
            return end() - 1;
        }
        // Insert in middle: push front + rotate to minimize moves
        emplace_front(zstl::forward<Args>(args)...);
        std::rotate(begin(), begin() + 1, begin() + offset + 1);
        return begin() + offset;
    }

    // ============================================================
    // erase
    // ============================================================
    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        if (first == last) return iterator(const_cast<T*>(first.cur_),
                                            const_cast<T**>(first.node_));
        difference_type dist_to_front = first - start_;
        difference_type count = last - first;

        if (dist_to_front < (finish_ - last)) {
            // Move front elements forward
            std::copy_backward(begin(), begin() + dist_to_front,
                               begin() + dist_to_front + count);
            for (size_type i = 0; i < static_cast<size_type>(count); ++i) {
                pop_front();
            }
            return begin() + dist_to_front;
        } else {
            // Move back elements backward
            std::copy(begin() + dist_to_front + count, end(),
                      begin() + dist_to_front);
            for (size_type i = 0; i < static_cast<size_type>(count); ++i) {
                pop_back();
            }
            return begin() + dist_to_front;
        }
    }

    // ============================================================
    // resize
    // ============================================================
    void resize(size_type count) {
        resize(count, T());
    }

    void resize(size_type count, const T& value) {
        if (count < size()) {
            while (size() > count) pop_back();
        } else if (count > size()) {
            while (size() < count) push_back(value);
        }
    }

    // ============================================================
    // clear
    // ============================================================
    void clear() noexcept {
        // Destroy elements in the middle blocks
        for (map_pointer p = start_.node_ + 1; p < finish_.node_; ++p) {
            destroy_range(*p, *p + block_size());
        }
        // Destroy partial blocks at the ends
        if (start_.node_ == finish_.node_) {
            destroy_range(start_.cur_, finish_.cur_);
        } else {
            destroy_range(start_.cur_, start_.last_);
            destroy_range(finish_.first_, finish_.cur_);
        }

        // Reset to initial state
        if (map_) {
            // Free blocks except one middle block
            map_pointer start_node = map_ + (map_size_ / 2);
            if (start_.node_ != start_node) {
                // Free current blocks and allocate one in middle
                for (map_pointer p = start_.node_; p <= finish_.node_; ++p) {
                    if (*p) {
                        alloc_.deallocate(*p, block_size());
                        *p = nullptr;
                    }
                }
                *start_node = allocate_block();
            }
            start_.set_node(start_node);
            start_.cur_ = start_.first_;
            finish_.set_node(start_node);
            finish_.cur_ = finish_.first_;
        }
    }

    // ============================================================
    // swap
    // ============================================================
    void swap(deque& other) noexcept {
        zstl::swap(map_, other.map_);
        zstl::swap(map_size_, other.map_size_);
        zstl::swap(start_, other.start_);
        zstl::swap(finish_, other.finish_);
    }
};

// ============================================================
// Non-member swap
// ============================================================
template<typename T, typename Alloc>
void swap(deque<T, Alloc>& a, deque<T, Alloc>& b) noexcept {
    a.swap(b);
}

} // namespace zstl
