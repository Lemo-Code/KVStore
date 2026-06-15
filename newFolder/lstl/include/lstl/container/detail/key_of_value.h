/**
 * @file    key_of_value.h
 * @brief   Key extraction policies for associative containers (identity and select1st).
 * @author  lstl team
 * @date    2025
 * @ingroup container_detail
 */
// Use of this source code is governed by a MIT-style license.
//
// key_of_value.h - Key extraction policies for associative containers.

#pragma once

#include "../../memory/functional.h"

namespace lstl {
namespace detail {

////////////////////////////////////////////////////////////////////////////
// Identity key extractor - value IS the key (for set types)
////////////////////////////////////////////////////////////////////////////
template <typename T>
struct identity_key {
    const T& operator()(const T& x) const { return x; }
};

////////////////////////////////////////////////////////////////////////////
// First-of-pair key extractor - for map types (key is pair::first)
////////////////////////////////////////////////////////////////////////////
template <typename Pair>
struct select1st_key {
    const typename Pair::first_type& operator()(const Pair& x) const {
        return x.first;
    }
};

////////////////////////////////////////////////////////////////////////////
// key_of_value helper - picks the right extractor based on container type
////////////////////////////////////////////////////////////////////////////
template <typename Value, bool IsMap>
struct key_of_value_impl {
    // Default: value is the key (set behavior)
    typedef identity_key<Value> type;
};

template <typename Value>
struct key_of_value_impl<Value, true> {
    // Map: value is a pair, key is pair.first
    typedef select1st_key<Value> type;
};

////////////////////////////////////////////////////////////////////////////
// key_of_value - Trait to get key extractor for a value type
// Usage:
//   using key_extract = typename key_of_value<value_type, /*IsMap=*/true>::type;
////////////////////////////////////////////////////////////////////////////
template <typename Value, bool IsMap>
using key_of_value = typename key_of_value_impl<Value, IsMap>::type;

} // namespace detail
} // namespace lstl
