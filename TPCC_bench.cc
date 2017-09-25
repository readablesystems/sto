#include <sstream>
#include <iomanip>
#include <algorithm>
#include <thread>

#include "TPCC_bench.hh"

namespace tpcc {

pthread_barrier_t tpcc_prepopulator::sync_barrier;

tpcc_db::tpcc_db(int num_whs) : num_whs_(num_whs) {
    tbl_whs_ = new wh_table_type();
    tbl_dts_ = new dt_table_type();
    tbl_cus_ = new cu_table_type();
    tbl_ods_ = new od_table_type();
    tbl_ols_ = new ol_table_type();
    tbl_nos_ = new no_table_type();
    tbl_its_ = new it_table_type();
    tbl_sts_ = new st_table_type();
    tbl_hts_ = new ht_table_type();
}

tpcc_db::~tpcc_db() {
    delete tbl_whs_;
    delete tbl_dts_;
    delete tbl_cus_;
    delete tbl_ods_;
    delete tbl_ols_;
    delete tbl_nos_;
    delete tbl_its_;
    delete tbl_sts_;
    delete tbl_hts_;
}

// @section: db prepopulation functions
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

    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        std::vector<uint64_t> cid_perm;
        for (uint64_t n = 1; n <= NUM_CUSTOMERS_PER_DISTRICT; ++n)
            cid_perm.push_back(n);
        random_shuffle(cid_perm);

        for (uint64_t i = 1; i <= NUM_CUSTOMERS_PER_DISTRICT; ++i) {
            uint64_t oid = i;
            order_key ok(wid, did, oid);
            order_value ov;

            ov.o_c_id = cid_perm[i];
            ov.o_carrier_id = (oid < 2101) ? ig.random(1, 10) : 0;
            ov.o_entry_d = ig.gen_date();
            ov.o_ol_cnt = ig.random(5, 15);
            ov.o_all_local = 1;

            db.tbl_orders().nontrans_put(ok, ov);

            for (uint64_t on = 1; on <= ov.o_ol_cnt; ++on) {
                orderline_key olk(wid, did, oid, on);
                orderline_value olv;

                olv.ol_i_id = ig.random(1, 100000);
                olv.ol_supply_w_id = wid;
                olv.ol_delivery_d = (oid < 2101) ? ov.o_entry_d : 0;
                olv.ol_quantity = 5;
                olv.ol_amount = (oid < 2101) ? 0 : ig.random(1, 999999);
                olv.ol_dist_info = random_a_string(24, 24);

                db.tbl_orderlines().nontrans_put(olk, olv);
            }

            if (oid >= 2101) {
                order_key nok(wid, did, oid);
                db.tbl_neworders().nontrans_put(nok, 0);
            }
        }
    }
}
// @endsection: db prepopulation functions

void tpcc_prepopulator::run() {
    int r;
    uint64_t worker_wid = worker_id + 1;
    if (worker_id == 0) {
        fill_items(1, 100001);
        fill_warehouses();
    }

    // barrier
    r = pthread_barrier_wait(&sync_barrier);
    assert(r == PTHREAD_BARRIER_SERIAL_THREAD || r == 0);

    expand_warehouse(worker_wid);

    // barrier
    r = pthread_barrier_wait(&sync_barrier);
    assert(r == PTHREAD_BARRIER_SERIAL_THREAD || r == 0);

    expand_districts(worker_wid);

    // barrier
    r = pthread_barrier_wait(&sync_barrier);
    assert(r == PTHREAD_BARRIER_SERIAL_THREAD || r == 0);

    expand_customers(worker_wid);
}

// @section: prepopulation string generators
std::string tpcc_prepopulator::random_a_string(int x, int y) {
    size_t len = ig.random(x, y);
    std::string str;
    str.reserve(len);

    for (auto i = 0u; i < len; ++i) {
        auto n = ig.random(0, 61);
        char c = (n < 26) ? ('a' + n) :
                ((n < 52) ? ('A' + (n - 26)) : ('0' + (n - 52)));
        str.push_back(c);
    }
    return str;
}

std::string tpcc_prepopulator::random_n_string(int x, int y) {
    size_t len = ig.random(x, y);
    std::string str;
    str.reserve(len);

    for (auto i = 0u; i < len; ++i) {
        auto n = ig.random(0, 9);
        str.push_back('0' + n);
    }
    return str;
}

std::string tpcc_prepopulator::to_last_name(int gen_n) {
    assert(gen_n <= 999 && gen_n >= 0);
    std::string str;

    int q = gen_n / 100;
    int r = gen_n % 100;
    assert(q < 10);
    str += std::string(last_names[q]);

    q = r / 10;
    r = r % 10;
    assert(r < 10 && q < 10);
    str += std::string(last_names[q]) + std::string(last_names[r]);

    return str;
}

std::string tpcc_prepopulator::random_state_name() {
    std::string str = "AA";
    for (auto& c : str)
        c += ig.random(0, 25);
    return str;
}

std::string tpcc_prepopulator::random_zip_code() {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << ig.random(0, 9999);
    return ss.str() + "11111";
}

void tpcc_prepopulator::random_shuffle(std::vector<uint64_t>& v) {
    std::shuffle(v.begin(), v.end(), ig.random_generator());
}
// @endsection: prepopulation string generators

}; // namespace tpcc

using namespace tpcc;

void prepopulation_worker(tpcc_db& db, int worker_id) {
    tpcc_prepopulator pop(worker_id, db);
    pop.run();
}

void prepopulate_db(tpcc_db& db, int num_workers) {
    int r;
    r = pthread_barrier_init(&tpcc_prepopulator::sync_barrier, nullptr, num_workers);
    assert(r == 0);

    std::vector<std::thread> prepop_thrs;
    for (int i = 0; i < num_workers; ++i)
        prepop_thrs.emplace_back(prepopulation_worker, std::ref(db), i);
    for (auto& t : prepop_thrs)
        t.join();

    r = pthread_barrier_destroy(&tpcc_prepopulator::sync_barrier);
    assert(r == 0);
}

void tpcc_runner_thread(tpcc_db& db, int runner_id, uint64_t w_start, uint64_t w_end, uint64_t num_txns) {
    tpcc_runner runner(runner_id, db, w_start, w_end);

    for (uint64_t i = 0; i < num_txns; ++i) {
        runner.run_txn_neworder();
    }
}

void run_benchmark(tpcc_db& db, int num_runners, uint64_t num_txns) {
    int q = db.num_warehouses() / num_runners;
    int r = db.num_warehouses() / num_runners;
    std::vector<std::thread> runner_thrs;
    uint64_t ntxns_thr = num_txns / num_runners;

    int last_xend = 1;

    for (int i = 0; i < num_runners; ++i) {
        int next_xend = last_xend + q;
        if (r > 0) {
            ++next_xend;
            --r;
        }
        runner_thrs.emplace_back(tpcc_runner_thread, std::ref(db), i, last_xend, next_xend - 1, ntxns_thr);
        last_xend = next_xend;
    }

    assert(last_xend == db.num_warehouses() + 1);

    for (auto& t : runner_thrs)
        t.join();
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    int num_warehouses = 12;
    int num_threads = 12;
    uint64_t num_txns = 1000000ul;

    tpcc_db db(num_warehouses);

    prepopulate_db(db, num_warehouses);

    run_benchmark(db, num_threads, num_txns);
}
