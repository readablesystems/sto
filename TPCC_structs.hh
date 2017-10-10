#pragma once

#include <string>
#include <cstring>
#include <cassert>
#include <byteswap.h>

#include "xxhash.h"

#include "str.hh" // lcdf::Str

#define NUM_DISTRICTS_PER_WAREHOUSE 10
#define NUM_CUSTOMERS_PER_DISTRICT  3000
#define NUM_ITEMS                   100000

namespace tpcc {

template <size_t ML>
class var_string {
public:
    static constexpr size_t max_length = ML;

    var_string() {
        bzero(s_, ML+1);
    }
    var_string(const char *c_str) {
        initialize_from(c_str);
    }
    var_string(const std::string& str) {
        initialize_from(str);
    }
    var_string(const var_string& vstr) {
        initialize_from(vstr);
    }

    bool operator==(const char *c_str) const {
        return !strncmp(s_, c_str, ML);
    }
    bool operator==(const std::string& str) const {
        return !strncmp(s_, str.c_str(), ML);
    }
    bool operator==(const var_string& rhs) const {
        return !strncmp(s_, rhs.s_, ML);
    }
    char operator[](size_t idx) const {
        return s_[idx];
    }
    var_string& operator=(const var_string& rhs) {
        initialize_from(rhs);
        return *this;
    }
    var_string& operator=(const var_string& rhs) volatile {
        initialize_from(rhs);
        return *const_cast<var_string *>(this);
    }
    explicit operator std::string() {
        return std::string(s_);
    }

    bool contains(const char *substr) const {
        return (strstr(s_, substr) != nullptr);
    }
    size_t length() const {
        return strlen(s_);
    }
    bool place(const char *str, size_t pos) {
        size_t in_len = strlen(str);
        if (pos + in_len > length())
            return false;
        memcpy(s_ + pos, str, in_len);
        return true;
    }
    const char *c_str() const {
        return s_;
    }
    char *c_str() {
        return s_;
    }
    const char *c_str() const volatile {
        return s_;
    }
    char *c_str() volatile {
        return const_cast<char *>(s_);
    }

    friend std::hash<var_string>;

private:
    void initialize_from(const char *c_str) {
        bzero(s_, ML+1);
        strncpy(s_, c_str, ML);
    }
    void initialize_from(const std::string& str) {
        initialize_from(str.c_str());
    }
    void initialize_from(const var_string& vstr) {
        memcpy(s_, vstr.s_, ML+1);
    }
    void initialize_from(const var_string& vstr) volatile {
        memcpy(const_cast<char *>(s_), vstr.s_, ML+1);
    }

    char s_[ML + 1];
};

template <size_t FL>
class fix_string {
public:
    fix_string() {
        memset(s_, ' ', FL);
    }
    fix_string(const char *c_str) {
        memset(s_, ' ', FL);
        strncpy(s_, c_str, FL);
    }
    fix_string(const std::string& str) {
        memset(s_, ' ', FL);
        strncpy(s_, str.c_str(), FL);
    }

    bool operator==(const char *c_str) const {
        return strlen(c_str) == FL && !memcmp(s_, c_str, FL);
    }
    bool operator==(const fix_string& rhs) const {
        return !memcmp(s_, rhs.s_, FL);
    }
    char operator[](size_t idx) const {
        return s_[idx];
    }
    fix_string& operator=(const fix_string& rhs) {
        initialize_from(rhs);
        return *this;
    }
    fix_string& operator=(const fix_string& rhs) volatile {
        initialize_from(rhs);
        return *const_cast<fix_string *>(this);
    }
    explicit operator std::string() {
        return std::string(s_, FL);
    }

    void insert_left(const char *buf, size_t cnt) {
        memmove(s_ + cnt, s_, FL - cnt);
        memcpy(s_, buf, cnt);
    }

    friend std::hash<fix_string>;

private:
    void initialize_from(const char *c_str) {
        memset(s_, ' ', FL);
        size_t len = strlen(c_str);
        for (size_t i = 0; i < len && i < FL; ++i)
            s_[i] = c_str[i];
    }
    void initialize_from(const std::string& str) {
        initialize_from(str);
    }
    void initialize_from(const fix_string& fstr) {
        memcpy(s_, fstr.s_, FL);
    }
    void initialize_from(const fix_string& fstr) volatile {
        memcpy(const_cast<char *>(s_), fstr.s_, FL);
    }

    char s_[FL];
};

// swap byte order so key can be used correctly in masstree
template <typename IntType>
static inline IntType bswap(IntType x) {
    if (sizeof(IntType) == 4)
        return __bswap_32(x);
    else if (sizeof(IntType) == 8)
        return __bswap_64(x);
    else
        assert(false);
}

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

struct warehouse_value {
    var_string<10> w_name;
    var_string<20> w_street_1;
    var_string<20> w_street_2;
    var_string<20> w_city;
    fix_string<2>  w_state;
    fix_string<9>  w_zip;
    int64_t       w_tax; // in 1/10000
    int64_t       w_ytd; // in 1/100
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
    var_string<10> d_name;
    var_string<20> d_street_1;
    var_string<20> d_street_2;
    var_string<20> d_city;
    fix_string<2>  d_state;
    fix_string<9>  d_zip;
    int64_t        d_tax;
    int64_t        d_ytd;
    uint64_t       d_next_o_id;
};

// CUSTOMER

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
    int64_t         c_balance;
    int64_t         c_ytd_payment;
    uint16_t        c_payment_cnt;
    uint16_t        c_delivery_cnt;
    fix_string<500> c_data;
};

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

struct order_key {
    order_key(uint64_t wid, uint64_t did, uint64_t oid) {
        o_w_id = bswap(wid);
        o_d_id = bswap(did);
        o_id = bswap(oid);
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

template <size_t ML>
struct hash<tpcc::var_string<ML>> {
    size_t operator()(const tpcc::var_string<ML>& arg) const {
        return XXH64(arg.s_, ML + 1, xxh_seed);
    }
};

template <size_t FL>
struct hash<tpcc::fix_string<FL>> {
    size_t operator()(const tpcc::fix_string<FL>& arg) const {
        return XXH64(arg.s_, FL, xxh_seed);
    }
};

template <>
struct hash<tpcc::warehouse_key> {
    size_t operator()(const tpcc::warehouse_key& arg) const {
        return arg.w_id;
    }
};

template <>
struct hash<tpcc::district_key> {
    size_t operator()(const tpcc::district_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::district_key), xxh_seed);
    }
};

template <>
struct hash<tpcc::customer_key> {
    size_t operator()(const tpcc::customer_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::customer_key), xxh_seed);
    }
};

template <>
struct hash<tpcc::history_key> {
    size_t operator()(const tpcc::history_key& arg) const {
        return arg.h_id;
    }
};

template <>
struct hash<tpcc::order_key> {
    size_t operator()(const tpcc::order_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::order_key), xxh_seed);
    }
};

template <>
struct hash<tpcc::orderline_key> {
    size_t operator()(const tpcc::orderline_key& arg) const {
        return XXH64(&arg, sizeof(tpcc::orderline_key), xxh_seed);
    }
};

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

}; // namespace std
