/**
 * @file    noncopyable.h
 * @brief   Non-copyable base class.
 * @ingroup base
 */
#pragma once

namespace zero {

class noncopyable {
public:
    noncopyable() = default;
    ~noncopyable() = default;
    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;
};

} // namespace zero
