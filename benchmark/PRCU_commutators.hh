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

    void operate(ht_value& val) const {
        val.value += v;
    }

    ht_value::col_type get() const {
        return v;
    }

private:
    ht_value::col_type v;
};

}
