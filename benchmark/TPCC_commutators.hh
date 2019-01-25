// Defines the commutators for TPCC structs

#pragma once

#include "MVCCCommutators.hh"
#include "TPCC_structs.hh"

namespace commutators {

using tpcc::district_value;
using tpcc::customer_value;
using tpcc::orderline_value;

template <>
class MvCommutator<district_value> {
public:
    explicit MvCommutator(int64_t new_ytd) : new_ytd(new_ytd) {}

    district_value& operate(district_value &d) {
        d.d_ytd += new_ytd;
        return d;
    }

private:
    int64_t new_ytd;
};


/*
 * Yihe's comment: as it turns out payment transactions don't actually commute
 * because it observes c_balance after updating it (bad transaction).
 * MvCommutator<customer_value> now supports only the delivery transaction.
 */
template <>
class MvCommutator<customer_value> {
public:
    MvCommutator(int64_t delta_balance, uint16_t delta_delivery_cnt)
        : delta_balance(delta_balance), delta_delivery_cnt(delta_delivery_cnt) {}

    customer_value& operate(customer_value &c) {
        c.c_balance += delta_balance;
        c.c_delivery_cnt += 1;
        return c;
    }

private:
    int64_t delta_balance;
    uint16_t delta_delivery_cnt;
};

template <>
class MvCommutator<orderline_value> {
public:
    explicit MvCommutator(uint32_t write_delivery_d) : write_delivery_d(write_delivery_d) {}
    orderline_value& operate(orderline_value& ol) {
        ol.ol_delivery_d = write_delivery_d;
    }
private:
    uint32_t write_delivery_d;
};

}
