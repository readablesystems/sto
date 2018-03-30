#pragma once

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <sampling.hh>

#include "compiler.hh"
#include "clp.h"
#include "Wikipedia_structs.hh"
#include "DB_index.hh"
#include "DB_params.hh"

namespace wikipedia {

struct constants {
    static constexpr int anonymous_page_update_prob = 26;
    static constexpr int anonymous_user_id = 0;
    static constexpr int token_length = 32;
    static constexpr int pages = 1000;
    static constexpr int users = 2000;
    static constexpr int max_watches_per_user = 1000;
    static constexpr int batch_size = 1000;

    static constexpr double num_watches_per_user_sigma = 1.75;
    static constexpr double user_id_sigma = 1.0001;
    static constexpr double watchlist_page_sigma = 1.0001;
    static constexpr double revision_user_sigma = 1.0001;
};

template <typename DBParams>
class wikipedia_db {
public:
    template <typename K, typename V>
    using OIndex = ordered_index<K, V, DBParams>;

    typedef OIndex<ipblocks_key, ipblocks_row>                           ipb_tbl_type;
    typedef OIndex<ipblocks_addr_idx_key, ipblocks_addr_idx_row>         ipb_addr_idx_type;
    typedef OIndex<ipblocks_user_idx_key, ipblocks_user_idx_row>         ipb_user_idx_type;
    typedef OIndex<logging_key, logging_row>                             log_tbl_type;
    typedef OIndex<page_key, page_row>                                   page_tbl_type;
    typedef OIndex<page_idx_key, page_idx_row>                           page_idx_type;
    typedef OIndex<page_restrictions_key, page_restrictions_row>         pr_tbl_type;
    typedef OIndex<page_restrictions_idx_key, page_restrictions_idx_row> pr_idx_type;
    typedef OIndex<recentchanges_key, recentchanges_row>                 rc_tbl_type;
    typedef OIndex<revision_key, revision_row>                           rev_tbl_type;
    typedef OIndex<text_key, text_row>                                   text_tbl_type;
    typedef OIndex<useracct_key, useracct_row>                           user_tbl_type;
    typedef OIndex<useracct_idx_key, useracct_idx_row>                   user_idx_type;
    typedef OIndex<user_groups_key, user_groups_row>                     ug_tbl_type;
    typedef OIndex<watchlist_key, watchlist_row>                         wl_tbl_type;
    typedef OIndex<watchlist_idx_key, watchlist_idx_row>                 wl_idx_type;

    ipb_tbl_type& tbl_ipblocks() {
        return tbl_ipb_;
    }
    ipb_addr_idx_type& idx_ipblocks_addr() {
        return idx_ipb_addr_;
    }
    ipb_user_idx_type& idx_ipblocks_user() {
        return idx_ipb_user_;
    }
    log_tbl_type& tbl_logging() {
        return tbl_log_;
    }
    page_tbl_type& tbl_page() {
        return tbl_page_;
    }
    page_idx_type& idx_page() {
        return idx_page_;
    }
    pr_tbl_type& tbl_page_restrictions() {
        return tbl_pr_;
    }
    pr_idx_type& idx_page_restrictions() {
        return idx_pr_;
    }
    rc_tbl_type& tbl_recentchanges() {
        return tbl_rc_;
    }
    rev_tbl_type& tbl_revision() {
        return tbl_rev_;
    }
    text_tbl_type& tbl_text() {
        return tbl_text_;
    }
    user_tbl_type& tbl_useracct() {
        return tbl_user_;
    }
    user_idx_type& idx_useracct() {
        return idx_user_;
    }
    ug_tbl_type& tbl_user_groups() {
        return tbl_ug_;
    }
    wl_tbl_type& tbl_watchlist() {
        return tbl_wl_;
    }
    wl_idx_type& idx_watchlist() {
        return idx_wl_;
    }

private:
    ipb_tbl_type      tbl_ipb_;
    ipb_addr_idx_type idx_ipb_addr_;
    ipb_user_idx_type idx_ipb_user_;
    log_tbl_type      tbl_log_;
    page_tbl_type     tbl_page_;
    page_idx_type     idx_page_;
    pr_tbl_type       tbl_pr_;
    pr_idx_type       idx_pr_;
    rc_tbl_type       tbl_rc_;
    rev_tbl_type      tbl_rev_;
    text_tbl_type     tbl_text_;
    user_tbl_type     tbl_user_;
    user_idx_type     idx_user_;
    ug_tbl_type       tbl_ug_;
    wl_tbl_type       tbl_wl_;
    wl_idx_type       idx_wl_;
};

enum class TxnType : int { AddWatchList = 0, GetPageAnon, GetPageAuth, RemoveWatchList, UpdatePage };

typedef sampling::StoCustomDistribution<TxnType>::weightgram_type workload_mix_type;

struct run_params {
    uint64_t num_users;
    uint64_t num_pages;
    double time_limit;
    workload_mix_type workload_mix;
};

// Pre-processed input distribution from wikibench trace (from OLTPBench)
// Defined in Wikipedia_data.cc
template <typename IntType>
using hdist_type = sampling::StoCustomDistribution<IntType>;

using ui_hdist_type = hdist_type<uint64_t>;
using si_hdist_type = hdist_type<int64_t>;
using txn_dist_type = hdist_type<TxnType>;

using unif_dist_type = sampling::StoUniformDistribution;
using zipf_dist_type = sampling::StoZipfDistribution;

using ui_hist_type = ui_hdist_type::histogram_type;
using si_hist_type = si_hdist_type::histogram_type;

extern const ui_hist_type page_title_len_hist;
extern const ui_hist_type revisions_per_page_hist;
extern const ui_hist_type page_namespace_hist;
extern const ui_hist_type rev_comment_len_hist;
extern const std::vector<size_t> rev_delta_sizes;
extern const std::vector<si_hist_type> rev_deltas;
extern const ui_hist_type rev_minor_edit_hist;
extern const ui_hist_type text_len_hist;
extern const ui_hist_type user_name_len_hist;
extern const ui_hist_type user_real_name_len_hist;
extern const ui_hist_type user_rev_count_hist;

struct runtime_dists {
    ui_hdist_type page_title_len_dist;
    ui_hdist_type page_namespace_dist;
    unif_dist_type user_id_dist;
    unif_dist_type ipv4_dist;
    zipf_dist_type page_id_dist;
    txn_dist_type txn_dist;

    runtime_dists(int id, uint64_t num_users, uint64_t num_pages, const workload_mix_type& workload_mix)
        : page_title_len_dist(id, page_title_len_hist),
          page_namespace_dist(id, page_namespace_hist),
          user_id_dist(id, 1, num_users, false /*supress shuffle*/),
          ipv4_dist(id, 0, 255, false),
          page_id_dist(id, 1, num_pages, constants::user_id_sigma, false /*supress shuffle*/),
          txn_dist(id, workload_mix) {}
};

struct load_dists {

};

class runtime_input_generator {
public:
    runtime_input_generator(int runner_id, size_t num_users, size_t num_pages, const workload_mix_type& workload_mix)
        : distributions(runner_id, num_users, num_pages, workload_mix) {}

    std::string curr_timestamp_string() {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%d%m%Y%H-%M");
        return ss.str();
    }

    std::string generate_ip_address() {
        std::stringstream ss;
        for (int i = 0; i < 4; ++i) {
            ss << distributions.ipv4_dist.sample();
            if (i != 3)
                ss << '.';
        }
        return ss.str();
    }

    TxnType next_transaction() {
        return distributions.txn_dist.sample();
    }

private:
    runtime_dists distributions;
};

template <typename DBParams>
class wikipedia_runner {
public:
    typedef wikipedia_db<DBParams> db_type;

    wikipedia_runner(int runner_id, db_type& database, const run_params& params)
        : id(runner_id), db(database), ig(runner_id, params.num_users, params.num_pages, params.workload_mix),
          tsc_elapse_limit() {
        tsc_elapse_limit =
                (uint64_t)(params.time_limit * db_params::constants::processor_tsc_frequency * db_params::constants::billion);
    }

    void run_txn_addWatchList(int user_id, int name_space, const std::string& page_title);
    void run_txn_getPageAnonymous(bool for_select, const std::string& user_ip,
                                  int name_space, const std::string& page_title);
    void run_txn_getPageAuthenticated(bool for_select, const std::string& user_ip,
                                      int user_id, int name_space, const std::string& page_title);
    void run_txn_removeWatchList(int user_id, int name_space, const std::string& page_title);
    void run_txn_updatePage(int text_id, int page_id, const std::string& page_title,
                            const std::string& page_text, int page_name_space, int user_id,
                            const std::string& user_ip, const std::string& user_text,
                            int rev_id, const std::string& rev_comment, int rev_minor_edit);

    // returns number of transactions committed
    size_t run();

private:
    bool txn_updatePage_inner(int text_id, int page_id, const std::string& page_title,
                              const std::string& page_text, int page_name_space, int user_id,
                              const std::string& user_ip, const std::string& user_text,
                              int rev_id, const std::string& rev_comment, int rev_minor_edit);
    int id;
    db_type& db;
    runtime_input_generator ig;
    uint64_t tsc_elapse_limit;
};

template <typename DBParams>
class wikipedia_loader {
public:
    explicit wikipedia_loader(wikipedia_db<DBParams>& wdb) : db(wdb) {}
    void load();
private:
    void load_useracct();
    void load_page();
    void load_watchlist();
    void load_revision();

    wikipedia_db<DBParams>& db;
};

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run() {
    ::TThread::set_id(id);
    auto tsc_begin = read_tsc();
    size_t cnt = 0;
    while (true) {
        auto t_type = ig.next_transaction();
        switch (t_type) {
            case TxnType::AddWatchList:
            case TxnType::RemoveWatchList:
            case TxnType::GetPageAnon:
            case TxnType::GetPageAuth:
            case TxnType::UpdatePage:
            default:
                always_assert(false, "unknown transaction type");
                break;
        }
        ++cnt;
        if ((read_tsc() - tsc_begin) >= tsc_elapse_limit)
            break;
    }

    return cnt;
}

}; // namespace wikipedia