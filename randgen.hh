#pragma once

struct Rand {
    typedef uint32_t result_type;
    result_type x;
    Rand(result_type a, result_type = 0)
        : x(a) {
    }
    result_type operator()() {
        x = 1103515245 * x + 12345;
        return x & 0x7fffffff;
    }
    static constexpr result_type min() {
        return 0;
    }
    static constexpr result_type max() {
        return 0x7fffffff;
    }
};
