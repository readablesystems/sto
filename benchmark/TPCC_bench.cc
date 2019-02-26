#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <thread>

#include "TPCC_bench.hh"
#include "TPCC_txns.hh"
#include "PlatformFeatures.hh"
#include "DB_profiler.hh"

static inline void print_usage(const char *);

namespace tpcc {

using bench::db_profiler;

const char *tpcc_input_generator::last_names[] = {
        "BAR", "OUGHT", "ABLE", "PRI", "PRES",
        "ESE", "ANTI", "CALLY", "ATION", "EING"};

template <typename DBParams>
tpcc_db<DBParams>::tpcc_db(int num_whs) : tbl_whs_((size_t)num_whs), oid_gen_() {
    //constexpr size_t num_districts = NUM_DISTRICTS_PER_WAREHOUSE;
    //constexpr size_t num_customers = NUM_CUSTOMERS_PER_DISTRICT * NUM_DISTRICTS_PER_WAREHOUSE;

    tbl_its_ = new it_table_type(999983/*NUM_ITEMS * 2*/);
    for (auto i = 0; i < num_whs; ++i) {
#if TPCC_SPLIT_TABLE
        tbl_dts_const_.emplace_back(999983/*num_districts * 2*/);
        tbl_dts_comm_.emplace_back(999983);
        tbl_cus_const_.emplace_back(999983/*num_customers * 2*/);
        tbl_cus_comm_.emplace_back(999983);
        tbl_ods_const_.emplace_back(999983/*num_customers * 10 * 2*/);
        tbl_ods_comm_.emplace_back(999983);
        tbl_ols_const_.emplace_back(999983/*num_customers * 100 * 2*/);
        tbl_ols_comm_.emplace_back(999983);
#else
        tbl_dts_.emplace_back(999983/*num_districts * 2*/);
        tbl_cus_.emplace_back(999983/*num_customers * 2*/);
        tbl_ods_.emplace_back(999983/*num_customers * 10 * 2*/);
        tbl_ols_.emplace_back(999983/*num_customers * 100 * 2*/);
#endif
        tbl_cni_.emplace_back(999983/*num_customers * 2*/);
        tbl_oci_.emplace_back(999983/*num_customers * 2*/);
        tbl_nos_.emplace_back(999983/*num_customers * 10 * 2*/);
        tbl_sts_.emplace_back(999983/*NUM_ITEMS * 2*/);
        tbl_hts_.emplace_back(999983/*num_customers * 2*/);
    }
}

template <typename DBParams>
tpcc_db<DBParams>::~tpcc_db() {
    delete tbl_its_;
}

template <typename DBParams>
void tpcc_db<DBParams>::thread_init_all() {
    tbl_its_->thread_init();
#if TPCC_SPLIT_TABLE
    for (auto& t : tbl_dts_const_)
        t.thread_init();
    for (auto& t : tbl_dts_comm_)
        t.thread_init();
    for (auto& t : tbl_cus_const_)
        t.thread_init();
    for (auto& t : tbl_cus_comm_)
        t.thread_init();
    for (auto& t : tbl_ods_const_)
        t.thread_init();
    for (auto& t : tbl_ods_comm_)
        t.thread_init();
    for (auto& t : tbl_ols_const_)
        t.thread_init();
    for (auto& t : tbl_ols_comm_)
        t.thread_init();
#else
    for (auto& t : tbl_dts_)
        t.thread_init();
    for (auto& t : tbl_cus_)
        t.thread_init();
    for (auto& t : tbl_ods_)
        t.thread_init();
    for (auto& t : tbl_ols_)
        t.thread_init();
#endif
    for (auto& t : tbl_cni_)
        t.thread_init();
    for (auto& t : tbl_oci_)
        t.thread_init();
    for (auto& t : tbl_nos_)
        t.thread_init();
    for (auto& t : tbl_sts_)
        t.thread_init();
    for (auto& t : tbl_hts_)
        t.thread_init();
}

// @section: db prepopulation functions
template<typename DBParams>
void tpcc_prepopulator<DBParams>::fill_items(uint64_t iid_begin, uint64_t iid_xend) {
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
            (void)placed;
        }

        db.tbl_items().nontrans_put(ik, iv);
    }
}

template<typename DBParams>
void tpcc_prepopulator<DBParams>::fill_warehouses() {
    for (uint64_t wid = 1; wid <= ig.num_warehouses(); ++wid) {
        auto &wv = db.get_warehouse(wid);

        wv.cv.w_name = random_a_string(6, 10);
        wv.cv.w_street_1 = random_a_string(10, 20);
        wv.cv.w_street_2 = random_a_string(10, 20);
        wv.cv.w_city = random_a_string(10, 20);
        wv.cv.w_state = random_state_name();
        wv.cv.w_zip = random_zip_code();
        wv.cv.w_tax = ig.random(0, 2000);
        wv.ytd = 30000000;
    }
}

template<typename DBParams>
void tpcc_prepopulator<DBParams>::expand_warehouse(uint64_t wid) {
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
            (void)placed;
        }

        db.tbl_stocks(wid).nontrans_put(sk, sv);
    }

    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        district_key dk(wid, did);
#if TPCC_SPLIT_TABLE
        district_const_value dcv;
        district_comm_value dmv;

        dcv.d_name = random_a_string(6, 10);
        dcv.d_street_1 = random_a_string(10, 20);
        dcv.d_street_2 = random_a_string(10, 20);
        dcv.d_city = random_a_string(10, 20);
        dcv.d_state = random_state_name();
        dcv.d_zip = random_zip_code();
        dcv.d_tax = ig.random(0, 2000);
        dmv.d_ytd = 3000000;
        //dv.d_next_o_id = 3001;

        db.tbl_districts_const(wid).nontrans_put(dk, dcv);
        db.tbl_districts_comm(wid).nontrans_put(dk, dmv);
#else
        district_value dv;

        dv.d_name = random_a_string(6, 10);
        dv.d_street_1 = random_a_string(10, 20);
        dv.d_street_2 = random_a_string(10, 20);
        dv.d_city = random_a_string(10, 20);
        dv.d_state = random_state_name();
        dv.d_zip = random_zip_code();
        dv.d_tax = ig.random(0, 2000);
        dv.d_ytd = 3000000;
        //dv.d_next_o_id = 3001;

        db.tbl_districts(wid).nontrans_put(dk, dv);
#endif

    }
}

template<typename DBParams>
void tpcc_prepopulator<DBParams>::expand_districts(uint64_t wid) {
    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        for (uint64_t cid = 1; cid <= NUM_CUSTOMERS_PER_DISTRICT; ++cid) {
            int last_name_num = (cid <= 1000) ? int(cid - 1)
                                              : ig.gen_customer_last_name_num(false/*run time*/);
            customer_key ck(wid, did, cid);
#if TPCC_SPLIT_TABLE
            customer_const_value ccv;
            customer_comm_value cmv;

            ccv.c_last = ig.to_last_name(last_name_num);
            ccv.c_middle = "OE";
            ccv.c_first = random_a_string(8, 16);
            ccv.c_street_1 = random_a_string(10, 20);
            ccv.c_street_2 = random_a_string(10, 20);
            ccv.c_city = random_a_string(10, 20);
            ccv.c_zip = random_zip_code();
            ccv.c_phone = random_n_string(16, 16);
            ccv.c_since = ig.gen_date();
            ccv.c_credit = (ig.random(1, 100) <= 10) ? "BC" : "GC";
            ccv.c_credit_lim = 5000000;
            ccv.c_discount = ig.random(0, 5000);
            cmv.c_balance = -1000;
            cmv.c_ytd_payment = 1000;
            cmv.c_payment_cnt = 1;
            cmv.c_delivery_cnt = 0;
            cmv.c_data = random_a_string(300, 500);

            db.tbl_customers_const(wid).nontrans_put(ck, ccv);
            db.tbl_customers_comm(wid).nontrans_put(ck, cmv);

            customer_idx_key cik(wid, did, ccv.c_last, ccv.c_first);
#else
            customer_value cv;

            cv.c_last = ig.to_last_name(last_name_num);
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

            db.tbl_customers(wid).nontrans_put(ck, cv);

            customer_idx_key cik(wid, did, cv.c_last, cv.c_first);
#endif

            customer_idx_value civ(cid);
            db.tbl_customer_index(wid).nontrans_put(cik, civ);
        }
    }
}

template<typename DBParams>
void tpcc_prepopulator<DBParams>::expand_customers(uint64_t wid) {
    for (uint64_t did = 1; did <= NUM_DISTRICTS_PER_WAREHOUSE; ++did) {
        for (uint64_t cid = 1; cid <= NUM_CUSTOMERS_PER_DISTRICT; ++cid) {
            history_value hv;

            hv.h_c_id = cid;
            hv.h_c_d_id = did;
            hv.h_c_w_id = wid;
            hv.h_date = ig.gen_date();
            hv.h_amount = 1000;
            hv.h_data = random_a_string(12, 24);

            history_key hk(db.tbl_histories(wid).gen_key());
            db.tbl_histories(wid).nontrans_put(hk, hv);
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
            auto ol_count = (uint32_t) ig.random(5, 15);
            auto entry_date = ig.gen_date();
#if TPCC_SPLIT_TABLE
            order_const_value ocv;
            order_comm_value omv;


            ocv.o_c_id = cid_perm[i - 1];
            omv.o_carrier_id = (oid < 2101) ? ig.random(1, 10) : 0;
            ocv.o_entry_d = entry_date;
            ocv.o_ol_cnt = ol_count;
            ocv.o_all_local = 1;

            order_cidx_key ock(wid, did, ocv.o_c_id, oid);

            db.tbl_orders_const(wid).nontrans_put(ok, ocv);
            db.tbl_orders_comm(wid).nontrans_put(ok, omv);
#else
            order_value ov;

            ov.o_c_id = cid_perm[i - 1];
            ov.o_carrier_id = (oid < 2101) ? ig.random(1, 10) : 0;
            ov.o_entry_d = entry_date;
            ov.o_ol_cnt = ol_count;
            ov.o_all_local = 1;

            order_cidx_key ock(wid, did, ov.o_c_id, oid);

            db.tbl_orders(wid).nontrans_put(ok, ov);
#endif
            db.tbl_order_customer_index(wid).nontrans_put(ock, {});

            for (uint64_t on = 1; on <= ol_count; ++on) {
                orderline_key olk(wid, did, oid, on);
#if TPCC_SPLIT_TABLE
                orderline_const_value lcv;
                orderline_comm_value lmv;

                lcv.ol_i_id = ig.random(1, 100000);
                lcv.ol_supply_w_id = wid;
                lmv.ol_delivery_d = (oid < 2101) ? entry_date : 0;
                lcv.ol_quantity = 5;
                lcv.ol_amount = (oid < 2101) ? 0 : (int) ig.random(1, 999999);
                lcv.ol_dist_info = random_a_string(24, 24);

                db.tbl_orderlines_const(wid).nontrans_put(olk, lcv);
                db.tbl_orderlines_comm(wid).nontrans_put(olk, lmv);
#else
                orderline_value olv;

                olv.ol_i_id = ig.random(1, 100000);
                olv.ol_supply_w_id = wid;
                olv.ol_delivery_d = (oid < 2101) ? entry_date : 0;
                olv.ol_quantity = 5;
                olv.ol_amount = (oid < 2101) ? 0 : (int) ig.random(1, 999999);
                olv.ol_dist_info = random_a_string(24, 24);

                db.tbl_orderlines(wid).nontrans_put(olk, olv);
#endif
            }

            if (oid >= 2101) {
                order_key nok(wid, did, oid);
                db.tbl_neworders(wid).nontrans_put(nok, {});
            }
        }
    }
}
// @endsection: db prepopulation functions

template<typename DBParams>
void tpcc_prepopulator<DBParams>::run() {
    int r;

    always_assert(worker_id >= 1, "prepopulator worker id range error");
    // set affinity so that the warehouse is filled at the corresponding numa node
    set_affinity(worker_id - 1);

    if (worker_id == 1) {
        fill_items(1, 100001);
        fill_warehouses();
    }

    // barrier
    r = pthread_barrier_wait(&sync_barrier);
    always_assert(r == PTHREAD_BARRIER_SERIAL_THREAD || r == 0, "pthread_barrier_wait");

    expand_warehouse((uint64_t) worker_id);

    // barrier
    r = pthread_barrier_wait(&sync_barrier);
    always_assert(r == PTHREAD_BARRIER_SERIAL_THREAD || r == 0, "pthread_barrier_wait");

    expand_districts((uint64_t) worker_id);

    // barrier
    r = pthread_barrier_wait(&sync_barrier);
    always_assert(r == PTHREAD_BARRIER_SERIAL_THREAD || r == 0, "pthread_barrier_wait");

    expand_customers((uint64_t) worker_id);
}

// @section: prepopulation string generators
template<typename DBParams>
std::string tpcc_prepopulator<DBParams>::random_a_string(int x, int y) {
    size_t len = ig.random(x, y);
    std::string str;
    str.reserve(len);

    for (auto i = 0u; i < len; ++i) {
        auto n = ig.random(0, 61);
        char c = (n < 26) ? char('a' + n) :
                 ((n < 52) ? char('A' + (n - 26)) : char('0' + (n - 52)));
        str.push_back(c);
    }
    return str;
}

template<typename DBParams>
std::string tpcc_prepopulator<DBParams>::random_n_string(int x, int y) {
    size_t len = ig.random(x, y);
    std::string str;
    str.reserve(len);

    for (auto i = 0u; i < len; ++i) {
        auto n = ig.random(0, 9);
        str.push_back(char('0' + n));
    }
    return str;
}

template<typename DBParams>
std::string tpcc_prepopulator<DBParams>::random_state_name() {
    std::string str = "AA";
    for (auto &c : str)
        c += ig.random(0, 25);
    return str;
}

template<typename DBParams>
std::string tpcc_prepopulator<DBParams>::random_zip_code() {
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(4) << ig.random(0, 9999);
    return ss.str() + "11111";
}

template<typename DBParams>
void tpcc_prepopulator<DBParams>::random_shuffle(std::vector<uint64_t> &v) {
    std::shuffle(v.begin(), v.end(), ig.random_generator());
}
// @endsection: prepopulation string generators

// @section: clp parser definitions
enum {
    opt_dbid = 1, opt_nwhs, opt_nthrs, opt_time, opt_perf, opt_pfcnt, opt_gc,
    opt_node, opt_comm
};

static const Clp_Option options[] = {
    { "dbid",         'i', opt_dbid,  Clp_ValString, Clp_Optional },
    { "nwarehouses",  'w', opt_nwhs,  Clp_ValInt,    Clp_Optional },
    { "nthreads",     't', opt_nthrs, Clp_ValInt,    Clp_Optional },
    { "time",         'l', opt_time,  Clp_ValDouble, Clp_Optional },
    { "perf",         'p', opt_perf,  Clp_NoVal,     Clp_Optional },
    { "perf-counter", 'c', opt_pfcnt, Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "gc",           'g', opt_gc,    Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "node",         'n', opt_node,  Clp_NoVal,     Clp_Negate| Clp_Optional },
    { "commute",      'x', opt_comm,  Clp_NoVal,     Clp_Negate| Clp_Optional },
};

// @endsection: clp parser definitions

template <typename DBParams>
class tpcc_access {
public:
    static void prepopulation_worker(tpcc_db<DBParams> &db, int worker_id) {
        tpcc_prepopulator<DBParams> pop(worker_id, db);
        db.thread_init_all();
        pop.run();
    }

    static void prepopulate_db(tpcc_db<DBParams> &db) {
        int r;
        r = pthread_barrier_init(&tpcc_prepopulator<DBParams>::sync_barrier, nullptr, db.num_warehouses());
        always_assert(r == 0, "pthread_barrier_init failed");

        std::vector<std::thread> prepop_thrs;
        for (int i = 1; i <= db.num_warehouses(); ++i)
            prepop_thrs.emplace_back(prepopulation_worker, std::ref(db), i);
        for (auto &t : prepop_thrs)
            t.join();

        r = pthread_barrier_destroy(&tpcc_prepopulator<DBParams>::sync_barrier);
        always_assert(r == 0, "pthread_barrier_destroy failed");
    }

    static void tpcc_runner_thread(tpcc_db<DBParams>& db, db_profiler& prof, int runner_id, uint64_t w_start,
                                   uint64_t w_end, double time_limit, uint64_t& txn_cnt) {
        tpcc_runner<DBParams> runner(runner_id, db, w_start, w_end);
        typedef typename tpcc_runner<DBParams>::txn_type txn_type;

        uint64_t local_cnt = 0;

        ::TThread::set_id(runner_id);
        set_affinity(runner_id);
        db.thread_init_all();

        uint64_t tsc_diff = (uint64_t)(time_limit * constants::processor_tsc_frequency * constants::billion);
        auto start_t = prof.start_timestamp();

        while (true) {
            auto curr_t = read_tsc();
            if ((curr_t - start_t) >= tsc_diff)
                break;

            txn_type t = runner.next_transaction();
            switch (t) {
                case txn_type::new_order:
                    runner.run_txn_neworder();
                    break;
                case txn_type::payment:
                    runner.run_txn_payment();
                    break;
                case txn_type::order_status:
                    runner.run_txn_orderstatus();
                    break;
                case txn_type::delivery:
                    runner.run_txn_delivery();
                    break;
                case txn_type::stock_level:
                    runner.run_txn_stocklevel();
                    break;
                default:
                    fprintf(stderr, "r:%d unknown txn type\n", runner_id);
                    assert(false);
                    break;
            };

            ++local_cnt;
        }

        txn_cnt = local_cnt;
    }

    static uint64_t run_benchmark(tpcc_db<DBParams>& db, db_profiler& prof, int num_runners, double time_limit) {
        int q = db.num_warehouses() / num_runners;
        int r = db.num_warehouses() % num_runners;

        std::vector<std::thread> runner_thrs;
        std::vector<uint64_t> txn_cnts(size_t(num_runners), 0);

        if (q == 0) {
            q = (num_runners + db.num_warehouses() - 1) / db.num_warehouses();
            int qq = q;
            int wid = 1;
            for (int i = 0; i < num_runners; ++i) {
                if ((qq--) == 0) {
                    ++wid;
                    qq += q;
                }
                fprintf(stdout, "runner %d: [%d, %d]\n", i, wid, wid);
                runner_thrs.emplace_back(tpcc_runner_thread, std::ref(db), std::ref(prof),
                                         i, wid, wid, time_limit, std::ref(txn_cnts[i]));
            }
        } else {
            int last_xend = 1;

            for (int i = 0; i < num_runners; ++i) {
                int next_xend = last_xend + q;
                if (r > 0) {
                    ++next_xend;
                    --r;
                }
                fprintf(stdout, "runner %d: [%d, %d]\n", i, last_xend, next_xend - 1);
                runner_thrs.emplace_back(tpcc_runner_thread, std::ref(db), std::ref(prof),
                                         i, last_xend, next_xend - 1, time_limit, std::ref(txn_cnts[i]));
                last_xend = next_xend;
            }

            always_assert(last_xend == db.num_warehouses() + 1, "warehouse distribution error");
        }

        for (auto &t : runner_thrs)
            t.join();

        uint64_t total_txn_cnt = 0;
        for (auto& cnt : txn_cnts)
            total_txn_cnt += cnt;
        return total_txn_cnt;
    }

    static int execute(int argc, const char *const *argv) {
        int ret = 0;

        bool spawn_perf = false;
        bool counter_mode = false;
        int num_warehouses = 1;
        int num_threads = 1;
        double time_limit = 10.0;
        bool enable_gc = false;

        Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

        int opt;
        bool clp_stop = false;
        while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
            switch (opt) {
                case opt_dbid:
                    break;
                case opt_nwhs:
                    num_warehouses = clp->val.i;
                    break;
                case opt_nthrs:
                    num_threads = clp->val.i;
                    break;
                case opt_time:
                    time_limit = clp->val.d;
                    break;
                case opt_perf:
                    spawn_perf = !clp->negated;
                    break;
                case opt_pfcnt:
                    counter_mode = !clp->negated;
                    break;
                case opt_gc:
                    enable_gc = !clp->negated;
                    break;
                case opt_node:
                    break;
                case opt_comm:
                    break;
                default:
                    print_usage(argv[0]);
                    ret = 1;
                    clp_stop = true;
                    break;
            }
        }

        Clp_DeleteParser(clp);
        if (ret != 0)
            return ret;

        auto profiler_mode = counter_mode ?
                             Profiler::perf_mode::counters : Profiler::perf_mode::record;

        if (counter_mode && !spawn_perf) {
            // turns on profiling automatically if perf counters are requested
            spawn_perf = true;
        }

        if (spawn_perf) {
            std::cout << "Info: Spawning perf profiler in "
                      << (counter_mode ? "counter" : "record") << " mode" << std::endl;
        }

        db_profiler prof(spawn_perf);
        tpcc_db<DBParams> db(num_warehouses);

        std::cout << "Prepopulating database..." << std::endl;
        prepopulate_db(db);
        std::cout << "Prepopulation complete." << std::endl;

        std::thread advancer;
        std::cout << "Garbage collection: " << (enable_gc ? "enabled" : "disabled") << std::endl;
        if (enable_gc) {
            advancer = std::thread(&Transaction::epoch_advancer, nullptr);
            advancer.detach();
        }

        prof.start(profiler_mode);
        auto num_trans = run_benchmark(db, prof, num_threads, time_limit);
        prof.finish(num_trans);

        if (enable_gc) {
            MvRegistry::stop();
            while (!MvRegistry::done());
        }

        return 0;
    }
}; // class tpcc_access

}; // namespace tpcc

using namespace tpcc;
using namespace db_params;

static inline void print_usage(const char *argv_0) {
    std::stringstream ss;
    ss << "Usage of " << std::string(argv_0) << ":" << std::endl
       << "  --dbid=<STRING> (or -i<STRING>)" << std::endl
       << "    Specify the type of DB concurrency control used. Can be one of the followings:" << std::endl
       << "      default, opaque, 2pl, adaptive, swiss, tictoc, defaultnode, mvcc, mvccnode" << std::endl
       << "  --nwarehouses=<NUM> (or -w<NUM>)" << std::endl
       << "    Specify the number of warehouses (default 1)." << std::endl
       << "  --nthreads=<NUM> (or -t<NUM>)" << std::endl
       << "    Specify the number of threads (or TPCC workers/terminals, default 1)." << std::endl
       << "  --time=<NUM> (or -l<NUM>)" << std::endl
       << "    Specify the time (duration) for which the benchmark is run (default 10 seconds)." << std::endl
       << "  --perf (or -p)" << std::endl
       << "    Spawns perf profiler in record mode for the duration of the benchmark run." << std::endl
       << "  --perf-counter (or -c)" << std::endl
       << "    Spawns perf profiler in counter mode for the duration of the benchmark run." << std::endl
       << "  --gc (or -g)" << std::endl
       << "    Enable garbage collection (default false)." << std::endl
       << "  --node (or -n)" << std::endl
       << "    Enable node tracking (default false)." << std::endl
       << "  --commute (or -x)" << std::endl
       << "    Enable commutative updates in MVCC (default false)." << std::endl;
    std::cout << ss.str() << std::flush;
}

double constants::processor_tsc_frequency;
bench::dummy_row bench::dummy_row::row;

int main(int argc, const char *const *argv) {
    db_params_id dbid = db_params_id::Default;
    int ret_code = 0;
   
    Clp_Parser *clp = Clp_NewParser(argc, argv, arraysize(options), options);

    int opt;
    bool clp_stop = false;
    bool node_tracking = false;
    bool enable_commute = false;
    while (!clp_stop && ((opt = Clp_Next(clp)) != Clp_Done)) {
        switch (opt) {
        case opt_dbid:
            dbid = parse_dbid(clp->val.s);
            if (dbid == db_params_id::None) {
                std::cout << "Unsupported DB CC id: "
                    << ((clp->val.s == nullptr) ? "" : std::string(clp->val.s)) << std::endl;
                print_usage(argv[0]);
                ret_code = 1;
                clp_stop = true;
            }
            break;
        case opt_node:
            node_tracking = !clp->negated;
            break;
        case opt_comm:
            enable_commute = !clp->negated;
            break;
        default:
            break;
        }
    }

    Clp_DeleteParser(clp);
    if (ret_code != 0)
        return ret_code;

    auto cpu_freq = determine_cpu_freq();
    if (cpu_freq == 0.0)
        return 1;
    else
        constants::processor_tsc_frequency = cpu_freq;

    switch (dbid) {
    case db_params_id::Default:
        if (node_tracking && enable_commute) {
            ret_code = tpcc_access<db_default_commute_node_params>::execute(argc, argv);
        } else if (node_tracking) {
            ret_code = tpcc_access<db_default_node_params>::execute(argc, argv);
        } else if (enable_commute) {
            ret_code = tpcc_access<db_default_commute_params>::execute(argc, argv);
        } else {
            ret_code = tpcc_access<db_default_params>::execute(argc, argv);
        }
        break;
    /*
    case db_params_id::Opaque:
        ret_code = tpcc_access<db_opaque_params>::execute(argc, argv);
        break;
    case db_params_id::TwoPL:
        ret_code = tpcc_access<db_2pl_params>::execute(argc, argv);
        break;
    case db_params_id::Adaptive:
        ret_code = tpcc_access<db_adaptive_params>::execute(argc, argv);
        break;
    case db_params_id::Swiss:
        ret_code = tpcc_access<db_swiss_params>::execute(argc, argv);
        break;
    case db_params_id::TicToc:
        ret_code = tpcc_access<db_tictoc_params>::execute(argc, argv);
        break;
    */
    case db_params_id::MVCC:
        if (node_tracking && enable_commute) {
            ret_code = tpcc_access<db_mvcc_commute_node_params>::execute(argc, argv);
        } else if (node_tracking) {
            ret_code = tpcc_access<db_mvcc_node_params>::execute(argc, argv);
        } else if (enable_commute) {
            ret_code = tpcc_access<db_mvcc_commute_params>::execute(argc, argv);
        } else {
            ret_code = tpcc_access<db_mvcc_params>::execute(argc, argv);
        }
        break;
    default:
        std::cerr << "unsupported db config parameter id" << std::endl;
        ret_code = 1;
        break;
    };

    return ret_code;
}
