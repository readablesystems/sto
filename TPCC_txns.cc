#include "TPCC_bench.hh"
#include "TPCC_index.hh"

namespace tpcc {

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

    // begin txn
    TRANSACTION {

    bool abort, result;
    uintptr_t row;
    void *value;

    int64_t wh_tax_rate, dt_tax_rate;
    uint64_t dt_next_oid;

    std::tie(abort, result, std::ignore, value) = db.tbl_warehouses().select_row(warehouse_key(q_w_id));
    TXN_DO(abort);
    assert(result);
    wh_tax_rate = reinterpret_cast<warehouse_value *>(value)->w_tax;

    std::tie(abort, result, row, value) = db.tbl_districts().select_row(district_key(q_w_id, q_d_id), true);
    TXN_DO(abort);
    assert(result);
    district_value *new_dv = Sto::tx_alloc(reinterpret_cast<district_value *>(value));
    dt_tax_rate = new_dv->d_tax;
    dt_next_oid = new_dv->d_next_o_id ++;
    db.tbl_districts().update_row(row, new_dv);

    std::tie(abort, result, std::ignore, value) = db.tbl_customers().select_row(customer_key(q_w_id, q_d_id, q_c_id));
    TXN_DO(abort);
    assert(result);

    auto cus_discount = reinterpret_cast<customer_value *>(value)->c_discount;
    out_cus_last = reinterpret_cast<customer_value *>(value)->c_last;
    out_cus_credit = reinterpret_cast<customer_value *>(value)->c_credit;

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
        uint64_t oid = reinterpret_cast<item_value *>(value)->i_im_id;
        TXN_DO(oid != 0);
        uint32_t i_price = reinterpret_cast<item_value *>(value)->i_price;
        out_item_names[i] = reinterpret_cast<item_value *>(value)->i_name;
        auto i_data = reinterpret_cast<item_value *>(value)->i_data;

        std::tie(abort, result, row, value) = db.tbl_stocks().select_row(stock_key(wid, iid), true);
        TXN_DO(abort);
        assert(result);
        stock_value *new_sv = Sto::tx_alloc(reinterpret_cast<stock_value *>(value));
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

    // commit txn
    // retry until commits
    } RETRY(true);
}

}; // namespace tpcc
