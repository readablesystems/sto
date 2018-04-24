#pragma once

#include <string>
#include <cassert>
#include "DB_structs.hh"
#include "str.hh"

namespace tpce {

using namespace bench;

struct zip_code_key {
    fix_string<12> zc_code;
};

struct zip_code_row {
    var_string<80> zc_town;
    var_string<80> zc_div;
};

struct address_key {
    int64_t ad_id;
};

struct address_row {
    var_string<80> ad_line1;
    var_string<80> ad_line2;
    fix_string<12> ad_zc_code;
    var_string<80> ad_ctry;
};

struct status_type_key {
    fix_string<4> st_id;
};

struct status_type_row {
    fix_string<10> st_name;
};

struct taxrate_key {
    fix_string<4> tx_id;
};

struct taxrate_row {
    var_string<50> tx_name;
    float          tx_rate;
};

struct customer_key {
    int64_t c_id;
};

struct customer_row {
    var_string<20> c_tax_id;
    fix_string<4>  c_st_id;
    var_string<30> c_l_name;
    var_string<30> c_f_name;
    fix_string<1>  c_m_name;
    fix_string<1>  c_gndr;
    int32_t        c_tier;
    uint32_t       c_dob;
    int64_t        c_ad_id;
    fix_string<3>  c_ctry_1;
    fix_string<3>  c_area_1;
    fix_string<10> c_local_1;
    fix_string<5>  c_ext_1;
    fix_string<3>  c_ctry_2;
    fix_string<3>  c_area_2;
    fix_string<10> c_local_2;
    fix_string<5>  c_ext_2;
    fix_string<3>  c_ctry_3;
    fix_string<3>  c_area_3;
    fix_string<10> c_local_3;
    fix_string<5>  c_ext_3;
    var_string<50> c_email_1;
    var_string<50> c_email_2;
};

// Market Tables

struct exchange_key {
    fix_string<6> ex_id;
};

struct exchange_row {
    var_string<100> ex_name;
    int32_t         ex_num_symb;
    int32_t         ex_open;
    int32_t         ex_close;
    var_string<150> ex_desc;
    int64_t         ex_ad_id;
};

struct sector_key {
    fix_string<2> sc_id;
};

struct sector_row {
    var_string<30> sc_name;
};

struct industry_key {
    fix_string<2> in_id;
};

struct industry_row {
    var_string<50> in_name;
    fix_string<2>  in_sc_id;
};

struct company_key {
    int64_t co_id;
};

struct company_row {
    fix_string<4>   co_st_id;
    var_string<60>  co_name;
    fix_string<2>   co_in_id;
    fix_string<4>   co_sp_rate;
    var_string<100> co_ceo;
    int64_t         co_ad_id;
    var_string<150> co_desc;
    uint32_t        co_open_date;
};

struct company_competitor_key {
    int64_t       cp_co_id;
    int64_t       cp_comp_co_id;
    fix_string<2> cp_in_id;
};

struct security_key {
    fix_string<15> s_symb;
};

struct security_row {
    fix_string<6>  s_issue;
    fix_string<4>  s_st_id;
    var_string<70> s_name;
    fix_string<6>  s_ex_id;
    int64_t        s_co_id;
    int64_t        s_num_out;
    uint32_t       s_start_date;
    uint32_t       s_exch_date;
    float          s_pe;
    float          s_52wk_high;
    uint32_t       s_52wk_high_date;
    float          s_52wk_low;
    uint32_t       s_52wk_low_date;
    float          s_dividend;
    float          s_yield;
};

struct daily_market_key {
    uint32_t       dm_date;
    fix_string<15> dm_s_symb;
};

struct daily_market_row {
    float   dm_close;
    float   dm_high;
    float   dm_low;
    int64_t dm_vol;
};

struct financial_key {
    int64_t fi_co_id;
    int32_t fi_year;
    int32_t fi_qtr;
};

struct financial_row {
    uint32_t fi_qtr_start_date;
    float    fi_revenue;
    float    fi_net_earn;
    float    fi_basic_eps;
    float    fi_dilut_eps;
    float    fi_margin;
    float    fi_inventory;
    float    fi_assets;
    float    fi_liability;
    int64_t  fi_out_basic;
    int64_t  fi_out_dilut;
};

struct last_trade_key {
    fix_string<15> lt_s_symb;
};

struct last_trade_row {
    uint32_t lt_dts;
    float    lt_price;
    float    lt_open_price;
    int64_t  lt_vol;
};

struct news_item_key {
    int64_t ni_id;
};

struct news_item_row {
    var_string<80>   ni_headline;
    var_string<225>  ni_summary;
    var_string<1024> ni_item;
    uint32_t         ni_dts;
    var_string<30>   ni_source;
    var_string<30>   ni_author;
};

struct news_xref_key {
    int64_t nx_ni_id;
    int64_t nx_co_id;
};

// Broker tables 1/3

struct broker_key {
    int64_t b_id;
};

struct broker_row {
    fix_string<4>   b_st_id;
    var_string<100> b_name;
    int32_t         b_num_trades;
    float           b_comm_total;
};

// Customer tables 2/2

struct customer_account_key {
    int64_t ca_id;
};

struct customer_account_row {
    int64_t        ca_b_id;
    int64_t        ca_c_id;
    var_string<50> ca_name;
    int32_t        ca_tax_st;
    float          ca_bal;
};

struct account_permission_key {
    int64_t       ap_ca_id;
    fix_string<4> ap_acl;
};

struct account_permission_row {
    var_string<20> ap_tax_id;
    var_string<30> ap_l_name;
    var_string<30> ap_f_name;
};

struct customer_taxrate_key {
    fix_string<4> cx_tx_id;
    int64_t       cx_c_id;
} __attribute__((packed));

// Broker tables 2/3

struct trade_type_key {
    fix_string<3> tt_id;
};

struct trade_type_row {
    fix_string<12> tt_name;
    int32_t        tt_is_sell;
    int32_t        tt_is_mrkt;
};

struct trade_key {
    int64_t t_id;
};

struct trade_row {
    uint32_t       t_dts;
    fix_string<4>  t_st_id;
    fix_string<3>  t_tt_id;
    int32_t        t_is_cash;
    fix_string<15> t_s_symb;
    int32_t        t_qty;
    float          t_bid_price;
    int64_t        t_ca_id;
    var_string<64> t_exec_name;
    float          t_trade_price;
    float          t_chrg;
    float          t_comm;
    float          t_tax;
    int32_t        t_lifo;
};

struct settlement_key {
    int64_t se_t_id;
};

struct settlement_row {
    var_string<40> se_cash_type;
    uint32_t       se_cash_due_date;
    float          se_amt;
};

struct trade_history_key {
    int64_t       th_t_id;
    fix_string<4> th_st_id;
};

struct trade_history_row {
    uint32_t th_dts;
};

struct holding_summary_key {
    int64_t hs_ca_id;
    fix_string<15> hs_s_symb;
};

struct holding_summary_row {
    int32_t hs_qty;
};

struct holding_key {
    int64_t h_t_id;
};

struct holding_row {
    int64_t        h_ca_id;
    fix_string<15> h_s_symb;
    uint32_t       h_dts;
    float          h_price;
    int32_t        h_qty;
};

struct holding_history_key {
    int64_t hh_h_t_id;
    int64_t hh_t_id;
};

struct holding_history_row {
    int32_t hh_before_qty;
    int32_t hh_after_qty;
};

struct watch_list_key {
    int64_t wl_id;
};

struct watch_list_row {
    int64_t wl_c_id;
};

struct watch_item_key {
    int64_t        wi_wl_id;
    fix_string<15> wi_s_symb;
};

// Broker tables 3/3

struct cash_transaction_key {
    int64_t ct_t_id;
};

struct cash_transaction_row {
    uint32_t        ct_dts;
    float           ct_amt;
    var_string<100> ct_name;
};

struct charge_key {
    fix_string<3> ch_tt_id;
    int32_t       ch_c_tier;
} __attribute__((packed));

struct charge_row {
    float ch_chrg;
};

struct commission_rate_key {
    int32_t       cr_c_tier;
    int32_t       cr_from_qty;
    fix_string<3> cr_tt_id;
    fix_string<6> cr_ex_id;
};

struct commission_rate_row {
    int32_t cr_to_qty;
    float   cr_rate;
};

struct trade_request_key {
    int64_t tr_t_id;
};

struct trade_request_row {
    fix_string<3>  tr_tt_id;
    fix_string<15> tr_s_symb;
    int32_t        tr_qty;
    float          tr_bid_price;
    int64_t        tr_ca_id;
};

}; // namespace tpce
