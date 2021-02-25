#include <variant>

struct index_key {
    index_key() = default;
    index_key(int32_t k1, int32_t k2)
        : key_1(bench::bswap(k1)), key_2(bench::bswap(k2)) {}
    bool operator==(const index_key& other) const {
        return (key_1 == other.key_1) && (key_2 == other.key_2);
    }
    bool operator!=(const index_key& other) const {
        return !(*this == other);
    }

    int32_t key_1;
    int32_t key_2;
};

#include "unit-adaptive-structs-generated.hh"
