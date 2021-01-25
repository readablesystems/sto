#pragma once

#include <string>
#include <random>
#include <variant>

#include "DB_structs.hh" // bench::fix_string
#include "str.hh" // lcdf::Str
#include "Interface.hh"
#include "DB_index.hh" // bench::get_version; bench::ordered_index; bench::version_adapter
#include "Adapter.hh"

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

#define COL_WIDTH 4
#define HALF_NUM_COLUMNS 5
typedef fix_string<COL_WIDTH> col_type;

namespace ycsb_value_datatypes {

enum class NamedColumn : int {
    even_columns = 0,
    odd_columns,
    COLCOUNT
};

template <size_t StartIndex, size_t EndIndex>
struct split_value;

template <size_t SplitIndex>
struct unified_value;

template <>
struct split_value<0, 1> {
    explicit split_value() = default;

    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> even_columns;
};
template <>
struct split_value<1, 2> {
    explicit split_value() = default;

    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> odd_columns;
};
template <>
struct unified_value<1> {
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() {
        return split_0.even_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
        return split_0.even_columns;
    }
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() {
        return split_1.odd_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
        return split_1.odd_columns;
    }

    split_value<0, 1> split_0;
    split_value<1, 2> split_1;
};

template <>
struct split_value<0, 2> {
    explicit split_value() = default;

    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> even_columns;
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS> odd_columns;
};
template <>
struct unified_value<2> {
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() {
        return split_0.even_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
        return split_0.even_columns;
    }
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() {
        return split_0.odd_columns;
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
        return split_0.odd_columns;
    }

    split_value<0, 2> split_0;
};

struct ycsb_value {
    explicit ycsb_value() = default;

    using NamedColumn = ycsb_value_datatypes::NamedColumn;
    
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->even_columns();
        }
        return std::get<unified_value<2>>(value).even_columns();
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& even_columns() const {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->even_columns();
        }
        return std::get<unified_value<2>>(value).even_columns();
    }
    std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->odd_columns();
        }
        return std::get<unified_value<2>>(value).odd_columns();
    }
    const std::array<fix_string<COL_WIDTH>, HALF_NUM_COLUMNS>& odd_columns() const {
        if (auto val = std::get_if<unified_value<1>>(&value)) {
            return val->odd_columns();
        }
        return std::get<unified_value<2>>(value).odd_columns();
    }

    std::variant<
        unified_value<2>,
        unified_value<1>> value;
};

};  // namespace ycsb_value_datatypes

using ycsb_value = ycsb_value_datatypes::ycsb_value;
CREATE_ADAPTER(ycsb_value, 2);


using ycsb_even_half_value = ycsb_value_datatypes::split_value<0, 1>;
using ycsb_odd_half_value = ycsb_value_datatypes::split_value<1, 2>;

class ycsb_input_generator {
public:
    ycsb_input_generator(int thread_id)
            : gen(thread_id), dis(0, 61) {}

    template <typename value_type>
    value_type random_ycsb_value() {
        value_type ret;
        for (size_t i = 0; i < HALF_NUM_COLUMNS; i++) {
            ret.odd_columns()[i] = random_a_string(COL_WIDTH);
            ret.even_columns()[i] = random_a_string(COL_WIDTH);
        }
        return ret;
    }

    std::mt19937& generator() {
        return gen;
    }

    void random_ycsb_col_value_inplace(col_type *dst) {
        for (size_t i = 0; i < COL_WIDTH; ++i)
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
