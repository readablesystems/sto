#pragma once

#include <random>
#include "TPCC_structs.hh"

#include "MassTrans.hh"
#include "Hashtable.hh"

#define NUM_DISTRICTS_PER_WAREHOUSE 10
#define NUM_CUSTOMERS_PER_DISTRICT  3000
#define NUM_ITEMS                   100000

#define A_GEN_CUSTOMER_ID           1023
#define A_GEN_ITEM_ID               8191

#define C_GEN_CUSTOMER_ID           259
#define C_GEN_ITEM_ID               7911

namespace tpcc {

class tpcc_input_generator {
public:
    static constexpr uint64_t C_gen_customer_id = C_GEN_CUSTOMER_ID;

    tpcc_input_generator() : rd(), gen(rd()) {}

    uint64_t nurand(uint64_t a, uint64_t c, uint64_t x, uint64_t y) {
        uint64_t r1 = random(0, a) | random(x, y) + c;
        return (r1 % (y - x + 1)) + x;
    }
    uint64_t random(uint64_t x, uint64_t y) {
        std::uniform_int_distribution<uint64_t> dist(x, y);
        return dist(gen);
    }
    uint64_t num_warehouses() const {
        return scale_factor;
    }

    uint64_t gen_customer_id() {
        return nurand(A_GEN_CUSTOMER_ID, C_GEN_CUSTOMER_ID, 1, NUM_CUSTOMERS_PER_DISTRICT);
    }
    uint64_t gen_item_id() {
        return nurand(A_GEN_ITEM_ID, C_GEN_ITEM_ID, 1, NUM_ITEMS);
    }
    uint32_t gen_date() {
        return random(1505244122, 1599938522);
    }

private:
    std::random_device rd;
    std::mt19937 gen;
    uint64_t scale_factor;
};

class tpcc_runner {
public:
    inline bool run_txn_neworder();

private:
    tpcc_input_generator ig;

    uint64_t w_id_start;
    uint64_t w_id_end;
};

bool tpcc_runner::run_txn_neworder() {
    uint64_t q_w_id  = ig.random(w_id_start, w_id_end);
    uint64_t q_d_id = ig.random(1, 10);
    uint64_t q_c_id = ig.gen_customer_id();
    uint64_t num_items = ig.random(5, 15);
    uint64_t rbk = ig.random(1, 100);

    uint64_t ol_i_ids[15];
    uint64_t ol_supply_w_ids[15];
    uint64_t ol_quantities[15];

    uint32_t o_entry_d = ig.gen_date();

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
        }
        ol_supply_w_ids[i] = ol_s_w_id;

        ol_quantities[i] = ig.random(1, 10);
    }
}

};
