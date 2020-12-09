#pragma once

#include "TPCC_bench.hh"
#include <set>

#ifndef TPCC_OBSERVE_C_BALANCE
#define TPCC_OBSERVE_C_BALANCE 1
#endif

namespace tpcc {

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_neworder() {
    typedef warehouse_value::NamedColumn wh_nc;
    typedef district_value::NamedColumn dt_nc;
    typedef customer_value::NamedColumn cu_nc;
    typedef item_value::NamedColumn it_nc;
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
    var_string<16> out_cus_last;
    fix_string<2> out_cus_credit;
    var_string<24> out_item_names[15];
    double out_total_amount = 0.0;
    char out_brand_generic[15];
    (void) out_brand_generic;

    size_t starts = 0;

    // begin txn
    RWTXN {
    ++starts;

    int64_t wh_tax_rate, dt_tax_rate;
    uint64_t dt_next_oid;

    {
    warehouse_key wk(q_w_id);
    auto [abort, result, row, value] = db.tbl_warehouses().select_split_row(wk,
        {{wh_nc::w_tax, access_t::read}}
    );
    (void)row; (void)result;
    CHK(abort);
    assert(result);
    wh_tax_rate = value.w_tax();
    }

    {
    district_key dk(q_w_id, q_d_id);
    auto [abort, result, row, value] = db.tbl_districts(q_w_id).select_split_row(dk,
        {{dt_nc::d_tax, access_t::read}}
    );
    (void)row; (void)result;
    CHK(abort);
    assert(result);

    TXP_INCREMENT(txp_tpcc_no_stage1);

    dt_tax_rate = value.d_tax();
    dt_next_oid = db.oid_generator().next(q_w_id, q_d_id);
    //dt_next_oid = new_dv->d_next_o_id ++;
    //db.tbl_districts(q_w_id).update_row(row, new_dv);
    }

    int64_t cus_discount;
    {
    customer_key ck(q_w_id, q_d_id, q_c_id);
    auto [abort, result, row, value] = db.tbl_customers(q_w_id).select_split_row(ck,
        {{cu_nc::c_discount, access_t::read},
         {cu_nc::c_last, access_t::read},
         {cu_nc::c_credit, access_t::read}}
    );
    (void)row; (void)result;
    CHK(abort);
    assert(result);

    TXP_INCREMENT(txp_tpcc_no_stage2);

    cus_discount = value.c_discount();
    out_cus_last = value.c_last();
    out_cus_credit = value.c_credit();
    }

    order_key ok(q_w_id, q_d_id, dt_next_oid);
    order_cidx_key ock(q_w_id, q_d_id, q_c_id, dt_next_oid);
    order_value* ov = Sto::tx_alloc<order_value>();
    ov->o_c_id = q_c_id;
    ov->o_carrier_id = 0;
    ov->o_all_local = all_local ? 1 : 0;
    ov->o_entry_d = o_entry_d;
    ov->o_ol_cnt = num_items;

    {
    auto [abort, result] = db.tbl_orders(q_w_id).insert_row(ok, ov, false);
    (void)result;
    CHK(abort);
    assert(!result);

    std::tie(abort, result) = db.tbl_neworders(q_w_id).insert_row(ok, &bench::dummy_row::row, false);
    (void)result;
    CHK(abort);
    assert(!result);
    std::tie(abort, result) = db.tbl_order_customer_index(q_w_id).insert_row(ock, &bench::dummy_row::row, false);
    (void)result;
    CHK(abort);
    assert(!result);
    }

    TXP_INCREMENT(txp_tpcc_no_stage3);

    TXP_ACCOUNT(txp_tpcc_no_stage4, num_items);

    for (uint64_t i = 0; i < num_items; ++i) {
        uint64_t iid = ol_i_ids[i];
        uint64_t wid = ol_supply_w_ids[i];
        uint64_t qty = ol_quantities[i];
        uint64_t oid;
        uint32_t i_price;

        {
        auto [abort, result, row, value] = db.tbl_items().select_split_row(item_key(iid),
            {{it_nc::i_im_id, access_t::read},
             {it_nc::i_price, access_t::read},
             {it_nc::i_name, access_t::read},
             {it_nc::i_data, access_t::read}});
        (void)row; (void)result;
        CHK(abort);
        assert(result);
        oid = value.i_im_id();
        CHK(oid != 0);
        i_price = value.i_price();
        out_item_names[i] = value.i_name();
        //auto i_data = reinterpret_cast<const item_value *>(value)->i_data;
        }

        {
        auto [abort, result, row, value] = db.tbl_stocks(wid).select_split_row(stock_key(wid, iid),
            {{st_nc::s_quantity, Commute ? access_t::write : access_t::update},
             {st_nc::s_ytd, Commute ? access_t::write : access_t::update},
             {st_nc::s_order_cnt, Commute ? access_t::write : access_t::update},
             {st_nc::s_remote_cnt, Commute ? access_t::write : access_t::update},
             {st_nc::s_dists, access_t::read },
             {st_nc::s_data, access_t::read }}
        );
        (void)result;
        CHK(abort);
        assert(result);
        int32_t s_quantity = value.s_quantity();
        auto s_dist = value.s_dists()[q_d_id - 1];
        //auto s_data = sv->s_data;
        //if (i_data.contains("ORIGINAL") && s_data.contains("ORIGINAL"))
        //    out_brand_generic[i] = 'B';
        //else
        //    out_brand_generic[i] = 'G';

        if constexpr (Commute) {
            commutators::Commutator<stock_value> comm(qty, wid != q_w_id);
            db.tbl_stocks(wid).update_row(row, comm);
        } else {
            stock_value* new_sv = Sto::tx_alloc<stock_value>();
            value.copy_into(new_sv);
            if ((s_quantity - 10) >= (int32_t) qty)
                new_sv->s_quantity -= qty;
            else
                new_sv->s_quantity += (91 - (int32_t) qty);
            new_sv->s_ytd += qty;
            new_sv->s_order_cnt += 1;
            if (wid != q_w_id)
                new_sv->s_remote_cnt += 1;
            db.tbl_stocks(wid).update_row(row, new_sv);
        }

        double ol_amount = qty * i_price/100.0;

        orderline_key olk(q_w_id, q_d_id, dt_next_oid, i + 1);
        orderline_value *olv = Sto::tx_alloc<orderline_value>();
        olv->ol_i_id = iid;
        olv->ol_supply_w_id = wid;
        olv->ol_delivery_d = 0;
        olv->ol_quantity = qty;
        olv->ol_amount = ol_amount;
        olv->ol_dist_info = s_dist;

        std::tie(abort, result) = db.tbl_orderlines(q_w_id).insert_row(olk, olv, false);
        (void)result;
        CHK(abort);
        assert(!result);

        out_total_amount += ol_amount * (1.0 - cus_discount/100.0) * (1.0 + (wh_tax_rate + dt_tax_rate)/100.0);

        TXP_INCREMENT(txp_tpcc_no_stage5);
        }
    }

    // commit txn
    // retry until commits
    } TEND(true);

    TXP_INCREMENT(txp_tpcc_no_commits);
    TXP_ACCOUNT(txp_tpcc_no_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_payment() {
    typedef warehouse_value::NamedColumn wh_nc;
    typedef district_value::NamedColumn dt_nc;
    typedef customer_value::NamedColumn cu_nc;

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
    var_string<10> out_w_name, out_d_name;
    var_string<20> out_w_street_1, out_w_street_2, out_w_city;
    var_string<20> out_d_street_1, out_d_street_2, out_d_city;
    fix_string<2> out_w_state, out_d_state;
    fix_string<9> out_w_zip, out_d_zip;
    var_string<16> out_c_first, out_c_last;
    fix_string<2> out_c_middle;
    var_string<20> out_c_street_1, out_c_street_2, out_c_city;
    fix_string<2> out_c_state;
    fix_string<9> out_c_zip;
    fix_string<16> out_c_phone;
    fix_string<2> out_c_credit;
    uint32_t out_c_since;
    int64_t out_c_credit_lim;
    int64_t out_c_discount;
    int64_t out_c_balance;
    (void)out_c_since;
    (void)out_c_credit_lim;
    (void)out_c_discount;
    (void)out_c_balance;

    size_t starts = 0;

    // begin txn
    RWTXN {
    Sto::transaction()->special_txp = true;
    ++starts;

    // select warehouse row for update and retrieve warehouse info
    {
    warehouse_key wk(q_w_id);
    auto [success, result, row, value] = db.tbl_warehouses().select_split_row(wk,
        {{wh_nc::w_name, access_t::read},
         {wh_nc::w_street_1, access_t::read},
         {wh_nc::w_street_2, access_t::read},
         {wh_nc::w_city, access_t::read},
         {wh_nc::w_state, access_t::read},
         {wh_nc::w_zip, access_t::read},
         {wh_nc::w_ytd, Commute ? access_t::write : access_t::update}}
    );
    (void)result;
    CHK(success);
    assert(result);
    out_w_name = value.w_name();
    out_w_street_1 = value.w_street_1();
    out_w_street_2 = value.w_street_2();
    out_w_city = value.w_city();
    out_w_state = value.w_state();
    out_w_zip = value.w_zip();

    // update warehouse ytd
    if constexpr (Commute) {
        commutators::Commutator<warehouse_value> commutator(h_amount);
        db.tbl_warehouses().update_row(row, commutator);
    } else {
        auto new_wv = Sto::tx_alloc<warehouse_value>();
        value.copy_into(new_wv);
        new_wv->w_ytd += h_amount;
        db.tbl_warehouses().update_row(row, new_wv);
    }
    }

    // select district row and retrieve district info
    {
    district_key dk(q_w_id, q_d_id);
    auto [success, result, row, value] = db.tbl_districts(q_w_id).select_split_row(dk,
        {{dt_nc::d_name, access_t::read},
         {dt_nc::d_street_1, access_t::read},
         {dt_nc::d_street_2, access_t::read},
         {dt_nc::d_city, access_t::read},
         {dt_nc::d_state, access_t::read},
         {dt_nc::d_zip, access_t::read},
         {dt_nc::d_ytd, Commute ? access_t::write : access_t::update}}
    );
    (void)result;
    CHK(success);
    assert(result);
    out_d_name = value.d_name();
    out_d_street_1 = value.d_street_1();
    out_d_street_2 = value.d_street_2();
    out_d_city = value.d_city();
    out_d_state = value.d_state();
    out_d_zip = value.d_zip();

    TXP_INCREMENT(txp_tpcc_pm_stage1);

    if constexpr (Commute) {
        // update district ytd commutatively
        commutators::Commutator<district_value> commutator(h_amount);
        db.tbl_districts(q_w_id).update_row(row, commutator);
    } else {
        auto new_dv = Sto::tx_alloc<district_value>();
        value.copy_into(new_dv);
        // update district ytd in-place
        new_dv->d_ytd += h_amount;
        db.tbl_districts(q_w_id).update_row(row, new_dv);
    }

    TXP_INCREMENT(txp_tpcc_pm_stage2);
    }

    // select and update customer
    if (by_name) {
        customer_idx_key ck(q_c_w_id, q_c_d_id, last_name);
        auto [success, result, row, value] = db.tbl_customer_index(q_c_w_id).select_split_row(ck,
            {{customer_idx_value::NamedColumn::c_ids, access_t::read}});
        (void)row; (void)result;
        CHK(success);
        assert(result);
        auto& c_id_list = value.c_ids();
        uint64_t rows[100];
        int cnt = 0;
        for (auto it = c_id_list.begin(); cnt < 100 && it != c_id_list.end(); ++it, ++cnt) {
            rows[cnt] = *it;
        }
        q_c_id = rows[cnt / 2];
    } else {
        always_assert(q_c_id != 0, "q_c_id invalid when selecting customer by c_id");
    }

    TXP_INCREMENT(txp_tpcc_pm_stage3);

    customer_key ck(q_c_w_id, q_c_d_id, q_c_id);
    auto [success, result, row, value] = db.tbl_customers(q_c_w_id).select_split_row(ck,
        {{cu_nc::c_since,    access_t::read},
         {cu_nc::c_credit,   access_t::read},
         {cu_nc::c_discount, access_t::read},
         {cu_nc::c_balance, access_t::update},
         {cu_nc::c_payment_cnt, Commute ? access_t::write : access_t::update},
         {cu_nc::c_ytd_payment, Commute ? access_t::write : access_t::update},
         {cu_nc::c_credit, Commute ? access_t::write : access_t::update}}
    );
    (void)result;
    CHK(success);
    assert(result);

    TXP_INCREMENT(txp_tpcc_pm_stage4);

    out_c_since = value.c_since();
    out_c_credit_lim = value.c_credit_lim();
    out_c_discount = value.c_discount();
    out_c_balance = value.c_balance();

    if constexpr (Commute) {
        if (value.c_credit() == "BC") {
            commutators::Commutator<customer_value> commutator(-h_amount, h_amount, q_c_id, q_c_d_id, q_c_w_id,
                                                                      q_d_id, q_w_id, h_amount);
            db.tbl_customers(q_c_w_id).update_row(row, commutator);
        } else {
            commutators::Commutator<customer_value> commutator(-h_amount, h_amount);
            db.tbl_customers(q_c_w_id).update_row(row, commutator);
        }
    } else {
        auto new_cv = Sto::tx_alloc<customer_value>();
        value.copy_into(new_cv);
        new_cv->c_balance -= h_amount;
        new_cv->c_payment_cnt += 1;
        new_cv->c_ytd_payment += h_amount;
        if (value.c_credit() == "BC") {
            c_data_info info(q_c_id, q_c_d_id, q_c_w_id, q_d_id, q_w_id, h_amount);
            new_cv->c_data.insert_left(info.buf(), c_data_info::len);
        }
        db.tbl_customers(q_c_w_id).update_row(row, new_cv);
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

#if HISTORY_SEQ_INSERT
    history_key hk(db.tbl_histories(q_c_w_id).gen_key());
#else
    history_key hk(q_w_id, q_d_id, q_c_id, db.tbl_histories(q_c_w_id).gen_key());
#endif
    std::tie(success, result) = db.tbl_histories(q_c_w_id).insert_row(hk, hv);
    assert(success);
    assert(!result);

    TXP_INCREMENT(txp_tpcc_pm_stage6);

    // commit txn
    // retry until commits
    } TEND(true);

    TXP_INCREMENT(txp_tpcc_pm_commits);
    TXP_ACCOUNT(txp_tpcc_pm_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_orderstatus() {
    typedef customer_value::NamedColumn cu_nc;
    typedef order_value::NamedColumn od_nc;
    typedef orderline_value::NamedColumn ol_nc;
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
    var_string<16> out_c_first, out_c_last;
    fix_string<2> out_c_middle;
    int64_t out_c_balance;
    uint64_t out_o_carrier_id;
    uint32_t out_o_entry_date;
    uint64_t out_ol_i_id;
    uint64_t out_ol_supply_w_id;
    uint32_t out_ol_quantity;
    uint32_t out_ol_delivery_d;
    int32_t out_ol_amount;
    (void)out_c_balance;
    (void)out_o_carrier_id;
    (void)out_o_entry_date;

    size_t starts = 0;

    TXN {
    ++starts;

    if (by_name) {
        customer_idx_key ck(q_w_id, q_d_id, last_name);
        auto [success, result, row, value] = db.tbl_customer_index(q_w_id).select_split_row(ck,
            {{customer_idx_value::NamedColumn::c_ids, access_t::read}});
        (void)row; (void)result;
        CHK(success);
        assert(result);
        auto& c_id_list = value.c_ids();
        uint64_t rows[100];
        int cnt = 0;
        for (auto it = c_id_list.begin(); cnt < 100 && it != c_id_list.end(); ++it, ++cnt) {
            rows[cnt] = *it;
        }
        q_c_id = rows[cnt / 2];
    } else {
        always_assert(q_c_id != 0, "q_c_id invalid when selecting customer by c_id");
    }

    customer_key ck(q_w_id, q_d_id, q_c_id);
    auto [success, result, row, value] = db.tbl_customers(q_w_id).select_split_row(ck,
        {{cu_nc::c_first, access_t::read},
         {cu_nc::c_last, access_t::read},
         {cu_nc::c_middle, access_t::read},
         {cu_nc::c_balance, access_t::read}}
    );
    (void)row; (void)result;
    CHK(success);
    assert(result);

    // simulate retrieving customer info
    out_c_first = value.c_first();
    out_c_last = value.c_last();
    out_c_middle = value.c_middle();
    out_c_balance = value.c_balance();

    // find the highest order placed by customer q_c_id
    uint64_t cus_o_id = 0;
    auto scan_callback = [&] (const order_cidx_key& key, const auto&) -> bool {
        cus_o_id = bswap(key.o_id);
        return true;
    };

    order_cidx_key k0(q_w_id, q_d_id, q_c_id, 0);
    order_cidx_key k1(q_w_id, q_d_id, q_c_id, std::numeric_limits<uint64_t>::max());

    bool scan_success = db.tbl_order_customer_index(q_w_id)
            .template range_scan<decltype(scan_callback), true/*reverse*/>(k1, k0, scan_callback, RowAccess::ObserveExists, true, 1/*reverse scan for only 1 item*/);
    CHK(scan_success);

    if (cus_o_id > 0) {
        order_key ok(q_w_id, q_d_id, cus_o_id);
        auto [success, result, row, value] = db.tbl_orders(q_w_id).select_split_row(ok,
            {{od_nc::o_entry_d, access_t::read},
             {od_nc::o_carrier_id, access_t::read}});
        (void)row; (void)result;
        CHK(success);
        assert(result);

        out_o_entry_date = value.o_entry_d();
        out_o_carrier_id = value.o_carrier_id();

        auto ol_scan_callback = [&] (const orderline_key&, const auto& scan_value) -> bool {
            auto olv = (typename std::remove_reference_t<decltype(db)>::ol_table_type::accessor_t)(scan_value);
            out_ol_i_id = olv.ol_i_id();
            out_ol_supply_w_id = olv.ol_supply_w_id();
            out_ol_quantity = olv.ol_quantity();
            out_ol_amount = olv.ol_amount();
            out_ol_delivery_d = olv.ol_delivery_d();
            return true;
        };

        orderline_key olk0(q_w_id, q_d_id, cus_o_id, 0);
        orderline_key olk1(q_w_id, q_d_id, cus_o_id, std::numeric_limits<uint64_t>::max());

        scan_success = db.tbl_orderlines(q_w_id)
                .template range_scan<decltype(ol_scan_callback), false/*reverse*/>(olk0, olk1, ol_scan_callback,
                        {{ol_nc::ol_i_id, access_t::read},
                         {ol_nc::ol_supply_w_id, access_t::read},
                         {ol_nc::ol_quantity, access_t::read},
                         {ol_nc::ol_amount, access_t::read},
                         {ol_nc::ol_delivery_d, access_t::read}}
                );
        CHK(scan_success);
    } else {
        // order doesn't exist, simply commit the transaction
    }

    // commit txn
    // retry until commits
    } TEND(true);

    TXP_INCREMENT(txp_tpcc_os_commits);
    TXP_ACCOUNT(txp_tpcc_os_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_delivery(uint64_t q_w_id,
                                             std::array<uint64_t, NUM_DISTRICTS_PER_WAREHOUSE>& last_delivered) {
    typedef order_value::NamedColumn od_nc;
    typedef orderline_value::NamedColumn ol_nc;
    typedef customer_value::NamedColumn cu_nc;

    uint64_t carrier_id = ig.random(1, 10);
    uint32_t delivery_date = ig.gen_date();

    uint64_t order_id;
    std::array<uint64_t, NUM_DISTRICTS_PER_WAREHOUSE> delivered_order_ids;
    std::vector<uint64_t> ol_nums;
    int32_t ol_amount_sum;

    auto no_scan_callback = [&order_id] (const order_key& ok, const auto&) -> bool {
        order_id = bswap(ok.o_id);
        return true;
    };

    size_t starts = 0;

    TXP_INCREMENT(txp_tpcc_dl_stage1);

    RWTXN {
    ++starts;

    for (uint64_t q_d_id = 1; q_d_id <= 10; ++q_d_id) {
        order_id = 0;
        //printf("district %lu\n", q_d_id);
        //Sto::print_read_set_size("0");

        order_key k0(q_w_id, q_d_id, last_delivered[q_d_id - 1]);
        order_key k1(q_w_id, q_d_id, std::numeric_limits<order_key::oid_type>::max());
        bool scan_success = db.tbl_neworders(q_w_id)
                .template range_scan<decltype(no_scan_callback), false/*reverse*/>(k0, k1, no_scan_callback, RowAccess::ObserveValue, true, 1);
        CHK(scan_success);
        //Sto::print_read_set_size("1");

        TXP_INCREMENT(txp_tpcc_dl_stage2);

        delivered_order_ids[q_d_id - 1] = order_id;
        if (order_id == 0)
            continue;

        order_key ok(q_w_id, q_d_id, order_id);
        {
        auto [success, result] = db.tbl_neworders(q_w_id).delete_row(ok);
        CHK(success);
        CHK(result);
        assert(result);
        }

        uint64_t q_c_id = 0;
        uint32_t ol_cnt = 0;
        {
        auto [success, result, row, value] = db.tbl_orders(q_w_id).select_split_row(ok,
            {{od_nc::o_c_id, access_t::read},
             {od_nc::o_ol_cnt, access_t::read},
             {od_nc::o_carrier_id, Commute ? access_t::write : access_t::update}}
        );
        (void)result;
        CHK(success);
        assert(result);

        TXP_INCREMENT(txp_tpcc_dl_stage4);

        q_c_id = value.o_c_id();
        assert(q_c_id != 0);
        ol_cnt = value.o_ol_cnt();

        if constexpr (Commute) {
            commutators::Commutator<order_value> commutator(carrier_id);
            db.tbl_orders(q_w_id).update_row(row, commutator);
        } else {
            order_value* new_ov = Sto::tx_alloc<order_value>();
            value.copy_into(new_ov);
            new_ov->o_carrier_id = carrier_id;
            db.tbl_orders(q_w_id).update_row(row, new_ov);
        }
        }

        {
        ol_amount_sum = 0;
        for (uint32_t ol_num = 1; ol_num <= ol_cnt; ++ol_num) {
            orderline_key olk(q_w_id, q_d_id, order_id, ol_num);
            auto [success, result, row, value] = db.tbl_orderlines(q_w_id).select_split_row(olk,
                {{ol_nc::ol_amount, access_t::read},
                 {ol_nc::ol_delivery_d, Commute ? access_t::write : access_t::update}}
            );
            (void)result;
            CHK(success);
            assert(result);

            TXP_INCREMENT(txp_tpcc_dl_stage3);

            ol_amount_sum += value.ol_amount();

            if constexpr (Commute) {
                commutators::Commutator<orderline_value> commutator(delivery_date);
                db.tbl_orderlines(q_w_id).update_row(row, commutator);
            } else {
                orderline_value* new_olv = Sto::tx_alloc<orderline_value>();
                value.copy_into(new_olv);
                new_olv->ol_delivery_d = delivery_date;
                db.tbl_orderlines(q_w_id).update_row(row, new_olv);
            }
        }
        }

        {
        customer_key ck(q_w_id, q_d_id, q_c_id);
        auto [success, result, row, value] = db.tbl_customers(q_w_id).select_split_row(ck,
            {{cu_nc::c_balance, Commute ? access_t::write : access_t::update},
             {cu_nc::c_delivery_cnt, Commute ? access_t::write : access_t::update}}
        );
        (void)result;
        CHK(success);
        assert(result);

        TXP_INCREMENT(txp_tpcc_dl_stage5);

        if constexpr (Commute) {
            commutators::Commutator<customer_value> commutator((int64_t)ol_amount_sum);
            db.tbl_customers(q_w_id).update_row(row, commutator);
        } else {
            auto new_cv = Sto::tx_alloc<customer_value>();
            value.copy_into(new_cv);
            new_cv->c_balance += (int64_t)ol_amount_sum;
            new_cv->c_delivery_cnt += 1;
            db.tbl_customers(q_w_id).update_row(row, new_cv);
        }
        }
    }

    } TEND(true);

    last_delivered = delivered_order_ids;

    TXP_INCREMENT(txp_tpcc_dl_commits);
    TXP_ACCOUNT(txp_tpcc_dl_aborts, starts - 1);
}

template <typename DBParams>
void tpcc_runner<DBParams>::run_txn_stocklevel(){
    typedef orderline_value::NamedColumn ol_nc;
    typedef stock_value::NamedColumn st_nc;

    uint64_t q_w_id = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);
    auto threshold = (int32_t)ig.random(10, 20);

    std::set<uint64_t> ol_iids;

    int out_count = 0;
    (void)out_count;

    auto ol_scan_callback = [ &ol_iids] (const orderline_key&, const auto& scan_value) -> bool {
        auto olv = (typename std::remove_reference_t<decltype(db)>::ol_table_type::accessor_t)(scan_value);
        ol_iids.insert(olv.ol_i_id());
        return true;
    };

    size_t starts = 0;

    TXN {
    ++starts;

    ol_iids.clear();
    auto d_next_oid = db.oid_generator().get(q_w_id, q_d_id);

    uint64_t oid_lower = (d_next_oid > 20) ? d_next_oid - 20 : 0;
    orderline_key olk0(q_w_id, q_d_id, oid_lower, 0);
    orderline_key olk1(q_w_id, q_d_id, d_next_oid, 0);

    bool scan_success = db.tbl_orderlines(q_w_id)
            .template range_scan<decltype(ol_scan_callback), false/*reverse*/>(olk1, olk0, ol_scan_callback,
                    {{ol_nc::ol_i_id, access_t::read}}
            );
    CHK(scan_success);

    for (auto iid : ol_iids) {
        stock_key sk(q_w_id, iid);
        auto [success, result, row, value] = db.tbl_stocks(q_w_id).select_split_row(sk,
            {{st_nc::s_quantity, access_t::read}}
        );
        (void)row; (void)result;
        CHK(success);
        assert(result);
        if(value.s_quantity() < threshold) {
            out_count += 1;
        }
    }

    } TEND(true);

    TXP_INCREMENT(txp_tpcc_st_commits);
    TXP_ACCOUNT(txp_tpcc_st_aborts, starts - 1);
}

}; // namespace tpcc
