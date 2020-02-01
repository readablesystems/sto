#pragma once

#include "Sto.hh"
#include "VersionSelector.hh"
#include "TPCC_structs.hh"

namespace ver_sel {

template <typename VersImpl>
class VerSel<tpcc::warehouse_value, VersImpl> : public VerSelBase<VerSel<tpcc::warehouse_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }
    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        typedef tpcc::warehouse_value::NamedColumn nc;
        auto col_name = static_cast<nc>(col_n);
        if (col_name == nc::w_ytd)
            return 0;
        else
            return 1;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(tpcc::warehouse_value *dst, const tpcc::warehouse_value *src, int cell) {
        switch (cell) {
        case 0:
            dst->w_ytd = src->w_ytd;
            break;
        case 1:
            dst->w_name = src->w_name;
            dst->w_street_1 = src->w_street_1;
            dst->w_street_2 = src->w_street_2;
            dst->w_city = src->w_city;
            dst->w_state = src->w_state;
            dst->w_zip = src->w_zip;
            dst->w_tax = src->w_tax;
            break;
        default:
            always_assert(false, "cell id out of bound\n");
            break;
        }
    }

private:
    version_type vers_[num_versions];
};

template <typename VersImpl>
class VerSel<tpcc::district_value, VersImpl> : public VerSelBase<VerSel<tpcc::district_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }
    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        typedef tpcc::district_value::NamedColumn nc;
        auto col_name = static_cast<nc>(col_n);
        if (col_name == nc::d_ytd)
            return 0;
        else
            return 1;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(tpcc::district_value *dst, const tpcc::district_value *src, int cell) {
        switch (cell) {
        case 0:
            dst->d_ytd = src->d_ytd;
            break;
        case 1:
            dst->d_name = src->d_name;
            dst->d_street_1 = src->d_street_1;
            dst->d_street_2 = src->d_street_2;
            dst->d_city = src->d_city;
            dst->d_state = src->d_state;
            dst->d_zip = src->d_zip;
            dst->d_tax = src->d_tax;
            break;
        default:
            always_assert(false, "cell id out of bound\n");
            break;
        }
    }

private:
    version_type vers_[num_versions];
};

template <typename VersImpl>
class VerSel<tpcc::customer_value, VersImpl> : public VerSelBase<VerSel<tpcc::customer_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() {
        new (&vers_[0]) version_type(v);
    }
    VerSel(type v, bool insert) : vers_() {
        new (&vers_[0]) version_type(v, insert);
    }

    constexpr static int map_impl(int col_n) {
        uint64_t mask = ~(~0ul << vidx_width) ;
        int shift = col_n * vidx_width;
        return static_cast<int>((col_cell_map & (mask << shift)) >> shift);
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(tpcc::customer_value *dst, const tpcc::customer_value *src, int cell) {
        switch (cell) {
        case 0:
            dst->c_balance = src->c_balance;
            dst->c_ytd_payment = src->c_ytd_payment;
            dst->c_payment_cnt = src->c_payment_cnt;
            dst->c_delivery_cnt = src->c_delivery_cnt;
            dst->c_data = src->c_data;
            break;
        case 1:
            dst->c_first = src->c_first;
            dst->c_middle = src->c_middle;
            dst->c_last = src->c_last;
            dst->c_street_1 = src->c_street_1;
            dst->c_street_2 = src->c_street_2;
            dst->c_city = src->c_city;
            dst->c_state = src->c_state;
            dst->c_zip = src->c_zip;
            dst->c_phone = src->c_phone;
            dst->c_since = src->c_since;
            dst->c_credit = src->c_credit;
            dst->c_credit_lim = src->c_credit_lim;
            dst->c_discount = src->c_discount;
            break;
        default:
            always_assert(false, "cell id out of bound\n");
            break;
        }
    }

private:
    version_type vers_[num_versions];
    static constexpr unsigned int vidx_width = 1u;
    static constexpr uint64_t col_cell_map = 8191ul;
};

template <typename VersImpl>
class VerSel<tpcc::order_value, VersImpl> : public VerSelBase<VerSel<tpcc::order_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() { (void)v; }
    VerSel(type v, bool insert) : vers_() { (void)v; (void)insert; }

    constexpr static int map_impl(int col_n) {
        uint64_t mask = ~(~0ul << vidx_width) ;
        int shift = col_n * vidx_width;
        return static_cast<int>((col_cell_map & (mask << shift)) >> shift);
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(tpcc::order_value *dst, const tpcc::order_value *src, int cell) {
        switch (cell) {
            case 0:
                dst->o_carrier_id = src->o_carrier_id;
                break;
            case 1:
                dst->o_c_id = src->o_c_id;
                dst->o_entry_d = src->o_entry_d;
                dst->o_ol_cnt = src->o_ol_cnt;
                dst->o_all_local = src->o_all_local;
                break;
            default:
                always_assert(false, "cell id out of bound\n");
                break;
        }
    }

private:
    version_type vers_[num_versions];
    static constexpr unsigned int vidx_width = 1u;
    static constexpr uint64_t col_cell_map = 29ul;
};

template <typename VersImpl>
class VerSel<tpcc::orderline_value, VersImpl> : public VerSelBase<VerSel<tpcc::orderline_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() { (void)v; }
    VerSel(type v, bool insert) : vers_() { (void)v; (void)insert; }

    constexpr static int map_impl(int col_n) {
        uint64_t mask = ~(~0ul << vidx_width) ;
        int shift = col_n * vidx_width;
        return static_cast<int>((col_cell_map & (mask << shift)) >> shift);
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(tpcc::orderline_value *dst, const tpcc::orderline_value *src, int cell) {
        switch (cell) {
            case 0:
                dst->ol_delivery_d = src->ol_delivery_d;
                break;
            case 1:
                dst->ol_i_id = src->ol_i_id;
                dst->ol_supply_w_id = src->ol_supply_w_id;
                dst->ol_quantity = src->ol_quantity;
                dst->ol_amount = src->ol_amount;
                dst->ol_dist_info = src->ol_dist_info;
                break;
            default:
                always_assert(false, "cell id out of bound\n");
                break;
        }
    }

private:
    version_type vers_[num_versions];
    static constexpr unsigned int vidx_width = 1u;
    static constexpr uint64_t col_cell_map = 59ul;
};

template <typename VersImpl>
class VerSel<tpcc::stock_value, VersImpl> : public VerSelBase<VerSel<tpcc::stock_value, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 2;

    explicit VerSel(type v) : vers_() { (void)v; }
    VerSel(type v, bool insert) : vers_() { (void)v; (void)insert; }

    static int map_impl(int col_n) {
        typedef tpcc::stock_value::NamedColumn nc;
        if (col_n <= static_cast<int>(nc::s_remote_cnt))
            return 0;
        else
            return 1;
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(tpcc::stock_value *dst, const tpcc::stock_value *src, int cell) {
        switch (cell) {
            case 0:
                dst->s_quantity = src->s_quantity;
                dst->s_ytd = src->s_ytd;
                dst->s_order_cnt = src->s_order_cnt;
                dst->s_remote_cnt = src->s_remote_cnt;
                break;
            case 1:
                dst->s_dists = src->s_dists;
                dst->s_data = src->s_data;
                break;
            default:
                always_assert(false, "cell id out of bound\n");
                break;
        }
    }

private:
    version_type vers_[num_versions];
};

}; // namespace ver_sel

