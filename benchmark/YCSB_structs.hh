#pragma once

#include <string>
#include <random>

#include "DB_structs.hh" // bench::fix_string
#include "str.hh" // lcdf::Str
#include "Interface.hh"
#include "DB_index.hh" // bench::get_version; bench::ordered_index; bench::version_adapter

namespace ycsb {

using bench::fix_string;
using bench::get_version;
using bench::version_adapter;

enum class mode_id : int { ReadOnly = 0, MediumContention, HighContention };

struct ycsb_key {
    ycsb_key(uint64_t id) {
        // no scan operations for ycsb,
        // so byte swap is not required.
        w_id = id;
    }
    bool operator==(const ycsb_key& other) const {
        return w_id == other.w_id;
    }
    bool operator!=(const ycsb_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t w_id;
};

struct ycsb_half_value {
    static constexpr size_t col_width = 100;
    static constexpr size_t num_cols = 5;
    enum class NamedColumn : int {
        odd_columns = 0
    };

    typedef fix_string<col_width> col_type;
    col_type cols[num_cols];
};

struct ycsb_value {
    static constexpr size_t col_width = 100;
    static constexpr size_t num_cols = 10;
    enum class NamedColumn : int {
        odd_columns = 0,
        even_columns
    };

    typedef fix_string<col_width> col_type;
    col_type cols[num_cols];
};

class ycsb_input_generator {
public:
    ycsb_input_generator(int thread_id)
            : gen(thread_id), dis(0, 61) {}

    template <typename value_type>
    value_type random_ycsb_value() {
        value_type ret;
        for (size_t i = 0; i < value_type::num_cols; i++) {
            ret.cols[i] = random_a_string(value_type::col_width);
        }
        return ret;
    }

    std::mt19937& generator() {
        return gen;
    }

    void random_ycsb_col_value_inplace(ycsb_value::col_type *dst) {
        for (size_t i = 0; i < ycsb_value::col_width; ++i)
            (*dst)[i] = random_char();
    }

private:
    std::string random_a_string(size_t len) {
        std::string str;
        str.reserve(len);
        for (auto i = 0u; i < len; ++i) {
            str.push_back(random_char());
        }
        return str;
    }

    char random_char() {
        auto n = dis(gen);
        char c = (n < 26) ? char('a' + n) :
                 ((n < 52) ? char('A' + (n - 26)) : char('0' + (n - 52)));
        return c;
    }

    std::mt19937 gen;
    std::uniform_int_distribution<int> dis;
};

}; // namespace ycsb

namespace std {

template <>
struct hash<ycsb::ycsb_key> {
    size_t operator() (const ycsb::ycsb_key& arg) const {
        return arg.w_id;
    }
};

};
