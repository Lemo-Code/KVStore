// zstl bitset — fixed-size bitmap with bitwise operations
//
// bitset<N> stores N bits as an array of uint64_t words.
// Bits are indexed from 0 (LSB of word 0) to N-1.
//
// Complexity:
//   - set/reset/flip/test/count/size: O(1) amortized or O(N/64)
//   - bitwise & | ^ ~: O(N/64)
//   - to_string: O(N)
//   - shift << >>: O(N/64)
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include "zstl/memory/utility.h"
#include "zstl/containers/string.h"

namespace zstl {

template<size_t N>
class bitset {
private:
    static constexpr size_t kBitsPerWord = 64;
    static constexpr size_t kNumWords = (N + kBitsPerWord - 1) / kBitsPerWord;

    using word_type = uint64_t;

    word_type words_[kNumWords];

    // Clear bits beyond N-1 in the last word
    void sanitize_last_word() noexcept {
        if constexpr (N % kBitsPerWord != 0) {
            words_[kNumWords - 1] &= ((word_type(1) << (N % kBitsPerWord)) - 1);
        }
    }

    static constexpr size_t word_index(size_t pos) noexcept {
        return pos / kBitsPerWord;
    }

    static constexpr size_t bit_index(size_t pos) noexcept {
        return pos % kBitsPerWord;
    }

public:
    // ---- reference proxy (for operator[]) ----
    class reference {
        friend class bitset;
        word_type& word_;
        word_type mask_;

        reference(word_type& w, size_t pos) noexcept
            : word_(w), mask_(word_type(1) << pos) {}

    public:
        reference& operator=(bool val) noexcept {
            if (val) word_ |= mask_;
            else word_ &= ~mask_;
            return *this;
        }

        reference& operator=(const reference& other) noexcept {
            return *this = static_cast<bool>(other);
        }

        operator bool() const noexcept { return (word_ & mask_) != 0; }

        bool operator~() const noexcept { return (word_ & mask_) == 0; }

        reference& flip() noexcept {
            word_ ^= mask_;
            return *this;
        }
    };

    // ============================================================
    // Constructors
    // ============================================================
    constexpr bitset() noexcept {
        reset();
    }

    constexpr bitset(unsigned long long val) noexcept {
        reset();
        for (size_t i = 0; i < kBitsPerWord && i < N; ++i) {
            if (val & (1ULL << i)) {
                set(i);
            }
        }
    }

    // ============================================================
    // Bit access
    // ============================================================
    // Non-const operator[] returns a proxy reference
    reference operator[](size_t pos) noexcept {
        return reference(words_[word_index(pos)], bit_index(pos));
    }

    // Const operator[] returns bool
    bool operator[](size_t pos) const noexcept {
        return (words_[word_index(pos)] >> bit_index(pos)) & word_type(1);
    }

    // ============================================================
    // Set / reset / flip / test
    // ============================================================

    // Set all bits to 1 (or the specified bit)
    bitset& set() noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            words_[i] = ~word_type(0);
        }
        sanitize_last_word();
        return *this;
    }

    bitset& set(size_t pos, bool val = true) {
        if (pos >= N) throw std::out_of_range("bitset::set: bit position out of range");
        if (val) {
            words_[word_index(pos)] |= (word_type(1) << bit_index(pos));
        } else {
            words_[word_index(pos)] &= ~(word_type(1) << bit_index(pos));
        }
        return *this;
    }

    // Reset all bits to 0 (or the specified bit)
    bitset& reset() noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            words_[i] = 0;
        }
        return *this;
    }

    bitset& reset(size_t pos) {
        return set(pos, false);
    }

    // Flip all bits (or the specified bit)
    bitset& flip() noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            words_[i] = ~words_[i];
        }
        sanitize_last_word();
        return *this;
    }

    bitset& flip(size_t pos) {
        if (pos >= N) throw std::out_of_range("bitset::flip: bit position out of range");
        words_[word_index(pos)] ^= (word_type(1) << bit_index(pos));
        return *this;
    }

    // Test if bit is set
    bool test(size_t pos) const {
        if (pos >= N) throw std::out_of_range("bitset::test: bit position out of range");
        return (words_[word_index(pos)] >> bit_index(pos)) & word_type(1);
    }

    // ============================================================
    // Count / size
    // ============================================================

    // Count bits set to 1
    size_t count() const noexcept {
        size_t n = 0;
        for (size_t i = 0; i < kNumWords; ++i) {
            n += __builtin_popcountll(words_[i]);
        }
        return n;
    }

    constexpr size_t size() const noexcept { return N; }

    // Check if all bits are set
    bool all() const noexcept {
        for (size_t i = 0; i < kNumWords - 1; ++i) {
            if (words_[i] != ~word_type(0)) return false;
        }
        if constexpr (N % kBitsPerWord == 0) {
            return words_[kNumWords - 1] == ~word_type(0);
        } else {
            word_type mask = (word_type(1) << (N % kBitsPerWord)) - 1;
            return (words_[kNumWords - 1] & mask) == mask;
        }
    }

    // Check if any bit is set
    bool any() const noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            if (words_[i] != 0) return true;
        }
        return false;
    }

    // Check if no bits are set
    bool none() const noexcept { return !any(); }

    // ============================================================
    // Conversions
    // ============================================================

    unsigned long to_ulong() const {
        for (size_t i = 1; i < kNumWords; ++i) {
            if (words_[i] != 0) {
                throw std::overflow_error("bitset::to_ulong: bitset value too large");
            }
        }
        return static_cast<unsigned long>(words_[0]);
    }

    unsigned long long to_ullong() const {
        for (size_t i = 1; i < kNumWords; ++i) {
            if (words_[i] != 0) {
                throw std::overflow_error("bitset::to_ullong: bitset value too large");
            }
        }
        return words_[0];
    }

    // to_string: convert to '0'/'1' string representation
    // MSB (bit N-1) appears first in resulting string
    string to_string(char zero = '0', char one = '1') const {
        string result;
        result.reserve(N);
        for (size_t i = N; i > 0; --i) {
            result += (*this)[i - 1] ? one : zero;
        }
        return result;
    }

    // ============================================================
    // Bitwise operations (member)
    // ============================================================

    bitset& operator&=(const bitset& other) noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            words_[i] &= other.words_[i];
        }
        return *this;
    }

    bitset& operator|=(const bitset& other) noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            words_[i] |= other.words_[i];
        }
        return *this;
    }

    bitset& operator^=(const bitset& other) noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            words_[i] ^= other.words_[i];
        }
        return *this;
    }

    bitset operator~() const noexcept {
        bitset result(*this);
        result.flip();
        return result;
    }

    // ============================================================
    // Shift operations
    // ============================================================

    bitset& operator<<=(size_t shift) noexcept {
        if (shift == 0) return *this;
        if (shift >= N) {
            reset();
            return *this;
        }

        size_t word_shift = shift / kBitsPerWord;
        size_t bit_shift = shift % kBitsPerWord;

        if (bit_shift == 0) {
            // Simple word shift
            for (size_t i = kNumWords; i > word_shift; --i) {
                words_[i - 1] = words_[i - 1 - word_shift];
            }
            for (size_t i = 0; i < word_shift && i < kNumWords; ++i) {
                words_[i] = 0;
            }
        } else {
            for (size_t i = kNumWords; i > word_shift; --i) {
                size_t src = i - 1 - word_shift;
                words_[i - 1] = words_[src] << bit_shift;
                if (src > 0) {
                    words_[i - 1] |= words_[src - 1] >> (kBitsPerWord - bit_shift);
                }
            }
            for (size_t i = 0; i < word_shift && i < kNumWords; ++i) {
                words_[i] = 0;
            }
        }
        sanitize_last_word();
        return *this;
    }

    bitset operator<<(size_t shift) const noexcept {
        bitset result(*this);
        result <<= shift;
        return result;
    }

    bitset& operator>>=(size_t shift) noexcept {
        if (shift == 0) return *this;
        if (shift >= N) {
            reset();
            return *this;
        }

        size_t word_shift = shift / kBitsPerWord;
        size_t bit_shift = shift % kBitsPerWord;

        if (bit_shift == 0) {
            // Simple word shift
            for (size_t i = 0; i < kNumWords - word_shift; ++i) {
                words_[i] = words_[i + word_shift];
            }
            for (size_t i = kNumWords - word_shift; i < kNumWords; ++i) {
                words_[i] = 0;
            }
        } else {
            for (size_t i = 0; i < kNumWords - word_shift; ++i) {
                size_t src = i + word_shift;
                words_[i] = words_[src] >> bit_shift;
                if (src + 1 < kNumWords) {
                    words_[i] |= words_[src + 1] << (kBitsPerWord - bit_shift);
                }
            }
            for (size_t i = kNumWords - word_shift; i < kNumWords; ++i) {
                words_[i] = 0;
            }
        }
        sanitize_last_word();
        return *this;
    }

    bitset operator>>(size_t shift) const noexcept {
        bitset result(*this);
        result >>= shift;
        return result;
    }

    // ============================================================
    // Comparison
    // ============================================================
    bool operator==(const bitset& other) const noexcept {
        for (size_t i = 0; i < kNumWords; ++i) {
            if (words_[i] != other.words_[i]) return false;
        }
        return true;
    }

    bool operator!=(const bitset& other) const noexcept {
        return !(*this == other);
    }
};

// ============================================================
// Non-member bitwise operators
// ============================================================

template<size_t N>
bitset<N> operator&(const bitset<N>& a, const bitset<N>& b) noexcept {
    bitset<N> result(a);
    result &= b;
    return result;
}

template<size_t N>
bitset<N> operator|(const bitset<N>& a, const bitset<N>& b) noexcept {
    bitset<N> result(a);
    result |= b;
    return result;
}

template<size_t N>
bitset<N> operator^(const bitset<N>& a, const bitset<N>& b) noexcept {
    bitset<N> result(a);
    result ^= b;
    return result;
}

} // namespace zstl
