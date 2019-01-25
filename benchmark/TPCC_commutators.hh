// Defines the commutators for TPCC structs

#pragma once

#include "MVCCCommutators.hh"
#include "TPCC_structs.hh"

namespace commutators {

using tpcc::district_value;
using tpcc::customer_value;

template <>
class MvCommutator<district_value> {
public:
    MvCommutator(int64_t new_ytd) : new_ytd(new_ytd) {}

    district_value& operate(district_value &d) {
        d.d_ytd += new_ytd;
        return d;
    }

private:
    int64_t new_ytd;
};

/*
template <>
class MvCommutator<customer_value> {
public:
    MvCommutator(
};
*/

}
