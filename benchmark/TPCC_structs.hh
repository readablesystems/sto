#pragma once

#include <string>
#include <cassert>

#include "DB_structs.hh"
#include "xxhash.h"
#include "str.hh" // lcdf::Str

#define NUM_DISTRICTS_PER_WAREHOUSE 10
#define NUM_CUSTOMERS_PER_DISTRICT  3000
#define NUM_ITEMS                   100000

#ifndef TABLE_FINE_GRAINED
#define TABLE_FINE_GRAINED 0
#endif

namespace tpcc {

// singleton class used for fast oid generation
// this is a replacement for the d_next_o_id field in district tables to
// avoid excessive aborts when used with STO concurrency control

using namespace bench;

class tpcc_oid_generator {
public:
    static constexpr size_t max_whs = 32;
    static constexpr size_t max_dts = 16;

    tpcc_oid_generator() {
        for (auto &oid_gen : oid_gens) {
            for (uint64_t &j : oid_gen) {
                j = 3001;
            }
        }
    }

    uint64_t next(uint64_t wid, uint64_t did) {
        return fetch_and_add(&(oid_gens[wid % max_whs][did % max_dts]), 1);
    }

private:
    uint64_t oid_gens[max_whs][max_dts];
};

// WAREHOUSE

struct warehouse_key {
    warehouse_key(uint64_t id) {
        w_id = bswap(id);
    }
    bool operator==(const warehouse_key& other) const {
        return w_id == other.w_id;
    }
    bool operator!=(const warehouse_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t w_id;
};

struct warehouse_const_value {
    enum class NamedColumn : int { w_name = 0,
                                   w_street_1,
                                   w_street_2,
                                   w_city,
                                   w_state,
                                   w_zip,
                                   w_tax };

    var_string<10> w_name;
    var_string<20> w_street_1;
    var_string<20> w_street_2;
    var_string<20> w_city;
    fix_string<2>  w_state;
    fix_string<9>  w_zip;
    int64_t        w_tax; // in 1/10000
    //uint64_t       w_ytd; // in 1/100
};

// DISTRICT

struct district_key {
    district_key(uint64_t wid, uint64_t did) {
        d_w_id = bswap(wid);
        d_id = bswap(did);
    }
    bool operator==(const district_key& other) const {
        return (d_w_id == other.d_w_id) && (d_id == other.d_id);
    }
    bool operator!=(const district_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t d_w_id;
    uint64_t d_id;
};

struct district_value {
    enum class NamedColumn : int { d_name = 0,
                                   d_street_1,
                                   d_street_2,
                                   d_city,
                                   d_state,
                                   d_zip,
                                   d_tax,
                                   d_ytd };    

    var_string<10> d_name;
    var_string<20> d_street_1;
    var_string<20> d_street_2;
    var_string<20> d_city;
    fix_string<2>  d_state;
    fix_string<9>  d_zip;
    int64_t        d_tax;
#if TABLE_FINE_GRAINED == 0
    int64_t        d_ytd;
#endif
    // we use the separate oid generator for better semantics in transactions
    //uint64_t       d_next_o_id;
};

// CUSTOMER

// customer name index hack <-- the true source of performance
struct customer_idx_key {
    customer_idx_key(uint64_t wid, uint64_t did, const var_string<16>& last, const var_string<16>& first) {
        c_w_id = bswap(wid);
        c_d_id = bswap(did);
        memcpy(c_last, last.c_str(), sizeof(c_last));
        memcpy(c_first, first.c_str(), sizeof(c_first));
    }

    customer_idx_key(uint64_t wid, uint64_t did, const std::string& last, unsigned char first_fill) {
        c_w_id = bswap(wid);
        c_d_id = bswap(did);
        memset(c_last, 0x00, sizeof(c_last));
        memcpy(c_last, last.c_str(), last.length());
        memset(c_first, first_fill, sizeof(c_first));
    }

    customer_idx_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }

    bool operator==(const customer_idx_key& other) const {
        return !memcmp(this, &other, sizeof(*this));
    }
    bool operator!=(const customer_idx_key& other) const {
        return !((*this) == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t c_w_id;
    uint64_t c_d_id;
    char c_last[16];
    char c_first[16];
};

struct customer_idx_value {
    enum class NamedColumn : int { c_id = 0 };    

    customer_idx_value(uint64_t cid) : c_id(cid) {};
    uint64_t c_id;
};

struct customer_key {
    customer_key(uint64_t wid, uint64_t did, uint64_t cid) {
        c_w_id = bswap(wid);
        c_d_id = bswap(did);
        c_id = bswap(cid);
    }
    customer_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }

    bool operator==(const customer_key& other) const {
        return (c_w_id == other.c_w_id) && (c_d_id == other.c_d_id) && (c_id == other.c_id);
    }
    bool operator!=(const customer_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t get_c_id() const {
        return bswap(c_id);
    }

    uint64_t c_w_id;
    uint64_t c_d_id;
    uint64_t c_id;
};

struct customer_value {
    enum class NamedColumn : int { c_first = 0,
                                 c_middle,
                                 c_last,
                                 c_street_1,
                                 c_street_2,
                                 c_city,
                                 c_state,
                                 c_zip,
                                 c_phone,
                                 c_since,
                                 c_credict,
                                 c_credit_lim,
                                 c_discount,
                                 c_balance,
                                 c_ytd_payment,
                                 c_payment_cnt,
                                 c_delivery_cnt,
                                 c_data };

    var_string<16>  c_first;
    fix_string<2>   c_middle;
    var_string<16>  c_last;
    var_string<20>  c_street_1;
    var_string<20>  c_street_2;
    var_string<20>  c_city;
    fix_string<2>   c_state;
    fix_string<9>   c_zip;
    fix_string<16>  c_phone;
    uint32_t        c_since;
    fix_string<2>   c_credit;
    int64_t         c_credit_lim;
    int64_t         c_discount;
#if TABLE_FINE_GRAINED == 0
    int64_t         c_balance;
    int64_t         c_ytd_payment;
    uint16_t        c_payment_cnt;
    uint16_t        c_delivery_cnt;
    fix_string<500> c_data;
#endif
};

#if TABLE_FINE_GRAINED == 1
struct customer_value_variable {
    enum class NamedColumn : int { c_since = 0,
                                   c_credit,
                                   c_credit_lim,
                                   c_discount,
                                   c_balance,
                                   c_ytd_payment,
                                   c_payment_cnt,
                                   c_delivery_cnt,
                                   c_data };

    // begin: duplicate data
    uint32_t      c_since;
    fix_string<2> c_credit;
    int64_t       c_credit_lim;
    int64_t       c_discount;
    // end: duplicate data
    int64_t       c_balance;
    int64_t       c_ytd_payment;
    uint16_t      c_payment_cnt;
    uint16_t      c_delivery_cnt;
    fix_string<500> c_data;
};
#endif

// HISTORY

struct history_key {
    history_key(uint64_t id) : h_id(bswap(id)) {}
    bool operator==(const history_key& other) const {
        return h_id == other.h_id;
    }
    bool operator!=(const history_key& other) const {
        return h_id != other.h_id;
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t h_id;
};

struct history_value {
    enum class NamedColumn : int { h_c_id = 0,
                                   h_c_d_id,
                                   h_c_w_id,
                                   h_d_id,
                                   h_w_id,
                                   h_date,
                                   h_amount,
                                   h_data };

    uint64_t       h_c_id;
    uint64_t       h_c_d_id;
    uint64_t       h_c_w_id;
    uint64_t       h_d_id;
    uint64_t       h_w_id;
    uint32_t       h_date;
    int64_t        h_amount;
    var_string<24> h_data;
};

// ORDER

struct order_cidx_key {
    order_cidx_key(uint64_t wid, uint64_t did, uint64_t cid, uint64_t oid) {
        o_w_id = bswap(wid);
        o_d_id = bswap(did);
        o_c_id = bswap(cid);
        o_id = bswap(oid);
    }

    order_cidx_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }

    bool operator==(const order_cidx_key& other) const {
        return !memcmp(this, &other, sizeof(*this));
    }
    bool operator!=(const order_cidx_key& other) const {
        return !((*this) == other);
    }

    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t o_w_id;
    uint64_t o_d_id;
    uint64_t o_c_id;
    uint64_t o_id;
};

struct order_key {
    order_key(uint64_t wid, uint64_t did, uint64_t oid) {
        o_w_id = bswap(wid);
        o_d_id = bswap(did);
        o_id = bswap(oid);
    }

    order_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }
    bool operator==(const order_key& other) const {
        return (o_w_id == other.o_w_id) && (o_d_id == other.o_d_id) && (o_id == other.o_id);
    }
    bool operator!=(const order_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t o_w_id;
    uint64_t o_d_id;
    uint64_t o_id;
};

struct order_value{
    enum class NamedColumn : int { o_c_id = 0,
                                   o_carrier_id,
                                   o_entry_d,
                                   o_ol_cnt,
                                   o_all_local };

    uint64_t o_c_id;
    uint64_t o_carrier_id;
    uint32_t o_entry_d;
    uint32_t o_ol_cnt;
    uint32_t o_all_local;
};

// ORDER-LINE

struct orderline_key {
    orderline_key(uint64_t w, uint64_t d, uint64_t o, uint64_t n) {
        ol_w_id = bswap(w);
        ol_d_id = bswap(d);
        ol_o_id = bswap(o);
        ol_number = bswap(n);
    }

    orderline_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }
    bool operator==(const orderline_key& other) const {
        return (ol_w_id == other.ol_w_id) && (ol_d_id == other.ol_d_id) &&
            (ol_o_id == other.ol_o_id) && (ol_number == other.ol_number);
    }
    bool operator!=(const orderline_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t ol_w_id;
    uint64_t ol_d_id;
    uint64_t ol_o_id;
    uint64_t ol_number;
};

struct orderline_value {
    enum class NamedColumn : int { ol_i_id = 0;
                                   ol_supply_w_id,
                                   ol_delivery_d,
                                   ol_quantity,
                                   ol_amount,
                                   ol_dist_info };

    uint64_t       ol_i_id;
    uint64_t       ol_supply_w_id;
    uint32_t       ol_delivery_d;
    uint32_t       ol_quantity;
    int32_t        ol_amount;
    fix_string<24> ol_dist_info;
};

// ITEM

struct item_key {
    item_key(uint64_t id) {
        i_id = bswap(id);
    }

    item_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }
    bool operator==(const item_key& other) const {
        return i_id == other.i_id;
    }
    bool operator!=(const item_key& other) const {
        return i_id != other.i_id;
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t i_id;
};

struct item_value {
    enum class NamedColumn : int { i_im_id = 0,
                                   i_price,
                                   i_name,
                                   i_data };

    uint64_t       i_im_id;
    uint32_t       i_price;
    var_string<24> i_name;
    var_string<50> i_data;
};

// STOCK

struct stock_key {
    stock_key(uint64_t w, uint64_t i) {
        s_w_id = bswap(w);
        s_i_id = bswap(i);
    }

    stock_key(const lcdf::Str& mt_key) {
        assert(mt_key.length() == sizeof(*this));
        memcpy(this, mt_key.data(), sizeof(*this));
    }
    bool operator==(const stock_key& other) const {
        return (s_w_id == other.s_w_id) && (s_i_id == other.s_i_id);
    }
    bool operator!=(const stock_key& other) const {
        return !(*this == other);
    }
    operator lcdf::Str() const {
        return lcdf::Str((const char *)this, sizeof(*this));
    }

    uint64_t s_w_id;
    uint64_t s_i_id;
};

struct stock_value {
    enum class NamedColumn : int { s_quantity,
                                   s_ytd,
                                   s_order_cnt,
                                   s_remote_cnt,
                                   s_dists,
                                   s_data };

    int32_t        s_quantity;
    uint32_t       s_ytd;
    uint32_t       s_order_cnt;
    uint32_t       s_remote_cnt;
    fix_string<24> s_dists[NUM_DISTRICTS_PER_WAREHOUSE];
    var_string<50> s_data;
};

}; // namespace tpcc

namespace std {

static constexpr size_t xxh_seed = 0xdeadbeefdeadbeef;

using bench::var_string;
using bench::fix_string;
using bench::bswap;

template <size_t ML>
struct hash<var_string<ML>> {
    size_t operator()(const var_string<ML>& arg) const {
        return XXH64(arg.s_, ML + 1, xxh_seed);
    }
};

template <size_t FL>
struct hash<fix_string<FL>> {
    size_t operator()(const fix_string<FL>& arg) const {
        return XXH64(arg.s_, FL, xxh_seed);
    }
};

template <>
struct hash<tpcc::warehouse_key> {
    size_t operator()(const tpcc::warehouse_key& arg) const {
        return arg.w_id;
    }
};

ostream& operator<<(ostream& os, const tpcc::warehouse_key& wk) {
    os << "warehouse_key:" << bswap(wk.w_id);
    return os;
}

template <>
struct hash<tpcc::district_key> {
    size_t operator()(const tpcc::district_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::district_key), xxh_seed);
    }
};

ostream& operator<<(ostream& os, const tpcc::district_key& dk) {
    os << "district_key:w="
       << bswap(dk.d_w_id) << ",d="
       << bswap(dk.d_id);
    return os;
}

template <>
struct hash<tpcc::customer_key> {
    size_t operator()(const tpcc::customer_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::customer_key), xxh_seed);
    }
};

ostream& operator<<(ostream& os, const tpcc::customer_key& ck) {
    os << "customer_key:w="
       << bswap(ck.c_w_id) << ",d="
       << bswap(ck.c_d_id) << ",c="
       << bswap(ck.c_id);
    return os;
}

template <>
struct hash<tpcc::history_key> {
    size_t operator()(const tpcc::history_key& arg) const {
        return arg.h_id;
    }
};

ostream& operator<<(ostream& os, const tpcc::history_key& hk) {
    os << "history_key:" << bswap(hk.h_id);
    return os;
}

template <>
struct hash<tpcc::order_key> {
    size_t operator()(const tpcc::order_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::order_key), xxh_seed);
    }
};

ostream& operator<<(ostream& os, const tpcc::order_key& ok) {
    os << "order_key";
    (void)ok;
    return os;
}

template <>
struct hash<tpcc::orderline_key> {
    size_t operator()(const tpcc::orderline_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::orderline_key), xxh_seed);
    }
};

ostream& operator<<(ostream& os, const tpcc::orderline_key& olk) {
    os << "orderline_key";
    (void)olk;
    return os;
}

template <>
struct hash<tpcc::item_key> {
    size_t operator()(const tpcc::item_key& arg) const {
        return arg.i_id;
    }
};

template <>
struct hash<tpcc::stock_key> {
    size_t operator()(const tpcc::stock_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::stock_key), xxh_seed);
    }
};

ostream& operator<<(ostream& os, const tpcc::customer_idx_key& cik) {
    os << "customer_idx_key";
    (void)cik;
    return os;
}

ostream& operator<<(ostream& os, const tpcc::order_cidx_key& oik) {
    os << "order_cidx_key";
    (void)oik;
    return os;
}

}; // namespace std
