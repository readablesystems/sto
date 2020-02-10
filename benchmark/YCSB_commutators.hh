#pragma once

#include "Commutators.hh"
#include "YCSB_structs.hh"

namespace commutators {

using ycsb_value = ycsb::ycsb_value;
using ycsb_odd_half_value = ycsb::ycsb_odd_half_value;
using ycsb_even_half_value = ycsb::ycsb_even_half_value;
using col_type = ycsb::col_type;

template <>
class Commutator<ycsb_value> {
public:
    Commutator() = default;
    explicit Commutator(int16_t col_n, const col_type& col_val) : n(col_n) {
        v = col_val;
    }

    ycsb_value& operate(ycsb_value& fv) const {
        if (n % 2) {
            fv.odd_columns[n/2] = v;
        } else {
            fv.even_columns[n/2] = v;
        }
        return fv;
    }

    friend Commutator<ycsb_odd_half_value>;
    friend Commutator<ycsb_even_half_value>;

private:
    int16_t n;
    col_type v;
};

template <>
class Commutator<ycsb_odd_half_value> : public Commutator<ycsb_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<ycsb_value>(std::forward<Args>(args)...) {}

    ycsb_odd_half_value& operate(ycsb_odd_half_value& hv) const {
        if (n % 2) {
            hv.odd_columns[n/2] = v;
        } else {
            always_assert(false, "Not to be executed.");
        }
        return hv;
    }
};

template <>
class Commutator<ycsb_even_half_value> : public Commutator<ycsb_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<ycsb_value>(std::forward<Args>(args)...) {}

    ycsb_even_half_value& operate(ycsb_even_half_value& hv) const {
        if ((n % 2) == 0) {
            hv.even_columns[n/2] = v;
        } else {
            always_assert(false, "Not to be executed.");
        }
        return hv;
    }
};

}
