#pragma once

#include "TPCC_bench.hh"
#include <set>

namespace tpcc {

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_neworder() {
    //fprintf(stdout, "NEWORDER\n");

    typedef stock_value::NamedColumn st_nc;

    uint64_t q_w_id  = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);
    uint64_t q_c_id = ig.gen_customer_id();
    uint64_t num_items = ig.random(5, 15);
    //uint64_t rbk = ig.random(1, 100); //XXX no rollbacks

    uint64_t ol_i_ids[15];
    uint64_t ol_supply_w_ids[15];
    uint64_t ol_quantities[15];

    uint32_t o_entry_d = ig.gen_date();

    bool all_local = true;

    for (uint64_t i = 0; i < num_items; ++i) {
        uint64_t ol_i_id = ig.gen_item_id();
        //XXX no rollbacks
        //if ((i == (num_items - 1)) && rbk == 1)
        //    ol_i_ids[i] = 0;
        //else
        ol_i_ids[i] = ol_i_id;

        bool supply_from_remote = (ig.num_warehouses() > 1) && (ig.random(1, 100) == 1);
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

    size_t starts = 0;

    // begin txn
    TRANSACTION {
    ++starts;

    bool abort, result;
    uintptr_t row;
    const void *value;

    int64_t wh_tax_rate, dt_tax_rate;
    uint64_t dt_next_oid;

    auto& wv = db.get_warehouse(q_w_id);
    wh_tax_rate = wv.cv.w_tax;

    district_key dk(q_w_id, q_d_id);
    std::tie(abort, result, row, value) = db.tbl_districts_const(q_w_id).select_row(dk, RowAccess::None);
    TXN_DO(abort);
    assert(result);
    auto dv = reinterpret_cast<const district_const_value*>(value);
    dt_tax_rate = dv->d_tax;
    dt_next_oid = db.oid_generator().next(q_w_id, q_d_id);
    //dt_next_oid = new_dv->d_next_o_id ++;
    //db.tbl_districts(q_w_id).update_row(row, new_dv);

    customer_key ck(q_w_id, q_d_id, q_c_id);
    std::tie(abort, result, std::ignore, value) = db.tbl_customers_const(q_w_id).select_row(ck, RowAccess::None);
    TXN_DO(abort);
    assert(result);

    auto ccv = reinterpret_cast<const customer_const_value*>(value);

    auto cus_discount = ccv->c_discount;
    out_cus_last = ccv->c_last;
    out_cus_credit = ccv->c_credit;

    order_key ok(q_w_id, q_d_id, dt_next_oid);
    order_value* ov = Sto::tx_alloc<order_value>();
    ov->o_c_id = q_c_id;
    ov->o_carrier_id = 0;
    ov->o_all_local = all_local ? 1 : 0;
    ov->o_entry_d = o_entry_d;
    ov->o_ol_cnt = num_items;

    order_cidx_key ock(q_w_id, q_d_id, q_c_id, dt_next_oid);

    std::tie(abort, result) = db.tbl_orders(q_w_id).insert_row(ok, ov, false);
    TXN_DO(abort);
    assert(!result);
    std::tie(abort, result) = db.tbl_neworders(q_w_id).insert_row(ok, &bench::dummy_row::row, false);
    TXN_DO(abort);
    assert(!result);
    std::tie(abort, result) = db.tbl_order_customer_index(q_w_id).insert_row(ock, &bench::dummy_row::row, false);
    TXN_DO(abort);
    assert(!result);

    for (uint64_t i = 0; i < num_items; ++i) {
        uint64_t iid = ol_i_ids[i];
        uint64_t wid = ol_supply_w_ids[i];
        uint64_t qty = ol_quantities[i];

        std::tie(abort, result, std::ignore, value) = db.tbl_items().select_row(item_key(iid), RowAccess::ObserveValue);
        TXN_DO(abort);
        assert(result);
        uint64_t oid = reinterpret_cast<const item_value *>(value)->i_im_id;
        TXN_DO(oid != 0);
        uint32_t i_price = reinterpret_cast<const item_value *>(value)->i_price;
        out_item_names[i] = reinterpret_cast<const item_value *>(value)->i_name;
        auto i_data = reinterpret_cast<const item_value *>(value)->i_data;

        std::tie(abort, result, row, value) = db.tbl_stocks(wid).select_row(stock_key(wid, iid), {{st_nc::s_quantity, true}, {st_nc::s_dists, false}, {st_nc::s_data, false}, {st_nc::s_ytd, true}, {st_nc::s_order_cnt, true}, {st_nc::s_remote_cnt, true}});
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
        db.tbl_stocks(wid).update_row(row, new_sv);

        double ol_amount = qty * i_price/100.0;

        orderline_key olk(q_w_id, q_d_id, dt_next_oid, i);
        orderline_value *olv = Sto::tx_alloc<orderline_value>();
        olv->ol_i_id = iid;
        olv->ol_supply_w_id = wid;
        olv->ol_delivery_d = 0;
        olv->ol_quantity = qty;
        olv->ol_amount = ol_amount;
        olv->ol_dist_info = s_dist;

        std::tie(abort, result) = db.tbl_orderlines(q_w_id).insert_row(olk, olv, false);
        TXN_DO(abort);
        assert(!result);

        out_total_amount += ol_amount * (1.0 - cus_discount/100.0) * (1.0 + (wh_tax_rate + dt_tax_rate)/100.0);
    }

    // commit txn
    // retry until commits
    } RETRY(true);

    TXP_INCREMENT(txp_tpcc_no_commits);
    TXP_ACCOUNT(txp_tpcc_no_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_payment() {

    //fprintf(stdout, "PAYMENT\n");
    uint64_t q_w_id = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);

    uint64_t q_c_w_id, q_c_d_id, q_c_id;
    std::string last_name;
    auto x = ig.random(1, 100);
    auto y = ig.random(1, 100);

    bool is_home = (ig.num_warehouses() == 1) || (x <= 85);
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
        last_name = ig.gen_customer_last_name_run();
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
    (void)out_c_since;
    (void)out_c_credit_lim;
    (void)out_c_discount;
    (void)out_c_balance;

    size_t starts = 0;

    // begin txn
    TRANSACTION {
    Sto::transaction()->special_txp = true;
    ++starts;

    bool success, result;
    uintptr_t row;
    const void *value;

    // select warehouse row FOR UPDATE and retrieve warehouse info
    warehouse_key wk(q_w_id);
    auto& wv = db.get_warehouse(q_w_id);

    out_w_name = wv.cv.w_name;
    out_w_street_1 = wv.cv.w_street_1;
    out_w_street_2 = wv.cv.w_street_2;
    out_w_city = wv.cv.w_city;
    out_w_state = wv.cv.w_state;
    out_w_zip = wv.cv.w_zip;

    // update warehouse ytd
    wv.ytd.increment(h_amount);

    // select district row and retrieve district info
    district_key dk(q_w_id, q_d_id);
    std::tie(success, result, row, value) = db.tbl_districts_const(q_w_id).select_row(dk, RowAccess::None);
    TXN_DO(success);
    assert(result);
    auto dv = reinterpret_cast<const district_const_value *>(value);
    out_d_name = dv->d_name;
    out_d_street_1 = dv->d_street_1;
    out_d_street_2 = dv->d_street_2;
    out_d_city = dv->d_city;
    out_d_state = dv->d_state;
    out_d_zip = dv->d_zip;

    std::tie(success, result, row, value) = db.tbl_districts_comm(q_w_id).select_row(dk,
            DBParams::MVCC ? RowAccess::None : RowAccess::UpdateValue);
    TXN_DO(success);
    assert(result);

    TXP_INCREMENT(txp_tpcc_pm_stage1);

    if (DBParams::MVCC) {
        // update district ytd commutatively
        commutators::MvCommutator<district_comm_value> commutator(h_amount);
        db.tbl_districts_comm(q_w_id).update_row(row, commutator);
    } else {
        auto dmv = reinterpret_cast<const district_comm_value*>(value);
        auto new_dmv = Sto::tx_alloc(dmv);
        // update district ytd in-place
        new_dmv->d_ytd += h_amount;
        db.tbl_districts_comm(q_w_id).update_row(row, new_dmv);
    }

    TXP_INCREMENT(txp_tpcc_pm_stage2);

    // select and update customer
    if (by_name) {
        std::vector<uint64_t> matches;
        auto scan_callback = [&] (const customer_idx_key& key, const customer_idx_value& civ) -> bool {
            (void)key;
            matches.emplace_back(civ.c_id);
            return true;
        };

        customer_idx_key ck0(q_c_w_id, q_c_d_id, last_name, 0x00 /*fill first name*/);
        customer_idx_key ck1(q_c_w_id, q_c_d_id, last_name, 0xff);

        success = db.tbl_customer_index(q_c_w_id)
                .template range_scan<decltype(scan_callback), false/*reverse*/>(ck0, ck1, scan_callback, RowAccess::ObserveValue);
        TXN_DO(success);

        size_t n = matches.size();
        always_assert(n > 0, "match size invalid");
        size_t idx = n / 2;
        if (n % 2 == 0)
            idx -= 1;
        q_c_id = matches[idx];

    } else {
        always_assert(q_c_id != 0, "q_c_id invalid when selecting customer by c_id");
    }

    TXP_INCREMENT(txp_tpcc_pm_stage3);

    customer_key ck(q_c_w_id, q_c_d_id, q_c_id);
    std::tie(success, result, row, value) = db.tbl_customers_const(q_c_w_id).select_row(ck, RowAccess::None);
    TXN_DO(success);
    assert(result);

    TXP_INCREMENT(txp_tpcc_pm_stage4);

    auto ccv = reinterpret_cast<const customer_const_value*>(value);
    out_c_since = ccv->c_since;
    out_c_credit_lim = ccv->c_credit_lim;
    out_c_discount = ccv->c_discount;

#if TPCC_OBSERVE_C_BALANCE
    std::tie(success, result, row, value) = db.tbl_customers_comm(q_c_w_id).select_row(ck, RowAccess::UpdateValue);
    TXN_DO(success);
    assert(result);
    auto cmv = reinterpret_cast<const customer_comm_value*>(value);
    out_c_balance = cmv->c_balance;
#else
    std::tie(success, result, row, value) = db.tbl_customers_comm(q_c_w_id).select_row(ck,
            DBParams::MVCC ? RowAccess::None : RowAccess::UpdateValue);
    TXN_DO(success);
    assert(result);
#endif

    if (DBParams::MVCC) {
        if (ccv->c_credit == "BC") {
            commutators::MvCommutator<customer_comm_value> commutator(-h_amount, h_amount, q_c_id, q_c_d_id, q_c_w_id,
                                                                      q_d_id, q_w_id, h_amount);
            db.tbl_customers_comm(q_c_w_id).update_row(row, commutator);
        } else {
            commutators::MvCommutator<customer_comm_value> commutator(-h_amount, h_amount);
            db.tbl_customers_comm(q_c_w_id).update_row(row, commutator);
        }
    } else {
        auto cmmv = reinterpret_cast<const customer_comm_value*>(value);
        auto new_cmmv = Sto::tx_alloc(cmmv);
        new_cmmv->c_balance -= h_amount;
        new_cmmv->c_payment_cnt += 1;
        new_cmmv->c_ytd_payment += h_amount;
        if (ccv->c_credit == "BC") {
            c_data_info info(q_c_id, q_c_d_id, q_c_w_id, q_d_id, q_w_id, h_amount);
            new_cmmv->c_data.insert_left(info.buf(), c_data_info::len);
        }
        db.tbl_customers_comm(q_c_w_id).update_row(row, new_cmmv);
    }

    TXP_INCREMENT(txp_tpcc_pm_stage5);

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

    history_key hk(db.tbl_histories(q_c_w_id).gen_key());
    std::tie(success, result) = db.tbl_histories(q_c_w_id).insert_row(hk, hv);
    assert(success);
    assert(!result);

    TXP_INCREMENT(txp_tpcc_pm_stage6);

    // commit txn
    // retry until commits
    } RETRY(true);

    TXP_INCREMENT(txp_tpcc_pm_commits);
    TXP_ACCOUNT(txp_tpcc_pm_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_orderstatus() {
    typedef order_value::NamedColumn od_nc;

    uint64_t q_w_id = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);

    std::string last_name;
    uint64_t q_c_id;

    auto x = ig.random(1, 100);
    bool by_name = (x <= 60);
    if (by_name) {
        last_name = ig.gen_customer_last_name_run();
        q_c_id = 0;
    } else {
        q_c_id = ig.gen_customer_id();
    }

    // holding outputs of the transaction
    volatile var_string<16> out_c_first, out_c_last;
    volatile fix_string<2> out_c_middle;
    volatile int64_t out_c_balance;
    volatile uint64_t out_o_carrier_id;
    volatile uint32_t out_o_entry_date;
    volatile uint64_t out_ol_i_id;
    volatile uint64_t out_ol_supply_w_id;
    volatile uint32_t out_ol_quantity;
    volatile uint32_t out_ol_delivery_d;
    volatile int32_t out_ol_amount;
    (void)out_c_balance;
    (void)out_o_carrier_id;
    (void)out_o_entry_date;

    size_t starts = 0;

    TRANSACTION {
    ++starts;

    bool success, result;
    uintptr_t row;
    const void *value;

    if (by_name) {
        std::vector<uint64_t> matches;
        auto scan_callback = [&] (const customer_idx_key&, const customer_idx_value& civ) -> bool {
            matches.emplace_back(civ.c_id);
            return true;
        };

        customer_idx_key ck0(q_w_id, q_d_id, last_name, 0x00);
        customer_idx_key ck1(q_w_id, q_d_id, last_name, 0xff);

        success = db.tbl_customer_index(q_w_id)
                .template range_scan<decltype(scan_callback), false/*reverse*/>(ck0, ck1, scan_callback, RowAccess::ObserveValue);
        TXN_DO(success);

        size_t n = matches.size();
        always_assert(n > 0, "match size invalid");
        size_t idx = n / 2;
        if (n % 2 == 0)
            idx -= 1;
        q_c_id = matches[idx];
    } else {
        always_assert(q_c_id != 0, "q_c_id invalid when selecting customer by c_id");
    }

    customer_key ck(q_w_id, q_d_id, q_c_id);
    std::tie(success, result, row, value) = db.tbl_customers_const(q_w_id).select_row(ck, RowAccess::None);
    TXN_DO(success);
    assert(result);

    auto ccv = reinterpret_cast<const customer_const_value*>(value);

    // simulate retrieving customer info
    out_c_first = ccv->c_first;
    out_c_last = ccv->c_last;
    out_c_middle = ccv->c_middle;

#if TPCC_OBSERVE_C_BALANCE
    std::tie(success, result, row, value) = db.tbl_customers_comm(q_w_id).select_row(ck, RowAccess::ObserveValue);
    auto cmv = reinterpret_cast<const customer_comm_value*>(value);
    out_c_balance = cmv->c_balance;
#endif

    // find the highest order placed by customer q_c_id
    uint64_t cus_o_id = 0;
    auto scan_callback = [&] (const order_cidx_key& key, const bench::dummy_row&) -> bool {
        cus_o_id = bswap(key.o_id);
        return true;
    };

    order_cidx_key k0(q_w_id, q_d_id, q_c_id, 0);
    order_cidx_key k1(q_w_id, q_d_id, q_c_id, std::numeric_limits<uint64_t>::max());

    success = db.tbl_order_customer_index(q_w_id)
            .template range_scan<decltype(scan_callback), true/*reverse*/>(k1, k0, scan_callback, RowAccess::ObserveExists, false, 1/*reverse scan for only 1 item*/);
    TXN_DO(success);

    if (cus_o_id > 0) {
        order_key ok(q_w_id, q_d_id, cus_o_id);
        std::tie(success, result, row, value) = db.tbl_orders(q_w_id).select_row(ok, {{od_nc::o_entry_d, false}, {od_nc::o_carrier_id, false}});
        TXN_DO(success);
        assert(result);

        auto ov = reinterpret_cast<const order_value *>(value);
        out_o_entry_date = ov->o_entry_d;
        out_o_carrier_id = ov->o_carrier_id;

        auto ol_scan_callback = [&] (const orderline_key& olk, const orderline_value& olv) -> bool {
            (void)olk;
            out_ol_i_id = olv.ol_i_id;
            out_ol_supply_w_id = olv.ol_supply_w_id;
            out_ol_quantity = olv.ol_quantity;
            out_ol_amount = olv.ol_amount;
            out_ol_delivery_d = olv.ol_delivery_d;
            return true;
        };

        orderline_key olk0(q_w_id, q_d_id, cus_o_id, 0);
        orderline_key olk1(q_w_id, q_d_id, cus_o_id, std::numeric_limits<uint64_t>::max());

        success = db.tbl_orderlines(q_w_id)
                .template range_scan<decltype(ol_scan_callback), false/*reverse*/>(olk0, olk1, ol_scan_callback, RowAccess::ObserveValue);
        TXN_DO(success);
    } else {
        // order doesn't exist, simply commit the transaction
    }

    // commit txn
    // retry until commits
    } RETRY(true);

    TXP_INCREMENT(txp_tpcc_os_commits);
    TXP_ACCOUNT(txp_tpcc_os_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_delivery() {
    typedef order_value::NamedColumn od_nc;

    uint64_t q_w_id = ig.random(w_id_start, w_id_end);
    uint64_t carrier_id = ig.random(1, 10);
    uint32_t delivery_date = ig.gen_date();

    bool success, result;
    uintptr_t row;
    const void *value;

    uint64_t order_id;
    std::vector<uint64_t> ol_nums;
    int32_t ol_amount_sum;

    auto no_scan_callback = [&order_id] (const order_key& ok, const bench::dummy_row&) -> bool {
        order_id = bswap(ok.o_id);
        return true;
    };

    size_t starts = 0;

    TRANSACTION {
    ++starts;

    for (uint64_t q_d_id = 1; q_d_id <= 10; ++q_d_id) {
        order_id = 0;

        order_key k0(q_w_id, q_d_id, 0);
        order_key k1(q_w_id, q_d_id, std::numeric_limits<uint64_t>::max());
        success = db.tbl_neworders(q_w_id)
                .template range_scan<decltype(no_scan_callback), false/*reverse*/>(k0, k1, no_scan_callback, RowAccess::ObserveValue, false, 1);
        TXN_DO(success);

        if (order_id == 0)
            continue;

        order_key ok(q_w_id, q_d_id, order_id);
        //std::tie(success, result) = db.tbl_neworders(q_w_id).delete_row(ok);
        //TXN_DO(success);
        //assert(result);

        std::tie(success, result, row, value) = db.tbl_orders(q_w_id).select_row(ok, {{od_nc::o_ol_cnt, false}, {od_nc::o_c_id, false}, {od_nc::o_carrier_id, true}});
        TXN_DO(success);
        assert(result);

        auto ov = reinterpret_cast<const order_value *>(value);
        uint64_t q_c_id = ov->o_c_id;
        auto ol_cnt = ov->o_ol_cnt;

        order_value *new_ov = Sto::tx_alloc(ov);
        new_ov->o_carrier_id = carrier_id;
        db.tbl_orders(q_w_id).update_row(row, new_ov);

        ol_amount_sum = 0;

        for (uint32_t ol_num = 1; ol_num <= ol_cnt; ++ol_num) {
            std::tie(success, result, row, value) = db.tbl_orderlines(q_w_id).select_row(orderline_key(q_w_id, q_d_id, order_id, ol_num), RowAccess::UpdateValue);
            TXN_DO(success);

            assert(result);
            auto olv = reinterpret_cast<const orderline_value *>(value);
            ol_amount_sum += olv->ol_amount;
            orderline_value *new_olv = Sto::tx_alloc(olv);
            new_olv->ol_delivery_d = delivery_date;
            db.tbl_orderlines(q_w_id).update_row(row, new_olv);
        }

        customer_key ck(q_w_id, q_d_id, q_c_id);
        std::tie(success, result, row, value) = db.tbl_customers_comm(q_w_id).select_row(ck,
                DBParams::MVCC ? RowAccess::None : RowAccess::UpdateValue);
        TXN_DO(success);
        assert(result);

        if (DBParams::MVCC) {
            commutators::MvCommutator<customer_comm_value> commutator((int64_t)ol_amount_sum);
            db.tbl_customers_comm(q_w_id).update_row(row, commutator);
        } else {
            auto cmv = reinterpret_cast<const customer_comm_value*>(value);
            auto new_cmv = Sto::tx_alloc(cmv);
            new_cmv->c_balance += (int64_t)ol_amount_sum;
            new_cmv->c_delivery_cnt += 1;
            db.tbl_customers_comm(q_w_id).update_row(row, new_cmv);
        }
    }

    } RETRY(true);

    TXP_INCREMENT(txp_tpcc_dl_commits);
    TXP_ACCOUNT(txp_tpcc_dl_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_stocklevel(){
    typedef stock_value::NamedColumn st_nc;

    uint64_t q_w_id = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);
    auto threshold = (int32_t)ig.random(10, 20);

    std::set<uint64_t> ol_iids;

    volatile int out_count = 0;
    (void)out_count;

    bool success, result;
    const void *value;

    auto ol_scan_callback = [ &ol_iids] (const orderline_key&, const orderline_value& value) -> bool {
        ol_iids.insert(value.ol_i_id);
        return true;
    };

    size_t starts = 0;

    TRANSACTION {
    ++starts;

    ol_iids.clear();
    auto d_next_oid = db.oid_generator().get(q_w_id, q_d_id);

    uint64_t oid_lower = (d_next_oid > 20) ? d_next_oid - 20 : 0;
    orderline_key olk0(q_w_id, q_d_id, oid_lower, 0);
    orderline_key olk1(q_w_id, q_d_id, d_next_oid, 0);

    success = db.tbl_orderlines(q_w_id)
            .template range_scan<decltype(ol_scan_callback), false/*reverse*/>(olk1, olk0, ol_scan_callback, RowAccess::ObserveValue);
    TXN_DO(success);

    for (auto iid : ol_iids) {
        stock_key sk(q_w_id, iid);
        std::tie(success, result, std::ignore, value) = db.tbl_stocks(q_w_id).select_row(sk, {{st_nc::s_quantity, false}});
        TXN_DO(success);
        assert(result);
        auto sv = reinterpret_cast<const stock_value *>(value);
        if(sv->s_quantity < threshold) {
            out_count += 1;
        }
    }

    } RETRY(true);

    TXP_INCREMENT(txp_tpcc_st_commits);
    TXP_ACCOUNT(txp_tpcc_st_aborts, starts - 1);
}

}; // namespace tpcc
