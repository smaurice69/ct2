#ifndef CT2_BITOPS_H
#define CT2_BITOPS_H

#include <cstdint>
#if defined(_MSC_VER)
#  include <intrin.h>
#endif

namespace ct2 {

// Portable population count for 64-bit integers
inline int popcount64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(x);
#elif defined(_MSC_VER)
    return static_cast<int>(__popcnt64(x));
#else
    int count = 0;
    while (x) {
        x &= x - 1;
        ++count;
    }
    return count;
#endif
}

// Portable count trailing zeros for 64-bit integers
inline int ctz64(uint64_t x) {
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(x);
#elif defined(_MSC_VER)
    unsigned long index;
    _BitScanForward64(&index, x);
    return static_cast<int>(index);
#else
    if (x == 0) return 64;
    int n = 0;
    while ((x & 1) == 0) {
        x >>= 1;
        ++n;
    }
    return n;
#endif
}

} // namespace ct2

#endif // CT2_BITOPS_H

