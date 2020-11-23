#pragma once

#include <string>
#include <random>

#include "DB_structs.hh" // bench::fix_string
#include "str.hh" // lcdf::Str
#include "Interface.hh"
#include "DB_index.hh" // bench::get_version; bench::ordered_index; bench::version_adapter

namespace prcubench {

using bench::fix_string;
using bench::get_version;
using bench::version_adapter;

enum class mode_id : int { Uniform = 0, Skewed };

struct ht_key {
    ht_key(uint64_t id) {
        key = id;
    }
    bool operator==(const ht_key& other) const {
        return key == other.key;
    }
    bool operator!=(const ht_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t key;
};

struct ht_value {
    typedef uint64_t col_type;
    enum class NamedColumn : int {
        column = 0,
    };

    col_type value;
};

class ht_input_generator {
public:
    ht_input_generator(int thread_id)
            : gen(thread_id), dis(0, 61) {}

    template <typename value_type>
    value_type random_ht_value() {
        return (ht_value) { .value = dis(gen) };
    }

    std::mt19937& generator() {
        return gen;
    }

    void random_ht_col_value_inplace(ht_value::col_type *dst) {
        *dst = dis(gen);
    }

private:
    std::mt19937 gen;
    std::uniform_int_distribution<ht_value::col_type> dis;
};

}; // namespace prcu_bench

namespace std {

template <>
struct hash<prcubench::ht_key> {
    size_t operator() (const prcubench::ht_key& arg) const {
        return arg.key;
    }
};

};
