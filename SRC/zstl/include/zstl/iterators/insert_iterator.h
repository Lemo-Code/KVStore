// zstl insert iterators — output iterators that insert into containers
// ============================================================
// Three types of insert iterators (all satisfy output_iterator):
//   1. back_insert_iterator  — calls container.push_back(value)
//   2. front_insert_iterator — calls container.push_front(value)
//   3. insert_iterator       — calls container.insert(pos, value) and advances pos
//
// Usage:
//   zstl::copy(src.begin(), src.end(), zstl::back_inserter(dst));
//   zstl::copy(src.begin(), src.end(), zstl::front_inserter(dst));
//   zstl::copy(src.begin(), src.end(), zstl::inserter(dst, dst.begin()));
//
// All operator* and operator++ return *this (no-op), because
// output iterators are single-pass: each assignment writes.
// ============================================================
#pragma once

#include "zstl/iterators/iterator_traits.h"

namespace zstl {

// ============================================================
// back_insert_iterator<Container>
// Appends elements via container.push_back().
// Satisfies output_iterator requirements.
// ============================================================
template<typename Container>
class back_insert_iterator {
public:
    using iterator_category = output_iterator_tag;
    using value_type        = void;
    using difference_type   = void;
    using pointer           = void;
    using reference         = void;
    using container_type    = Container;

    explicit back_insert_iterator(Container& c) noexcept : container_(&c) {}

    back_insert_iterator(const back_insert_iterator&) = default;
    back_insert_iterator& operator=(const back_insert_iterator&) = default;

    // Assignment inserts via push_back
    back_insert_iterator& operator=(const typename Container::value_type& value) {
        container_->push_back(value);
        return *this;
    }

    back_insert_iterator& operator=(typename Container::value_type&& value) {
        container_->push_back(zstl::move(value));
        return *this;
    }

    // Dereference returns *this (no-op, required by output_iterator concept)
    constexpr back_insert_iterator& operator*() noexcept { return *this; }
    constexpr back_insert_iterator& operator++() noexcept { return *this; }
    constexpr back_insert_iterator& operator++(int) noexcept { return *this; }

    // Accessor for the underlying container
    Container& container() const noexcept { return *container_; }

protected:
    Container* container_;
};

// ============================================================
// back_inserter — convenience factory for back_insert_iterator
// ============================================================
template<typename Container>
back_insert_iterator<Container> back_inserter(Container& c) noexcept {
    return back_insert_iterator<Container>(c);
}

// ============================================================
// front_insert_iterator<Container>
// Prepends elements via container.push_front().
// Satisfies output_iterator requirements.
// ============================================================
template<typename Container>
class front_insert_iterator {
public:
    using iterator_category = output_iterator_tag;
    using value_type        = void;
    using difference_type   = void;
    using pointer           = void;
    using reference         = void;
    using container_type    = Container;

    explicit front_insert_iterator(Container& c) noexcept : container_(&c) {}

    front_insert_iterator(const front_insert_iterator&) = default;
    front_insert_iterator& operator=(const front_insert_iterator&) = default;

    // Assignment inserts via push_front
    front_insert_iterator& operator=(const typename Container::value_type& value) {
        container_->push_front(value);
        return *this;
    }

    front_insert_iterator& operator=(typename Container::value_type&& value) {
        container_->push_front(zstl::move(value));
        return *this;
    }

    // Dereference returns *this (no-op)
    constexpr front_insert_iterator& operator*() noexcept { return *this; }
    constexpr front_insert_iterator& operator++() noexcept { return *this; }
    constexpr front_insert_iterator& operator++(int) noexcept { return *this; }

    // Accessor for the underlying container
    Container& container() const noexcept { return *container_; }

protected:
    Container* container_;
};

// ============================================================
// front_inserter — convenience factory for front_insert_iterator
// ============================================================
template<typename Container>
front_insert_iterator<Container> front_inserter(Container& c) noexcept {
    return front_insert_iterator<Container>(c);
}

// ============================================================
// insert_iterator<Container>
// Inserts elements via container.insert(pos, value) and increments pos.
// Satisfies output_iterator requirements.
// ============================================================
template<typename Container>
class insert_iterator {
public:
    using iterator_category = output_iterator_tag;
    using value_type        = void;
    using difference_type   = void;
    using pointer           = void;
    using reference         = void;
    using container_type    = Container;

    insert_iterator(Container& c, typename Container::iterator iter) noexcept
        : container_(&c), iter_(iter) {}

    insert_iterator(const insert_iterator&) = default;
    insert_iterator& operator=(const insert_iterator&) = default;

    // Assignment inserts at current position and advances
    insert_iterator& operator=(const typename Container::value_type& value) {
        iter_ = container_->insert(iter_, value);
        ++iter_;
        return *this;
    }

    insert_iterator& operator=(typename Container::value_type&& value) {
        iter_ = container_->insert(iter_, zstl::move(value));
        ++iter_;
        return *this;
    }

    // Dereference returns *this (no-op)
    constexpr insert_iterator& operator*() noexcept { return *this; }
    constexpr insert_iterator& operator++() noexcept { return *this; }
    constexpr insert_iterator& operator++(int) noexcept { return *this; }

    // Accessor for the underlying container
    Container& container() const noexcept { return *container_; }

protected:
    Container* container_;
    typename Container::iterator iter_;
};

// ============================================================
// inserter — convenience factory for insert_iterator
// ============================================================
template<typename Container>
insert_iterator<Container> inserter(Container& c, typename Container::iterator iter) noexcept {
    return insert_iterator<Container>(c, iter);
}

} // namespace zstl
