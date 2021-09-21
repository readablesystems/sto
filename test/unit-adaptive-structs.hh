#include <variant>

struct index_key {
    static constexpr int32_t key_1_min = 1;
    static constexpr int32_t key_1_max = 65535;
    static constexpr int32_t key_2_min = 1;
    static constexpr int32_t key_2_max = 65535;

    index_key() = default;
    index_key(int32_t k1, int32_t k2)
            : key_1(bench::bswap(k1)), key_2(bench::bswap(k2)) {
        assert(k1 >= key_1_min);
        assert(k1 <= key_1_max);
        assert(k2 >= key_2_min);
        assert(k2 <= key_2_max);
    }
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

namespace std {

static constexpr size_t xxh_seed = 0xfeedbeeff00dd1e5;

template <>
struct hash<index_key> {
    size_t operator()(const index_key& arg) const {
        return XXH64(&arg, sizeof(index_key), xxh_seed);
    }
};

}  // namespace std
