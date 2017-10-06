#pragma once

#include <random>
#include <string>

#include <pthread.h> // pthread_barrier_t

#include "compiler.hh"
#include "SystemProfiler.hh"
#include "TPCC_structs.hh"
#include "TPCC_index.hh"

#define A_GEN_CUSTOMER_ID           1023
#define A_GEN_ITEM_ID               8191

#define C_GEN_CUSTOMER_ID           259
#define C_GEN_ITEM_ID               7911

namespace tpcc {

class db_config_flags {
public:
    typedef uint32_t type;
    static constexpr type adpt = 0x1 << 0;
    static constexpr type opaq = 0x1 << 1;
    static constexpr type rmyw = 0x1 << 2;
    static constexpr type swis = 0x1 << 3;
    static constexpr type tctc = 0x1 << 4;
};

enum class db_params_id : int { Default = 1, Opaque, Adaptive, Swiss, TicToc };

class db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::Default;
    static constexpr bool RdMyWr = false;
    static constexpr bool Adaptive = false;
    static constexpr bool Opaque = false;
    static constexpr bool Swiss = false;
    static constexpr bool TicToc = false;
};

class db_opaque_params : public db_default_params {
public:
    static constexpr db_params_id Id = db_params_id::Opaque;
    //static constexpr bool RdMyWr = false;
    //static constexpr bool Adaptive = false;
    static constexpr bool Opaque = true;
    //static constexpr bool Swiss = false;
    //static constexpr bool TicToc = false;
};

class db_adaptive_params {
public:
    static constexpr db_params_id Id = db_params_id::Adaptive;
    static constexpr bool RdMyWr = false;
    static constexpr bool Adaptive = true;
    static constexpr bool Opaque = false;
    static constexpr bool Swiss = false;
    static constexpr bool TicToc = false;
};

class db_swiss_params {
public:
    static constexpr db_params_id Id = db_params_id::Swiss;
    static constexpr bool RdMyWr = false;
    static constexpr bool Adaptive = false;
    static constexpr bool Opaque = false;
    static constexpr bool Swiss = true;
    static constexpr bool TicToc = false;
};

class db_tictoc_params {
public:
    static constexpr db_params_id Id = db_params_id::TicToc;
    static constexpr bool RdMyWr = false;
    static constexpr bool Adaptive = false;
    static constexpr bool Opaque = false;
    static constexpr bool Swiss = false;
    static constexpr bool TicToc = true;
};

class constants {
public:
    static constexpr double million = 1000000.0;
    static constexpr double processor_tsc_frequency = 2.2; // in GHz
};

class tpcc_input_generator {
public:
    static const char * last_names[];

    tpcc_input_generator(int id, int num_whs) : rd(), gen(id), num_whs_(num_whs) {}
    tpcc_input_generator(int num_whs) : rd(), gen(rd()), num_whs_(num_whs) {}

    uint64_t nurand(uint64_t a, uint64_t c, uint64_t x, uint64_t y) {
        uint64_t r1 = (random(0, a) | random(x, y)) + c;
        return (r1 % (y - x + 1)) + x;
    }
    uint64_t random(uint64_t x, uint64_t y) {
        std::uniform_int_distribution<uint64_t> dist(x, y);
        return dist(gen);
    }
    uint64_t num_warehouses() const {
        return num_whs_;
    }

    uint64_t gen_warehouse_id() {
        return random(1, num_whs_);
    }
    uint64_t gen_customer_id() {
        return nurand(A_GEN_CUSTOMER_ID, C_GEN_CUSTOMER_ID, 1, NUM_CUSTOMERS_PER_DISTRICT);
    }

    int gen_customer_last_name_num() {
        return nurand(255, 0, 999, 7911/* XXX 4.3.2.3 magic C number */);
    }

    std::string to_last_name(int gen_n) {
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

    std::string gen_customer_last_name() {
        return to_last_name(gen_customer_last_name_num());
    }

    uint64_t gen_item_id() {
        return nurand(A_GEN_ITEM_ID, C_GEN_ITEM_ID, 1, NUM_ITEMS);
    }
    uint32_t gen_date() {
        return random(1505244122, 1599938522);
    }
    std::mt19937& random_generator() {
        return gen;
    }

private:
    std::random_device rd;
    std::mt19937 gen;
    uint64_t num_whs_;
};

template <typename DBParams>
class tpcc_db {
public:
    static constexpr bool PR = DBParams::RdMyWr;
    static constexpr bool PA = DBParams::Adaptive;
    static constexpr bool PO = DBParams::Opaque;

    template <typename K, typename V>
    using UIndex = unordered_index<K, V, PO, PA, PR>;

    template <typename K, typename V>
    using OIndex = ordered_index<K, V, PO, PA, PR>;

    typedef UIndex<warehouse_key, warehouse_value> wh_table_type;
    typedef UIndex<district_key, district_value>   dt_table_type;
    typedef OIndex<customer_key, customer_value>   cu_table_type;
    typedef UIndex<order_key, order_value>         od_table_type;
    typedef UIndex<orderline_key, orderline_value> ol_table_type;
    typedef UIndex<order_key, int>                 no_table_type;
    typedef UIndex<item_key, item_value>           it_table_type;
    typedef UIndex<stock_key, stock_value>         st_table_type;
    typedef UIndex<history_key, history_value>     ht_table_type;

    inline tpcc_db(int num_whs);
    inline tpcc_db(const std::string& db_file_name);
    inline ~tpcc_db();

    int num_warehouses() const {
        return num_whs_;
    }
    wh_table_type& tbl_warehouses() {
        return *tbl_whs_;
    }
    dt_table_type& tbl_districts() {
        return *tbl_dts_;
    }
    cu_table_type& tbl_customers() {
        return *tbl_cus_;
    }
    od_table_type& tbl_orders() {
        return *tbl_ods_;
    }
    ol_table_type& tbl_orderlines() {
        return *tbl_ols_;
    }
    no_table_type& tbl_neworders() {
        return *tbl_nos_;
    }
    it_table_type& tbl_items() {
        return *tbl_its_;
    }
    st_table_type& tbl_stocks() {
        return *tbl_sts_;
    }
    ht_table_type& tbl_histories() {
        return *tbl_hts_;
    }

private:
    int num_whs_;

    wh_table_type *tbl_whs_;
    dt_table_type *tbl_dts_;
    cu_table_type *tbl_cus_;
    od_table_type *tbl_ods_;
    ol_table_type *tbl_ols_;
    no_table_type *tbl_nos_;
    it_table_type *tbl_its_;
    st_table_type *tbl_sts_;
    ht_table_type *tbl_hts_;
};

template <typename DBParams>
class tpcc_runner {
public:
    enum class txn_type : int {new_order = 1, payment, order_status, delivery, stock_level};

    tpcc_runner(int id, tpcc_db<DBParams>& database, uint64_t w_start, uint64_t w_end)
        : ig(id, database.num_warehouses()), db(database),
          w_id_start(w_start), w_id_end(w_end) {}

    inline txn_type next_transaction() {
        uint64_t x = ig.random(1, 100);
        if (x <= 50)
            return txn_type::new_order;
        else
            return txn_type::payment;
    }

    inline void run_txn_neworder();
    inline void run_txn_payment();

private:
    tpcc_input_generator ig;
    tpcc_db<DBParams>& db;
    int runner_id;
    uint64_t w_id_start;
    uint64_t w_id_end;
};

template <typename DBParams>
class tpcc_prepopulator {
public:
    static pthread_barrier_t sync_barrier;

    tpcc_prepopulator(int id, tpcc_db<DBParams>& database)
        : ig(id, database.num_warehouses()), db(database), worker_id(id) {}

    inline void fill_items(uint64_t iid_begin, uint64_t iid_xend);
    inline void fill_warehouses();
    inline void expand_warehouse(uint64_t wid);
    inline void expand_districts(uint64_t wid);
    inline void expand_customers(uint64_t wid);

    inline void run();

private:
    inline std::string random_a_string(int x, int y);
    inline std::string random_n_string(int x, int y);
    inline std::string random_state_name();
    inline std::string random_zip_code();
    inline void random_shuffle(std::vector<uint64_t>& v);

    tpcc_input_generator ig;
    tpcc_db<DBParams>& db;
    int worker_id;
};

template <typename DBParams>
pthread_barrier_t tpcc_prepopulator<DBParams>::sync_barrier;

class tpcc_profiler {
public:
    tpcc_profiler(bool spawn_perf)
        : spawn_perf_(spawn_perf), perf_pid_(),
          start_tsc_(), end_tsc_() {}

    void start() {
        if (spawn_perf_)
            perf_pid_ = Profiler::spawn("tpcc_perf");
        start_tsc_ = read_tsc();
    }

    void finish() {
        end_tsc_ = read_tsc();
        if (spawn_perf_) {
            bool ok = Profiler::stop(perf_pid_);
            always_assert(ok);
        }
        // print elapsed time
        uint64_t elapsed_tsc = end_tsc_ - start_tsc_;
        std::cout << "Elapsed time: " << elapsed_tsc << " ticks" << std::endl;
        std::cout << "Real time: " << (double)elapsed_tsc / constants::million / constants::processor_tsc_frequency
            << " ms" << std::endl;

        // print STO stats
        Transaction::print_stats();
    }

private:
    bool spawn_perf_;
    pid_t perf_pid_;
    uint64_t start_tsc_;
    uint64_t end_tsc_;
};

};
