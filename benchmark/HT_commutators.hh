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

    void operate(ht_value& val) const {
        val.value = v;
    }

private:
    ht_value::col_type v;
};

}
