#pragma once

#include <random>
#include <string>

#include <pthread.h> // pthread_barrier_t

#include "TPCC_structs.hh"
#include "TPCC_index.hh"

#define A_GEN_CUSTOMER_ID           1023
#define A_GEN_ITEM_ID               8191

#define C_GEN_CUSTOMER_ID           259
#define C_GEN_ITEM_ID               7911

namespace tpcc {

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

class tpcc_db {
public:
    typedef unordered_index<warehouse_key, warehouse_value> wh_table_type;
    typedef unordered_index<district_key, district_value>   dt_table_type;
    typedef ordered_index<customer_key, customer_value>     cu_table_type;
    typedef unordered_index<order_key, order_value>         od_table_type;
    typedef unordered_index<orderline_key, orderline_value> ol_table_type;
    typedef unordered_index<order_key, int>                 no_table_type;
    typedef unordered_index<item_key, item_value>           it_table_type;
    typedef unordered_index<stock_key, stock_value>         st_table_type;
    typedef unordered_index<history_key, history_value>     ht_table_type;

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

class tpcc_runner {
public:
    tpcc_runner(int id, tpcc_db& database, uint64_t w_start, uint64_t w_end)
        : ig(id, database.num_warehouses()), db(database),
          w_id_start(w_start), w_id_end(w_end) {}

    inline void run_txn_neworder();
    inline void run_txn_payment();

private:
    tpcc_input_generator ig;
    tpcc_db& db;
    int runner_id;
    uint64_t w_id_start;
    uint64_t w_id_end;
};

class tpcc_prepopulator {
public:
    static pthread_barrier_t sync_barrier;

    tpcc_prepopulator(int id, tpcc_db& database)
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
    tpcc_db& db;
    int worker_id;
};

};
