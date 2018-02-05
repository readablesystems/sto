#pragma once

#include <string>
#include <random>

#include "TPCC_structs.hh" // tpcc::fix_string
#include "str.hh" // lcdf::Str

namespace ycsb {

enum class mode_id : int { ReadOnly = 0, MediumContention, HighContention };

static constexpr size_t ycsb_col_size = 8;

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

struct ycsb_value {
//    tpcc::fix_string<100>  cols[10];
    tpcc::fix_string<ycsb_col_size> col;
};

class ycsb_input_generator {
public:
    ycsb_input_generator(int thread_id)
            : gen(thread_id), dis(0, 61) {}

    ycsb_value random_ycsb_value() {
        ycsb_value ret;
//        for (int i = 0; i < 10; i++) {
//            ret.cols[i] = random_a_string();
//        }
        ret.col = random_a_string(ycsb_col_size);
        return ret;
    }

private:
    std::string random_a_string(size_t len) {
        std::string str;
        str.reserve(len);
        for (auto i = 0u; i < len; ++i) {
            auto n = dis(gen);
            char c = (n < 26) ? char('a' + n) :
                     ((n < 52) ? char('A' + (n - 26)) : char('0' + (n - 52)));
            str.push_back(c);
        }
        return str;
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
