/**
 * @file    vector.h
 * @brief   Dynamic array with amortized O(1) push_back.
 * @author  lstl team
 * @date    2025
 *
 * lstl::vector is a contiguous-memory container providing:
 * - O(1) random access via operator[]
 * - Amortized O(1) push_back (growth factor 2x)
 * - O(n) insert/erase at arbitrary positions
 * - Custom allocator support via simple_alloc
 *
 * Memory grows exponentially (2x) when capacity is exhausted,
 * achieving amortized O(1) push_back. POD types use memmove
 * for bulk relocation during reallocation.
 *
 * @tparam T     Element type.
 * @tparam Alloc Allocator type (default: lstl::allocator<T>).
 *
 * @ingroup container
 */

#pragma once

#include <cstddef>
#include <iterator>
#include <algorithm>
#include <initializer_list>
#include <stdexcept>

#include "../memory/type_traits.h"
#include "../memory/utility.h"
#include <type_traits>
#include "../memory/construct.h"
#include "../memory/uninitialized.h"
#include "../memory/allocator.h"
#include "../memory/alloc.h"
#include "../memory/pool.h"
#include "detail/reverse_iterator.h"

namespace lstl {

/**
 * @brief  Dynamic contiguous array.
 *
 * Provides a resizable array with the performance characteristics
 * of a C array plus automatic memory management.
 *
 * @tparam T     Element type (must be at least MoveConstructible and MoveAssignable).
 * @tparam Alloc Allocator type for memory management.
 *
 * @invariant  size() <= capacity()
 * @invariant  [start_, finish_) are constructed; [finish_, end_of_storage_) are uninitialized.
 *
 * @note  Iterators are raw pointers. They are invalidated by any operation
 *        that changes capacity() (push_back, insert, reserve, shrink_to_fit).
 */
template <typename T, typename Alloc = allocator<T>>
class vector {
public:
    // ---- Standard typedefs ----
    typedef T                                           value_type;       ///< Element type.
    typedef T*                                          pointer;          ///< Pointer to element.
    typedef const T*                                    const_pointer;    ///< Const pointer.
    typedef T&                                          reference;        ///< Reference to element.
    typedef const T&                                    const_reference;  ///< Const reference.
    typedef size_t                                      size_type;        ///< Size type.
    typedef ptrdiff_t                                   difference_type;  ///< Pointer difference type.
    typedef T*                                          iterator;         ///< Random-access iterator (raw pointer).
    typedef const T*                                    const_iterator;   ///< Const random-access iterator.
    typedef detail::reverse_iterator<iterator>           reverse_iterator;       ///< Reverse iterator.
    typedef detail::reverse_iterator<const_iterator>     const_reverse_iterator; ///< Const reverse iterator.
    typedef Alloc                                       allocator_type;   ///< Allocator type.

private:
    /** @brief  The underlying allocator for element memory. */
    typedef simple_alloc<T, default_alloc> data_allocator;

    iterator start_;           ///< Beginning of used (constructed) space.
    iterator finish_;          ///< One past the last constructed element.
    iterator end_of_storage_;  ///< End of allocated (capacity) space.

    /**
     * @brief  Inserts an element at @p pos when the vector needs to grow.
     *
     * Allocates new memory, copies/moves existing elements,
     * destroys old allocation, and updates internal pointers.
     * Provides strong exception safety guarantee.
     *
     * @param  pos    Insertion position.
     * @param  value  Value to insert.
     *
     * @throws  std::bad_alloc if allocation fails.
     * @throws  Re-throws from element copy constructors (after rollback).
     */
    void insert_aux(iterator pos, const T& value) {
        if (finish_ != end_of_storage_) {
            // Room exists at end: shift tail and place
            construct(finish_, *(finish_ - 1));
            ++finish_;
            std::copy_backward(pos, finish_ - 2, finish_ - 1);
            *pos = value;
        } else {
            // Must grow allocation
            size_type old_size = size();

            if (std::is_trivially_copyable<T>::value) {
                // POD fast path: realloc (可能原地扩展, 零拷贝)
                size_type new_cap = old_size != 0 ? 2 * old_size : 1;
                size_type pos_offset = static_cast<size_type>(pos - start_);
                iterator new_start = data_allocator::reallocate(start_, capacity(), new_cap);
                finish_ = new_start + old_size;
                start_ = new_start;
                end_of_storage_ = new_start + new_cap;
                pos = new_start + pos_offset;
                // Shift and insert
                if (pos == finish_) {
                    // push_back: no shift, just append
                    *finish_ = value;
                    ++finish_;
                } else {
                    construct(finish_, *(finish_ - 1));
                    ++finish_;
                    std::memmove(pos + 1, pos, (finish_ - 1 - pos) * sizeof(T));
                    *pos = value;
                }
            } else {
                // Non-POD path: allocate + copy-construct + destroy + free
                size_type new_cap = old_size != 0 ? 2 * old_size : 1;
                iterator new_start = data_allocator::allocate(new_cap);
                iterator new_finish = new_start;

                try {
                    new_finish = lstl::uninitialized_copy(start_, pos, new_start);
                    construct(new_finish, value);
                    ++new_finish;
                    new_finish = lstl::uninitialized_copy(pos, finish_, new_finish);
                } catch (...) {
                    lstl::destroy(new_start, new_finish);
                    data_allocator::deallocate(new_start, new_cap);
                    throw;
                }

                lstl::destroy(start_, finish_);
                data_allocator::deallocate(start_, capacity());

                start_ = new_start;
                finish_ = new_finish;
                end_of_storage_ = new_start + new_cap;
            }
        }
    }

public:
    // =====================================================================
    // Construction / Destruction
    // =====================================================================

    /// @brief  Default constructor — creates an empty vector.
    vector() : start_(nullptr), finish_(nullptr), end_of_storage_(nullptr) {}

    /**
     * @brief  Constructs a vector with @p n copies of @p value.
     * @param  n      Initial size.
     * @param  value  Value to fill with (default: T()).
     */
    explicit vector(size_type n, const T& value = T()) {
        start_ = data_allocator::allocate(n);
        finish_ = start_;
        end_of_storage_ = start_ + n;
        lstl::uninitialized_fill_n(start_, n, value);
        finish_ = start_ + n;
    }

    /**
     * @brief  Constructs a vector from an iterator range [first, last).
     * @tparam InputIterator  Iterator type (deduced; SFINAE-disabled for integral types).
     * @param  first, last     Range to copy from.
     */
    template <typename InputIterator,
              typename = typename enable_if<
                  !is_integral<InputIterator>::value
              >::type>
    vector(InputIterator first, InputIterator last) {
        size_type n = static_cast<size_type>(std::distance(first, last));
        start_ = data_allocator::allocate(n);
        finish_ = start_;
        end_of_storage_ = start_ + n;
        finish_ = lstl::uninitialized_copy(first, last, start_);
    }

    /**
     * @brief  Constructs from an initializer list.
     * @param  il  Initializer list of elements.
     */
    vector(std::initializer_list<T> il) : vector(il.begin(), il.end()) {}

    /**
     * @brief  Copy constructor — deep copies all elements.
     * @param  other  Source vector.
     */
    vector(const vector& other) {
        size_type n = other.size();
        start_ = data_allocator::allocate(n != 0 ? n : 1);
        finish_ = lstl::uninitialized_copy(other.start_, other.finish_, start_);
        end_of_storage_ = start_ + (n != 0 ? n : 1);
    }

    /**
     * @brief  Move constructor — transfers ownership of memory.
     *
     * O(1) operation. The source vector is left empty.
     *
     * @param  other  Source vector (will be emptied).
     */
    vector(vector&& other) noexcept
        : start_(other.start_)
        , finish_(other.finish_)
        , end_of_storage_(other.end_of_storage_) {
        other.start_ = other.finish_ = other.end_of_storage_ = nullptr;
    }

    /**
     * @brief  Destructor — destroys all elements and frees memory.
     */
    ~vector() {
        lstl::destroy(start_, finish_);
        data_allocator::deallocate(start_, capacity());
    }

    /**
     * @brief  Copy assignment — copy-swap idiom.
     * @param  other  Source vector.
     * @return        Reference to this vector.
     */
    vector& operator=(const vector& other) {
        if (this != &other) {
            size_type other_sz = other.size();
            if (other_sz > capacity()) {
                vector tmp(other);
                swap(tmp);
            } else {
                size_type min_sz = size() < other_sz ? size() : other_sz;
                std::copy(other.start_, other.start_ + min_sz, start_);
                if (size() > other_sz) {
                    lstl::destroy(start_ + other_sz, finish_);
                } else {
                    lstl::uninitialized_copy(other.start_ + size(), other.finish_, finish_);
                }
                finish_ = start_ + other_sz;
            }
        }
        return *this;
    }

    /**
     * @brief  Move assignment — transfers ownership.
     * @param  other  Source vector (will be emptied).
     * @return        Reference to this vector.
     */
    vector& operator=(vector&& other) noexcept {
        if (this != &other) {
            lstl::destroy(start_, finish_);
            data_allocator::deallocate(start_, capacity());
            start_ = other.start_;
            finish_ = other.finish_;
            end_of_storage_ = other.end_of_storage_;
            other.start_ = other.finish_ = other.end_of_storage_ = nullptr;
        }
        return *this;
    }

    // =====================================================================
    // Element Access
    // =====================================================================

    /**
     * @brief  Accesses the element at index @p n (no bounds check).
     * @param  n  Index (0-based).
     * @return    Reference to the nth element.
     * @pre       0 <= n < size()
     */
    reference operator[](size_type n) { return start_[n]; }

    /// @copydoc operator[]
    const_reference operator[](size_type n) const { return start_[n]; }

    /**
     * @brief  Accesses the element at index @p n with bounds checking.
     * @param  n  Index.
     * @return    Reference to the nth element.
     * @throws    std::out_of_range if n >= size().
     */
    reference at(size_type n) {
        if (n >= size()) throw std::out_of_range("vector::at");
        return start_[n];
    }

    /// @copydoc at
    const_reference at(size_type n) const {
        if (n >= size()) throw std::out_of_range("vector::at");
        return start_[n];
    }

    /// @brief  Returns a reference to the first element.
    reference front() { return *start_; }
    /// @copydoc front
    const_reference front() const { return *start_; }

    /// @brief  Returns a reference to the last element.
    reference back() { return *(finish_ - 1); }
    /// @copydoc back
    const_reference back() const { return *(finish_ - 1); }

    /// @brief  Returns a raw pointer to the underlying array.
    pointer data() { return start_; }
    /// @copydoc data
    const_pointer data() const { return start_; }

    // =====================================================================
    // Iterators
    // =====================================================================

    /// @name Forward iteration
    /// @{
    iterator begin() { return start_; }
    const_iterator begin() const { return start_; }
    const_iterator cbegin() const { return start_; }
    iterator end() { return finish_; }
    const_iterator end() const { return finish_; }
    const_iterator cend() const { return finish_; }
    /// @}

    /// @name Reverse iteration
    /// @{
    reverse_iterator rbegin() { return reverse_iterator(end()); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator(end()); }
    reverse_iterator rend() { return reverse_iterator(begin()); }
    const_reverse_iterator rend() const { return const_reverse_iterator(begin()); }
    /// @}

    // =====================================================================
    // Capacity
    // =====================================================================

    /**
     * @brief  Returns the number of elements.
     * @return  finish_ - start_
     */
    size_type size() const { return static_cast<size_type>(finish_ - start_); }

    /**
     * @brief  Returns the total number of elements that can be held
     *         without reallocation.
     * @return  end_of_storage_ - start_
     */
    size_type capacity() const { return static_cast<size_type>(end_of_storage_ - start_); }

    /**
     * @brief  Returns true if the vector is empty.
     * @return  size() == 0
     */
    bool empty() const { return start_ == finish_; }

    /**
     * @brief  Ensures capacity is at least @p n.
     *
     * If n > capacity(), reallocates storage. Does nothing otherwise.
     * Invalidates all iterators if reallocation occurs.
     *
     * @param  n  Minimum desired capacity.
     * @throws std::bad_alloc if allocation fails.
     */
    void reserve(size_type n) {
        if (n > capacity()) {
            if (std::is_trivially_copyable<T>::value) {
                // POD fast path: realloc (可能原地扩展)
                iterator new_start = data_allocator::reallocate(start_, capacity(), n);
                finish_ = new_start + size();
                start_ = new_start;
                end_of_storage_ = new_start + n;
            } else {
                iterator new_start = data_allocator::allocate(n);
                iterator new_finish;
                try {
                    new_finish = lstl::uninitialized_copy(start_, finish_, new_start);
                } catch (...) {
                    data_allocator::deallocate(new_start, n);
                    throw;
                }
                lstl::destroy(start_, finish_);
                data_allocator::deallocate(start_, capacity());
                start_ = new_start;
                finish_ = new_finish;
                end_of_storage_ = start_ + n;
            }
        }
    }

    /**
     * @brief  Reduces capacity to match size.
     *
     * Frees excess memory. Invalidates all iterators.
     */
    void shrink_to_fit() {
        if (finish_ < end_of_storage_) {
            vector tmp(*this);
            swap(tmp);
        }
    }

    // =====================================================================
    // Modifiers
    // =====================================================================

    /**
     * @brief  Appends an element to the end.
     *
     * Amortized O(1). If capacity() == size(), triggers reallocation
     * (2x growth factor).
     *
     * @param  value  Element to copy-construct at the end.
     */
    void push_back(const T& value) {
        if (finish_ != end_of_storage_) { construct(finish_, value); ++finish_; }
        else { insert_aux(end(), value); }
    }

    void push_back(T&& value) {
        if (finish_ != end_of_storage_) { construct(finish_, lstl::move(value)); ++finish_; }
        else { insert_aux(end(), lstl::move(value)); }
    }

    /**
     * @brief  Removes the last element.
     *
     * O(1). The element is destroyed but memory is not freed.
     *
     * @pre  !empty()
     */
    void pop_back() {
        --finish_;
        lstl::destroy(finish_);
    }

    /**
     * @brief  Inserts @p value before @p pos.
     *
     * O(n) where n = distance(pos, end()).
     *
     * @param  pos    Iterator before which to insert.
     * @param  value  Value to insert.
     * @return        Iterator pointing to the inserted element.
     */
    iterator insert(const_iterator pos, T&& value) {
        difference_type offset = pos - cbegin();
        if (finish_ != end_of_storage_) { construct(finish_, lstl::move(*(finish_ - 1))); ++finish_; std::move_backward(begin() + offset, finish_ - 2, finish_ - 1); *(begin() + offset) = lstl::move(value); }
        else { insert_aux(begin() + offset, value); }
        return begin() + offset;
    }
    iterator insert(const_iterator pos, const T& value) {
        difference_type offset = pos - cbegin();
        insert_aux(begin() + offset, value);
        return begin() + offset;
    }

    /**
     * @brief  Erases the element at @p pos.
     *
     * O(n) where n = distance(pos, end()).
     *
     * @param  pos  Iterator to the element to erase.
     * @return      Iterator following the erased element.
     */
    iterator erase(const_iterator pos) {
        difference_type offset = pos - cbegin();
        std::copy(begin() + offset + 1, end(), begin() + offset);
        pop_back();
        return begin() + offset;
    }

    /**
     * @brief  Destroys all elements without freeing memory.
     *
     * O(size()). After clear(), size() == 0 but capacity() is unchanged.
     */
    void clear() {
        lstl::destroy(start_, finish_);
        finish_ = start_;
    }

    /**
     * @brief  Resizes the vector to contain @p n elements.
     *
     * If n < size(), truncates. If n > size(), appends copies of @p value.
     * If n > capacity(), triggers reallocation.
     *
     * @param  n      New size.
     * @param  value  Value for new elements (default: T()).
     */
    void resize(size_type n, const T& value = T()) {
        if (n < size()) {
            lstl::destroy(start_ + n, finish_);
            finish_ = start_ + n;
        } else if (n > size()) {
            if (n > capacity()) reserve(n);
            lstl::uninitialized_fill(finish_, start_ + n, value);
            finish_ = start_ + n;
        }
    }

    /**
     * @brief  Swaps contents with another vector (O(1), noexcept).
     * @param  other  Vector to swap with.
     */
    void swap(vector& other) noexcept {
        lstl::swap(start_, other.start_);
        lstl::swap(finish_, other.finish_);
        lstl::swap(end_of_storage_, other.end_of_storage_);
    }
};

} // namespace lstl
