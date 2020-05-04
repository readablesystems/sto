#pragma once

#include "Commutators.hh"
#include "HT_structs.hh"

namespace commutators {

using ht_value = htbench::ht_value;

template <>
class Commutator<ht_value> {
public:
    Commutator() = default;
    explicit Commutator(const ht_value::col_type& v) : v(v) {}

    ht_value& operate(ht_value& val) const {
        val.value = v;
        return val;
    }

private:
    ht_value::col_type v;
};

}
