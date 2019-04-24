#pragma once

#include "Commutators.hh"
#include "YCSB_structs.hh"

namespace commutators {

using ycsb_value = ycsb::ycsb_value;
using ycsb_half_value = ycsb::ycsb_half_value;
using col_type = ycsb_value::col_type;

template <>
class Commutator<ycsb_value> {
public:
    Commutator() = default;
    explicit Commutator(int16_t col_n, const col_type& col_val) : n(col_n) {
        v = col_val;
    }

    ycsb_value& operate(ycsb_value& fv) const {
        fv.cols[n] = v;
        return fv;
    }

private:
    int16_t n;
    col_type v;
};

template <>
class Commutator<ycsb_half_value> {
public:
    Commutator() = default;
    explicit Commutator(int16_t col_n, const col_type& col_val) : n(col_n) {
        v = col_val;
    }

    ycsb_half_value& operate(ycsb_half_value& fv) const {
        fv.cols[n] = v;
        return fv;
    }

private:
    int16_t n;
    col_type v;
};

}
