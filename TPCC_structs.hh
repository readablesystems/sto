#pragma once

#include <string>
#include <cstring>

namespace tpcc {

template <size_t ML>
class var_string {
public:
    var_string() {
        bzero(s_, ML);
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

private:
    void initialize_from(const char *c_str) {
        bzero(s_, ML);
        strncpy(s_, c_str, ML);
    }
    void initialize_from(const std::string& str) {
        initialize_from(str.c_str());
    }
    void initialize_from(const var_string& vstr) {
        memcpy(s_, vstr.s_, ML+1);
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
        return strlen(c_str) == FL && !memncmp(s_, c_str, FL);
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

    char s_[FL];
};

// WAREHOUSE

struct warehouse_key {
    warehouse_key(uint64_t id) : w_id(id) {}
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
    district_key(uint64_t wid, uint64_t did) : d_w_id(wid), d_id(did) {}
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
    customer_key(uint64_t wid, uint64_t did, uint64_t cid) : c_w_id(wid), c_d_id(did), c_id(cid) {}
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
    var_string<500> c_data;
};

// HISTORY

struct history_key {
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

// NEW-ORDER (acts like a set)

struct neworder_key {
    uint64_t no_w_id;
    uint64_t no_d_id;
    uint64_t no_o_id;
};

// ORDER

struct order_key {
    order_key(uint64_t wid, uint64_t did, uint64_t oid) : o_w_id(wid), o_d_id(did), o_id(oid) {}
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
    item_key(uint64_t id) : i_id(id) {}
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
    stock_key(uint64_t w, uint64_t i) : s_w_id(w), s_i_id(i) {}
    uint64_t s_w_id;
    uint64_t s_i_id;
};

struct stock_value {
    int32_t        s_quantity;
    uint32_t       s_ytd;
    uint32_t       s_order_cnt;
    uint32_t       s_remote_cnt;
    fix_string<24> s_dists[10];
    var_string<50> s_data;
};

}; // namespace tpcc
