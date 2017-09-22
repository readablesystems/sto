#include "TPCC_txns.hh"

namespace tpcc {

void tpcc_prepopulator::fill_items(uint64_t iid_begin, uint64_t iid_xend) {
    for (auto iid = iid_begin; iid < iid_xend; ++iid) {
        item_key ik(iid);
        item_value iv;

        iv.i_im_id = ig.random(1, 10000);
        iv.i_price = ig.random(100, 10000);
        iv.i_name = random_a_string(14, 24);
        iv.i_data = random_a_string(26, 50);
        if (ig.random(1, 100) <= 10) {
            auto pos = ig.random(0, iv.i_data.length() - 8);
            auto placed = iv.i_data.place("ORIGINAL", pos);
            assert(placed);
        }

        db.tbl_items().nontrans_put(ik, iv);
    }
}

void tpcc_prepopulator::fill_warehouses() {
    for (uint64_t wid = 1; wid <= ig.num_warehouses(); ++wid) {
        warehouse_key wk(wid);
        warehouse_value wv;

        wv.w_name = random_a_string(6, 10);
        wv.w_street_1 = random_a_string(10, 20);
        wv.w_street_2 = random_a_string(10, 20);
        wv.w_city = random_a_string(10, 20);
        wv.w_state = random_state_name();
        wv.w_zip = random_zip_code();
        wv.w_tax = ig.random(0, 2000);
        wv.w_ytd = 30000000;

        db.tbl_warehouses().nontrans_put(wk, wv);
    }
}

void tpcc_prepopulator::expand_warehouse(uint64_t wid) {
    for (uint64_t iid = 1; iid <= NUM_ITEMS; ++iid) {
        stock_key sk(wid, iid);
        stock_value sv;

        sv.s_quantity = ig.random(10, 100);
        for (auto i = 0; i < NUM_DISTRICTS_PER_WAREHOUSE; ++i)
            sv.s_dists[i] = random_a_string(24, 24);
        sv.s_ytd = 0;
        sv.s_order_cnt = 0;
        sv.s_remote_cnt = 0;
        sv.s_data = random_a_string(26, 50);
        if (ig.random(1, 100) <= 10) {
            auto pos = ig.random(0, sv.s_data.length() - 8);
            auto placed = sv.s_data.place("ORIGINAL", pos);
            assert(placed);
        }

        db.tbl_stocks().nontrans_put(sk, sv);
    }

    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        district_key dk(wid, did);
        district_value dv;

        dv.d_name = random_a_string(6, 10);
        dv.d_street_1 = random_a_string(10, 20);
        dv.d_street_2 = random_a_string(10, 20);
        dv.d_city = random_a_string(10, 20);
        dv.d_state = random_state_name();
        dv.d_zip = random_zip_code();
        dv.d_tax = ig.random(0, 2000);
        dv.d_ytd = 3000000;
        dv.d_next_o_id = 3001;

        db.tbl_districts().nontrans_put(dk, dv);
    }
}

void tpcc_prepopulator::expand_districts(uint64_t wid) {
    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        for (uint64_t cid = 1; cid <= NUM_CUSTOMERS_PER_DISTRICT; ++cid) {
            customer_key ck(wid, did, cid);
            customer_value cv;

            int last_name_num = (cid <= 1000) ? ig.random(0, 999) : ig.nurand(255,0,999,7911/*XXX*/);
            cv.c_last = to_last_name(last_name_num);
            cv.c_middle = "OE";
            cv.c_first = random_a_string(8, 16);
            cv.c_street_1 = random_a_string(10, 20);
            cv.c_street_2 = random_a_string(10, 20);
            cv.c_city = random_a_string(10, 20);
            cv.c_zip = random_zip_code();
            cv.c_phone = random_n_string(16, 16);
            cv.c_since = ig.gen_date();
            cv.c_credit = (ig.random(1, 100) <= 10) ? "BC" : "GC";
            cv.c_credit_lim = 5000000;
            cv.c_discount = ig.random(0, 5000);
            cv.c_balance = -1000;
            cv.c_ytd_payment = 1000;
            cv.c_payment_cnt = 1;
            cv.c_delivery_cnt = 0;
            cv.c_data = random_a_string(300, 500);

            db.tbl_customers().nontrans_put(ck, cv);
        }
    }
}

void tpcc_prepopulator::expand_customers(uint64_t wid) {
    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        for (uint64_t cid = 1; cid <= NUM_CUSTOMERS_PER_DISTRICT; ++cid) {
            history_key hk(wid + did + cid);
            history_value hv;

            hv.h_c_id = cid;
            hv.h_c_d_id = did;
            hv.h_c_w_id = wid;
            hv.h_date = ig.gen_date();
            hv.h_amount = 1000;
            hv.h_data = random_a_string(12, 24);

            db.tbl_histories().nontrans_put(hk, hv);
        }
    }
}

}; // namespace tpcc
