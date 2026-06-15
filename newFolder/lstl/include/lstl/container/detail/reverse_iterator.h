/**
 * @file    reverse_iterator.h
 * @brief   Reverse iterator adapter for bidirectional/random-access iterators.
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// reverse_iterator.h - Reverse iterator adapter.

#pragma once

#include <iterator>

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// reverse_iterator - Adapts any bidirectional or random-access iterator
// to traverse in reverse order.
////////////////////////////////////////////////////////////////////////////
template <typename Iterator>
class reverse_iterator {
public:
    typedef typename std::iterator_traits<Iterator>::value_type        value_type;
    typedef typename std::iterator_traits<Iterator>::pointer           pointer;
    typedef typename std::iterator_traits<Iterator>::reference         reference;
    typedef typename std::iterator_traits<Iterator>::difference_type   difference_type;
    typedef typename std::iterator_traits<Iterator>::iterator_category iterator_category;

    typedef Iterator          iterator_type;
    typedef reverse_iterator  self;

    reverse_iterator() : current_() {}
    explicit reverse_iterator(iterator_type it) : current_(it) {}

    template <typename U>
    reverse_iterator(const reverse_iterator<U>& other) : current_(other.base()) {}

    iterator_type base() const { return current_; }

    reference operator*() const {
        iterator_type tmp = current_;
        return *--tmp;
    }

    pointer operator->() const {
        return &(operator*());
    }

    self& operator++() {
        --current_;
        return *this;
    }

    self operator++(int) {
        self tmp = *this;
        --current_;
        return tmp;
    }

    self& operator--() {
        ++current_;
        return *this;
    }

    self operator--(int) {
        self tmp = *this;
        ++current_;
        return tmp;
    }

    self operator+(difference_type n) const {
        return self(current_ - n);
    }

    self& operator+=(difference_type n) {
        current_ -= n;
        return *this;
    }

    self operator-(difference_type n) const {
        return self(current_ + n);
    }

    self& operator-=(difference_type n) {
        current_ += n;
        return *this;
    }

    reference operator[](difference_type n) const {
        return *(*this + n);
    }

    bool operator==(const self& other) const { return current_ == other.current_; }
    bool operator!=(const self& other) const { return current_ != other.current_; }

protected:
    iterator_type current_;
};

} // namespace detail
} // namespace lstl
