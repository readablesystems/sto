#pragma once

#include "Commutators.hh"
#include "PRCU_structs.hh"

namespace commutators {

using ht_value = prcubench::ht_value;

template <>
class Commutator<ht_value> {
public:
    Commutator() = default;
    explicit Commutator(const ht_value::col_type& v) : v(v) {}

    ht_value& operate(ht_value& val) const {
        val.value += v;
        return val;
    }

    ht_value::col_type get() const {
        return v;
    }

private:
    ht_value::col_type v;
};

}
