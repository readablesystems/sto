// Defines the commutators for TPCC structs

#pragma once

#include "MVCCCommutators.hh"
#include "TPCC_structs.hh"

namespace commutators {

using tpcc::district_comm_value;
using tpcc::customer_comm_value;
using tpcc::c_data_info;
using tpcc::orderline_value;

template <>
class MvCommutator<district_comm_value> {
public:
    MvCommutator() = default;

    explicit MvCommutator(int64_t delta_ytd) : delta_ytd(delta_ytd) {}

    district_comm_value& operate(district_comm_value &d) {
        d.d_ytd += delta_ytd;
        return d;
    }

private:
    int64_t delta_ytd;
};


/*
 * Yihe's comment: as it turns out payment transactions don't actually commute
 * because it observes c_balance after updating it (bad transaction).
 * MvCommutator<customer_value> now supports only the delivery transaction.
 */
template <>
class MvCommutator<customer_comm_value> {
public:
    enum class OpType : int16_t { Payment, Delivery };
    MvCommutator() = default;

    explicit MvCommutator(int64_t delta_balance, int64_t delta_ytd_payment)
            : delta_balance(delta_balance), delta_ytd_payment(delta_ytd_payment),
              op(OpType::Payment), bad_credit(false), delta_data() {}

    explicit MvCommutator(int64_t delta_balance, int64_t delta_ytd_payment,
            uint64_t c, uint64_t cd, uint64_t cw, uint64_t d, uint64_t w, int64_t hm)
        : delta_balance(delta_balance), delta_ytd_payment(delta_ytd_payment),
          op(OpType::Payment), bad_credit(true), delta_data(c, cd, cw, d, w, hm) {}

    explicit MvCommutator(int64_t delta_balance)
        : delta_balance(delta_balance), delta_ytd_payment(),
          op(OpType::Delivery), bad_credit(), delta_data() {}

    customer_comm_value& operate(customer_comm_value &c) {
        if (op == OpType::Payment) {
            c.c_balance += delta_balance;
            c.c_payment_cnt += 1;
            c.c_ytd_payment += delta_ytd_payment;
            if (bad_credit) {
                c.c_data.insert_left(delta_data.buf(), c_data_info::len);
            }
        } else if (op == OpType::Delivery) {
            c.c_balance += delta_balance;
            c.c_delivery_cnt += 1;
        }
        return c;
    }

private:
    int64_t     delta_balance;
    int64_t     delta_ytd_payment;
    OpType      op;
    bool        bad_credit;
    c_data_info delta_data;
};

template <>
class MvCommutator<orderline_value> {
public:
    MvCommutator() {}
    explicit MvCommutator(uint32_t write_delivery_d) : write_delivery_d(write_delivery_d) {}
    orderline_value& operate(orderline_value& ol) {
        ol.ol_delivery_d = write_delivery_d;
        return ol;
    }
private:
    uint32_t write_delivery_d;
};

}
