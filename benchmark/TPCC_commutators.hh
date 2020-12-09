// Defines the commutators for TPCC structs

#pragma once

#include "Commutators.hh"
#include "TPCC_structs.hh"

namespace commutators {

using warehouse_value = tpcc::warehouse_value;
using warehouse_value_frequpd = tpcc::warehouse_value_frequpd;
using warehouse_value_infreq = tpcc::warehouse_value_infreq;

using district_value = tpcc::district_value;
using district_value_frequpd = tpcc::district_value_frequpd;
using district_value_infreq = tpcc::district_value_infreq;

using customer_value = tpcc::customer_value;
using customer_value_frequpd = tpcc::customer_value_frequpd;
using customer_value_infreq = tpcc::customer_value_infreq;

using order_value = tpcc::order_value;
using order_value_frequpd = tpcc::order_value_frequpd;
using order_value_infreq = tpcc::order_value_infreq;

using orderline_value = tpcc::orderline_value;
using orderline_value_frequpd = tpcc::orderline_value_frequpd;
using orderline_value_infreq = tpcc::orderline_value_infreq;

using stock_value = tpcc::stock_value;
using stock_value_frequpd = tpcc::stock_value_frequpd;
using stock_value_infreq = tpcc::stock_value_infreq;

using tpcc::c_data_info;

template <>
class Commutator<warehouse_value> {
public:
    Commutator() = default;

    explicit Commutator(int64_t delta_ytd) : delta_ytd(delta_ytd) {}

    void operate(warehouse_value &w) const {
        w.w_ytd += (uint64_t)delta_ytd;
    }

private:
    int64_t delta_ytd;
    friend Commutator<warehouse_value_frequpd>;
};

template <>
class Commutator<warehouse_value_infreq> : Commutator<warehouse_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<warehouse_value>(std::forward<Args>(args)...) {}

    void operate(warehouse_value_infreq&) const {
    }
};

template <>
class Commutator<warehouse_value_frequpd> : Commutator<warehouse_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<warehouse_value>(std::forward<Args>(args)...) {}

    void operate(warehouse_value_frequpd &w) const {
        w.w_ytd += (uint64_t)delta_ytd;
    }
};

template <>
class Commutator<district_value> {
public:
    Commutator() = default;

    explicit Commutator(int64_t delta_ytd) : delta_ytd(delta_ytd) {}

    void operate(district_value &d) const {
        d.d_ytd += delta_ytd;
    }

private:
    int64_t delta_ytd;
    friend Commutator<district_value_frequpd>;
};

template <>
class Commutator<district_value_infreq> : Commutator<district_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<district_value>(std::forward<Args>(args)...) {}

    void operate(district_value_infreq&) const {
    }
};

template <>
class Commutator<district_value_frequpd> : Commutator<district_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<district_value>(std::forward<Args>(args)...) {}

    void operate(district_value_frequpd &d) const {
        d.d_ytd += (uint64_t)delta_ytd;
    }
};

/*
 * Yihe's comment: as it turns out payment transactions don't actually commute
 * because it observes c_balance after updating it (bad transaction).
 * Commutator<customer_value> now supports only the delivery transaction.
 */
template <>
class Commutator<customer_value> {
public:
    enum class OpType : int16_t { Payment, Delivery };
    Commutator() = default;

    explicit Commutator(int64_t delta_balance, int64_t delta_ytd_payment)
            : delta_balance(delta_balance), delta_ytd_payment(delta_ytd_payment),
              op(OpType::Payment), bad_credit(false), delta_data() {}

    explicit Commutator(int64_t delta_balance, int64_t delta_ytd_payment,
            uint64_t c, uint64_t cd, uint64_t cw, uint64_t d, uint64_t w, int64_t hm)
        : delta_balance(delta_balance), delta_ytd_payment(delta_ytd_payment),
          op(OpType::Payment), bad_credit(true), delta_data(c, cd, cw, d, w, hm) {}

    explicit Commutator(int64_t delta_balance)
        : delta_balance(delta_balance), delta_ytd_payment(),
          op(OpType::Delivery), bad_credit(), delta_data() {}

    void operate(customer_value &c) const {
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
    }

private:
    int64_t     delta_balance;
    int64_t     delta_ytd_payment;
    OpType      op;
    bool        bad_credit;
    c_data_info delta_data;

    friend Commutator<customer_value_infreq>;
    friend Commutator<customer_value_frequpd>;
};

template <>
class Commutator<customer_value_infreq> : Commutator<customer_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<customer_value>(std::forward<Args>(args)...) {}

    void operate(customer_value_infreq&) const {
    }
};

template <>
class Commutator<customer_value_frequpd> : Commutator<customer_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<customer_value>(std::forward<Args>(args)...) {}

    void operate(customer_value_frequpd &c) const {
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
    }
};

template <>
class Commutator<order_value> {
public:
    Commutator() = default;

    explicit Commutator(uint64_t write_carrier_id) : write_carrier_id(write_carrier_id) {}
    void operate(order_value& ov) const {
        ov.o_carrier_id = write_carrier_id;
    }
private:
    uint64_t write_carrier_id;

    friend Commutator<order_value_frequpd>;
};

template <>
class Commutator<order_value_infreq> : Commutator<order_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<order_value>(std::forward<Args>(args)...) {}

    void operate(order_value_infreq&) const {
    }
};

template <>
class Commutator<order_value_frequpd> : Commutator<order_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<order_value>(std::forward<Args>(args)...) {}

    void operate(order_value_frequpd &ov) const {
        ov.o_carrier_id = write_carrier_id;
    }
};

template <>
class Commutator<orderline_value> {
public:
    Commutator() = default;

    explicit Commutator(uint32_t write_delivery_d) : write_delivery_d(write_delivery_d) {}
    void operate(orderline_value& ol) const {
        ol.ol_delivery_d = write_delivery_d;
    }
private:
    uint32_t write_delivery_d;

    friend Commutator<orderline_value_frequpd>;
};

template <>
class Commutator<orderline_value_infreq> : Commutator<orderline_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<orderline_value>(std::forward<Args>(args)...) {}

    void operate(orderline_value_infreq&) const {
    }
};

template <>
class Commutator<orderline_value_frequpd> : Commutator<orderline_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<orderline_value>(std::forward<Args>(args)...) {}

    void operate(orderline_value_frequpd &ol) const {
        ol.ol_delivery_d = write_delivery_d;
    }
};

template <>
class Commutator<stock_value> {
public:
    Commutator() = default;

    explicit Commutator(int32_t qty, bool remote) : update_qty(qty), is_remote(remote) {}
    void operate(stock_value& sv) const {
        if ((sv.s_quantity - 10) >= update_qty)
            sv.s_quantity -= update_qty;
        else
            sv.s_quantity += (91 - update_qty);
        sv.s_ytd += update_qty;
        sv.s_order_cnt += 1;
        if (is_remote)
            sv.s_remote_cnt += 1;
    }

private:
    int32_t update_qty;
    bool is_remote;

    friend Commutator<stock_value_frequpd>;
};

template <>
class Commutator<stock_value_infreq> : Commutator<stock_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<stock_value>(std::forward<Args>(args)...) {}

    void operate(stock_value_infreq&) const {
    }
};

template <>
class Commutator<stock_value_frequpd> : Commutator<stock_value> {
public:
    Commutator() = default;

    template <typename... Args>
    Commutator(Args&&... args) : Commutator<stock_value>(std::forward<Args>(args)...) {}

    void operate(stock_value_frequpd &sv) const {
        if ((sv.s_quantity - 10) >= update_qty)
            sv.s_quantity -= update_qty;
        else
            sv.s_quantity += (91 - update_qty);
        sv.s_ytd += update_qty;
        sv.s_order_cnt += 1;
        if (is_remote)
            sv.s_remote_cnt += 1;
    }
};

}
