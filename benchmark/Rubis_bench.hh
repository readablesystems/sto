#pragma once

#include <iostream>
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <sampling.hh>
#include <PlatformFeatures.hh>

#include "Rubis_structs.hh"
#include "Rubis_commutators.hh"

#if TABLE_FINE_GRAINED
#include "Rubis_selectors.hh"
#endif

#include "DB_index.hh"
#include "DB_params.hh"

#if TABLE_FINE_GRAINED
#include "rubis_split_params_ts.hh"
#else
#include "rubis_split_params_default.hh"
#endif

namespace rubis {

struct constants {
    static constexpr size_t num_items = 500000;
    static constexpr size_t num_users = 100000;
    static constexpr double user_sigma = 0.2;
    static constexpr double item_sigma = 0.8;
    static constexpr size_t num_bids_per_item = 10;
    static constexpr size_t buynow_prepop = 500000;
};

template <typename DBParams>
class rubis_db {
public:
    template <typename K, typename V>
    using OIndex = typename std::conditional<
            DBParams::MVCC,
            mvcc_ordered_index<K, V, DBParams>,
            ordered_index<K, V, DBParams>>::type;

    typedef OIndex<item_key, item_row>     item_tbl_type;
    typedef OIndex<bid_key, bid_row>       bid_tbl_type;
    typedef OIndex<buynow_key, buynow_row> buynow_tbl_type;

    explicit rubis_db()
        : tbl_items_(),
          tbl_bids_(),
          tbl_buynow_() {}

    item_tbl_type& tbl_items() {
        return tbl_items_;
    }
    bid_tbl_type& tbl_bids() {
        return tbl_bids_;
    }
    buynow_tbl_type& tbl_buynow() {
        return tbl_buynow_;
    }

    void thread_init_all() {
        tbl_items_.thread_init();
        tbl_bids_.thread_init();
        tbl_buynow_.thread_init();
    }

private:
    item_tbl_type   tbl_items_;
    bid_tbl_type    tbl_bids_;
    buynow_tbl_type tbl_buynow_;
};

enum class TxnType : int { PlaceBid = 0, BuyNow, ViewItem };

using txn_dist_type = sampling::StoCustomDistribution<TxnType>;
typedef txn_dist_type::weightgram_type workload_mix_type;
typedef sampling::StoRandomDistribution<>::rng_type rng_type;
using zipf_dist_type = sampling::StoZipfDistribution<>;

extern workload_mix_type workload_weightgram;

struct run_params {
    uint64_t time_limit;
    uint64_t num_items;
    uint64_t num_users;
    double item_sigma;
    double user_sigma;
};

struct common_dists {
    rng_type thread_rng;
    zipf_dist_type user_id_dist;

    explicit common_dists(int seed, uint64_t num_users, double user_sigma)
        : thread_rng(seed), user_id_dist(thread_rng, 1, num_users, user_sigma) {}
};

struct load_dists {
    std::uniform_int_distribution<uint32_t> std_datetime_dist;

    load_dists()
        : std_datetime_dist(1519074334, 1550610463) {}
};

struct run_dists {
    std::uniform_int_distribution<uint32_t> std_bid_diff_dist;
    zipf_dist_type item_id_dist;
    txn_dist_type txn_dist;

    run_dists(rng_type& rng, uint64_t num_items, double item_sigma)
        : std_bid_diff_dist(1, 10),
          item_id_dist(rng, 1, num_items, item_sigma),
          txn_dist(rng, workload_weightgram) {}
};

class input_generator {
public:
    explicit input_generator(int seed, uint64_t num_users, double user_sigma)
        : dists(seed, num_users, user_sigma) {}

    uint32_t generate_date();
    uint64_t generate_user_id();
protected:
    common_dists dists;
};

uint32_t input_generator::generate_date() {
    auto duration = std::chrono::system_clock::now().time_since_epoch();
    auto n = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    return static_cast<uint32_t>(n);
}
uint64_t input_generator::generate_user_id() {
    return dists.user_id_dist.sample();
}

class loadtime_input_generator : public input_generator {
public:
    explicit loadtime_input_generator(int seed, uint64_t num_users, double user_sigma)
        : input_generator(seed, num_users, user_sigma), l_dists() {}
    uint32_t generate_random_date();

private:
    load_dists l_dists;
};

uint32_t loadtime_input_generator::generate_random_date() {
    return l_dists.std_datetime_dist(this->dists.thread_rng);
}

class runtime_input_generator : public input_generator {
public:
    runtime_input_generator(int seed, uint64_t num_items, uint64_t num_users, double item_sigma, double user_sigma)
        : input_generator(seed, num_users, user_sigma),
          r_dists(this->dists.thread_rng, num_items, item_sigma) {}

    uint32_t generate_bid_diff();
    uint64_t generate_item_id();
    TxnType next_transaction();

private:
    run_dists r_dists;
};

uint32_t runtime_input_generator::generate_bid_diff() {
    return r_dists.std_bid_diff_dist(this->dists.thread_rng);
}
uint64_t runtime_input_generator::generate_item_id() {
    return r_dists.item_id_dist.sample();
}
TxnType runtime_input_generator::next_transaction() {
    return r_dists.txn_dist.sample();
}

template <typename DBParams>
class rubis_runner {
public:
    typedef rubis_db<DBParams> db_type;
    static constexpr bool Commute = DBParams::Commute;

    explicit rubis_runner(int id, db_type& database, const run_params& p)
        : id(id), db(database), time_limit(p.time_limit), total_commits_(),
          ig(id+1040, p.num_items, p.num_users, p.item_sigma, p.user_sigma) {};

    void run();
    size_t total_commits() const {
        return total_commits_;
    }
    size_t run_txn_placebid(uint64_t item_id, uint64_t user_id, uint32_t max_bid, uint32_t qty, uint32_t bid);
    size_t run_txn_buynow(uint64_t item_id, uint64_t user_id, uint32_t qty);
    size_t run_txn_viewitem(uint64_t item_id);

private:
    int id;
    db_type& db;
    uint64_t time_limit;
    size_t total_commits_;
    runtime_input_generator ig;
};

template <typename DBParams>
class rubis_loader {
public:
    typedef rubis_db<DBParams> db_type;

    explicit rubis_loader(db_type& database)
        : db(database), ig(0, constants::num_users, constants::user_sigma) {}

    void load();

private:
    db_type& db;
    loadtime_input_generator ig;
};

template <typename DBParams>
void rubis_loader<DBParams>::load() {
    for (uint64_t iid = 1; iid <= constants::num_items; ++iid) {
        item_key ik(iid);
        item_row ir;
        ir.seller = ig.generate_user_id();
        ir.reserve_price = 50;
        ir.initial_price = 50;
        ir.start_date = ig.generate_random_date();
        ir.quantity = 10;
        ir.nb_of_bids = 10;
        ir.max_bid = 40;
        ir.end_date = ig.generate_random_date();

        db.tbl_items().nontrans_put(ik, ir);

        for (uint64_t i = 0; i < constants::num_bids_per_item; ++i) {
            auto bid_id = db.tbl_bids().gen_key();
            bid_key bk(iid, ig.generate_user_id(), bid_id);
            bid_row br{};
            br.max_bid = 40;
            br.bid = 40;
            br.quantity = 1;
            br.date = ig.generate_random_date();

            db.tbl_bids().nontrans_put(bk, br);
        }
    }

    for (uint64_t n = 0; n < constants::buynow_prepop; ++n) {
        auto id = db.tbl_buynow().gen_key();
        buynow_key bnk(ig.generate_user_id(), ig.generate_user_id(), id);
        buynow_row bnr{};
        bnr.quantity = 1;
        bnr.date = ig.generate_random_date();

        db.tbl_buynow().nontrans_put(bnk, bnr);
    }
}

}
