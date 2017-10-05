#pragma once

#include "TPCC_bench.hh"

namespace tpcc {

struct c_data_info {
    c_data_info (uint64_t c, uint64_t cd, uint64_t cw, uint64_t d, uint64_t w, int64_t hm)
        : cid(c), cdid(cd), cwid(cw), did(d), wid(w), h_amount(hm) {}
    const char *buf() const {
        return reinterpret_cast<const char *>(&cid);
    }

    static constexpr size_t len = sizeof(uint64_t)*6;

    uint64_t cid, cdid, cwid;
    uint64_t did, wid;
    int64_t h_amount;
};

struct cu_match_entry {
    typedef tpcc_db::cu_table_type::internal_elem ie_type;

    cu_match_entry(const var_string<16>& c_first, const ie_type *e)
        : first_name(c_first.c_str()), value_elem(e) {}

    bool operator<(const cu_match_entry& other) const {
        return first_name < other.first_name;
    }

    std::string first_name;
    const ie_type *value_elem;
};

void tpcc_runner::run_txn_neworder() {
    uint64_t q_w_id  = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);
    uint64_t q_c_id = ig.gen_customer_id();
    uint64_t num_items = ig.random(5, 15);
    uint64_t rbk = ig.random(1, 100);

    uint64_t ol_i_ids[15];
    uint64_t ol_supply_w_ids[15];
    uint64_t ol_quantities[15];

    uint32_t o_entry_d = ig.gen_date();

    bool all_local = true;

    for (uint64_t i = 0; i < num_items; ++i) {
        uint64_t ol_i_id = ig.gen_item_id();
        if ((i == (num_items - 1)) && rbk == 1)
            ol_i_ids[i] = 0;
        else
            ol_i_ids[i] = ol_i_id;

        bool supply_from_remote = (ig.random(1, 100) == 1);
        uint64_t ol_s_w_id = q_w_id;
        if (supply_from_remote) {
            do {
                ol_s_w_id = ig.random(1, ig.num_warehouses());
            } while (ol_s_w_id == q_w_id);
            all_local = false;
        }
        ol_supply_w_ids[i] = ol_s_w_id;

        ol_quantities[i] = ig.random(1, 10);
    }

    // holding outputs of the transaction
    volatile var_string<16> out_cus_last;
    volatile fix_string<2> out_cus_credit;
    volatile var_string<24> out_item_names[15];
    volatile double out_total_amount = 0.0;
    volatile char out_brand_generic[15];
    (void) out_brand_generic;

    // begin txn
    TRANSACTION {

    bool abort, result;
    uintptr_t row;
    const void *value;

    int64_t wh_tax_rate, dt_tax_rate;
    uint64_t dt_next_oid;

    std::tie(abort, result, std::ignore, value) = db.tbl_warehouses().select_row(warehouse_key(q_w_id));
    TXN_DO(abort);
    assert(result);
    wh_tax_rate = reinterpret_cast<const warehouse_value *>(value)->w_tax;

    std::tie(abort, result, row, value) = db.tbl_districts().select_row(district_key(q_w_id, q_d_id), true);
    TXN_DO(abort);
    assert(result);
    district_value *new_dv = Sto::tx_alloc(reinterpret_cast<const district_value *>(value));
    dt_tax_rate = new_dv->d_tax;
    dt_next_oid = new_dv->d_next_o_id ++;
    db.tbl_districts().update_row(row, new_dv);

    std::tie(abort, result, std::ignore, value) = db.tbl_customers().select_row(customer_key(q_w_id, q_d_id, q_c_id));
    TXN_DO(abort);
    assert(result);

    auto cus_discount = reinterpret_cast<const customer_value *>(value)->c_discount;
    out_cus_last = reinterpret_cast<const customer_value *>(value)->c_last;
    out_cus_credit = reinterpret_cast<const customer_value *>(value)->c_credit;

    order_key ok(q_w_id, q_d_id, dt_next_oid);
    order_value* ov = Sto::tx_alloc<order_value>();
    ov->o_c_id = q_c_id;
    ov->o_carrier_id = 0;
    ov->o_all_local = all_local ? 1 : 0;
    ov->o_entry_d = o_entry_d;
    ov->o_ol_cnt = num_items;

    std::tie(abort, result) = db.tbl_orders().insert_row(ok, ov, false);
    TXN_DO(abort);
    assert(!result);
    std::tie(abort, result) = db.tbl_neworders().insert_row(ok, nullptr, false);
    TXN_DO(abort);
    assert(!result);

    for (uint64_t i = 0; i < num_items; ++i) {
        uint64_t iid = ol_i_ids[i];
        uint64_t wid = ol_supply_w_ids[i];
        uint64_t qty = ol_quantities[i];

        std::tie(abort, result, std::ignore, value) = db.tbl_items().select_row(item_key(iid));
        TXN_DO(abort);
        assert(result);
        uint64_t oid = reinterpret_cast<const item_value *>(value)->i_im_id;
        TXN_DO(oid != 0);
        uint32_t i_price = reinterpret_cast<const item_value *>(value)->i_price;
        out_item_names[i] = reinterpret_cast<const item_value *>(value)->i_name;
        auto i_data = reinterpret_cast<const item_value *>(value)->i_data;

        std::tie(abort, result, row, value) = db.tbl_stocks().select_row(stock_key(wid, iid), true);
        TXN_DO(abort);
        assert(result);
        stock_value *new_sv = Sto::tx_alloc(reinterpret_cast<const stock_value *>(value));
        int32_t s_quantity = new_sv->s_quantity;
        auto s_dist = new_sv->s_dists[q_d_id - 1];
        auto s_data = new_sv->s_data;

        if (i_data.contains("ORIGINAL") && s_data.contains("ORIGINAL"))
            out_brand_generic[i] = 'B';
        else
            out_brand_generic[i] = 'G';

        if ((s_quantity - 10) >= (int32_t)qty)
            new_sv->s_quantity -= qty;
        else
            new_sv->s_quantity += (91 - (int32_t)qty);
        new_sv->s_ytd += qty;
        new_sv->s_order_cnt += 1;
        if (wid != q_w_id)
            new_sv->s_remote_cnt += 1;
        db.tbl_stocks().update_row(row, new_sv);

        double ol_amount = qty * i_price/100.0;

        orderline_key olk(q_w_id, q_d_id, dt_next_oid, i);
        orderline_value *olv = Sto::tx_alloc<orderline_value>();
        olv->ol_i_id = iid;
        olv->ol_supply_w_id = wid;
        olv->ol_delivery_d = 0;
        olv->ol_quantity = qty;
        olv->ol_amount = ol_amount;
        olv->ol_dist_info = s_dist;

        std::tie(abort, result) = db.tbl_orderlines().insert_row(olk, olv, false);
        TXN_DO(abort);
        assert(!result);

        out_total_amount += ol_amount * (1.0 - cus_discount/100.0) * (1.0 + (wh_tax_rate + dt_tax_rate)/100.0);
    }

    (void)__txn_committed;
    // commit txn
    // retry until commits
    } RETRY(true);
}

void tpcc_runner::run_txn_payment() {
    uint64_t q_w_id = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);

    uint64_t q_c_w_id, q_c_d_id, q_c_id;
    std::string last_name;
    auto x = ig.random(1, 100);
    auto y = ig.random(1, 100);

    bool is_home = (x <= 85);
    bool by_name = (y <= 60);

    if (is_home) {
        q_c_w_id = q_w_id;
        q_c_d_id = q_d_id;
    } else {
        do {
            q_c_w_id = ig.random(1, ig.num_warehouses());
        } while (q_c_w_id == q_w_id);
        q_c_d_id = ig.random(1, 10);
    }

    if (by_name) {
        last_name = ig.gen_customer_last_name();
        q_c_id = 0;
    } else {
        q_c_id = ig.gen_customer_id();
    }

    int64_t h_amount = ig.random(100, 500000);
    uint32_t h_date = ig.gen_date();

    // holding outputs of the transaction
    volatile var_string<10> out_w_name, out_d_name;
    volatile var_string<20> out_w_street_1, out_w_street_2, out_w_city;
    volatile var_string<20> out_d_street_1, out_d_street_2, out_d_city;
    volatile fix_string<2> out_w_state, out_d_state;
    volatile fix_string<9> out_w_zip, out_d_zip;
    volatile var_string<16> out_c_first, out_c_last;
    volatile fix_string<2> out_c_middle;
    volatile var_string<20> out_c_street_1, out_c_street_2, out_c_city;
    volatile fix_string<2> out_c_state;
    volatile fix_string<9> out_c_zip;
    volatile fix_string<16> out_c_phone;
    volatile fix_string<2> out_c_credit;
    volatile uint32_t out_c_since;
    volatile int64_t out_c_credit_lim;
    volatile int64_t out_c_discount;
    volatile int64_t out_c_balance;

    // begin txn
    TRANSACTION {

    bool success, result;
    uintptr_t row;
    const void *value;

    // select warehouse row FOR UPDATE and retrieve warehouse info
    warehouse_key wk(q_w_id);
    std::tie(success, result, row, value) = db.tbl_warehouses().select_row(wk, true);
    TXN_DO(success);
    assert(result);

    const warehouse_value *wv = reinterpret_cast<const warehouse_value *>(value);
    warehouse_value *new_wv = Sto::tx_alloc(wv);

    out_w_name = new_wv->w_name;
    out_w_street_1 = new_wv->w_street_1;
    out_w_street_2 = new_wv->w_street_2;
    out_w_city = new_wv->w_city;
    out_w_state = new_wv->w_state;
    out_w_zip = new_wv->w_zip;

    // update warehouse ytd
    new_wv->w_ytd += h_amount;
    db.tbl_warehouses().update_row(row, new_wv);

    // select district row FOR UPDATE and retrieve district info
    district_key dk(q_w_id, q_d_id);
    std::tie(success, result, row, value) = db.tbl_districts().select_row(dk, true);
    TXN_DO(success);
    assert(result);

    const district_value *dv = reinterpret_cast<const district_value *>(value);
    district_value *new_dv = Sto::tx_alloc(dv);

    out_d_name = new_dv->d_name;
    out_d_street_1 = new_dv->d_street_1;
    out_d_street_2 = new_dv->d_street_2;
    out_d_city = new_dv->d_city;
    out_d_state = new_dv->d_state;
    out_d_zip = new_dv->d_zip;

    // update district ytd
    new_dv->d_ytd += h_amount;
    db.tbl_districts().update_row(row, new_dv);

    // select and update customer
    if (by_name) {
        std::vector<cu_match_entry> match_set;
        auto scan_callback = [&] (const lcdf::Str&, const tpcc_db::cu_table_type::internal_elem *e, const customer_value& cv) {
            if (cv.c_last == last_name)
                match_set.emplace_back(cv.c_first, e);
            return true;
        };

        constexpr uint64_t max = std::numeric_limits<uint64_t>::max();
        customer_key ck0(0, 0, 0);
        customer_key ck1(max, max, max);

        success = db.tbl_customers().range_scan<decltype(scan_callback), false/*reverse*/>(ck0, ck1, scan_callback);
        TXN_DO(success);

        size_t n = match_set.size();
        assert(n > 0);
        std::sort(match_set.begin(), match_set.end());
        row = reinterpret_cast<uintptr_t>(match_set[n/2].value_elem);
        db.tbl_customers().select_row(row, true/*for update*/);
    } else {
        assert(q_c_id != 0);
        customer_key ck(q_c_w_id, q_c_d_id, q_c_id);
        std::tie(success, result, row, value) = db.tbl_customers().select_row(ck, true/*for update*/);
        TXN_DO(success);
        assert(result);
    }

    const customer_value *cv = reinterpret_cast<const customer_value *>(value);
    customer_value *new_cv = Sto::tx_alloc(cv);

    new_cv->c_balance -= h_amount;
    new_cv->c_ytd_payment += h_amount;
    new_cv->c_payment_cnt += 1;

    out_c_since = new_cv->c_since;
    out_c_credit_lim = new_cv->c_credit_lim;
    out_c_discount = new_cv->c_discount;
    out_c_balance = new_cv->c_balance;

    if (new_cv->c_credit == "BC") {
        auto info = c_data_info(q_c_id, q_c_d_id, q_c_w_id, q_d_id, q_w_id, h_amount);
        new_cv->c_data.insert_left(info.buf(), info.len);
    }

    db.tbl_customers().update_row(row, new_cv);

    // insert to history table
    history_value *hv = Sto::tx_alloc<history_value>();
    hv->h_c_id = q_c_id;
    hv->h_c_d_id = q_c_d_id;
    hv->h_c_w_id = q_c_w_id;
    hv->h_d_id = q_d_id;
    hv->h_w_id = q_w_id;
    hv->h_date = h_date;
    hv->h_amount = h_amount;
    hv->h_data = std::string(out_w_name.c_str()) + "    " + std::string(out_d_name.c_str());

    history_key hk(db.tbl_histories().gen_key());
    db.tbl_histories().insert_row(hk, hv);

    } RETRY(true);
}

}; // namespace tpcc

namespace std {
    template<>
    struct less<tpcc::cu_match_entry> {
        bool operator() (const tpcc::cu_match_entry& lhs,
                         const tpcc::cu_match_entry& rhs) const {
            return lhs < rhs;
        }
    };
};
