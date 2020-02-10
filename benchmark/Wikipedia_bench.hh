#pragma once

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <sampling.hh>
#include <PlatformFeatures.hh>

#include "compiler.hh"
#include "Wikipedia_structs.hh"
#include "Wikipedia_commutators.hh"

#if TABLE_FINE_GRAINED
#include "Wikipedia_selectors.hh"
#endif

#include "DB_index.hh"
#include "DB_params.hh"

#if TABLE_FINE_GRAINED
#include "wiki_split_params_ts.hh"
#else
#include "wiki_split_params_default.hh"
#endif

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
    static constexpr double page_id_sigma = 1.0001;
    static constexpr double watchlist_page_sigma = 1.0001;
    static constexpr double revision_user_sigma = 1.0001;
};

template <typename DBParams>
class wikipedia_db {
public:
    template <typename K, typename V>
    using OIndex = typename std::conditional<
            DBParams::MVCC,
            mvcc_ordered_index<K, V, DBParams>,
            ordered_index<K, V, DBParams>>::type;

    //typedef OIndex<ipblocks_key, ipblocks_row>                           ipb_tbl_type;
    //typedef OIndex<ipblocks_addr_idx_key, ipblocks_addr_idx_row>         ipb_addr_idx_type;
    //typedef OIndex<ipblocks_user_idx_key, ipblocks_user_idx_row>         ipb_user_idx_type;
    typedef OIndex<logging_key, logging_row>                             log_tbl_type;
    typedef OIndex<page_key, page_row>                                   page_tbl_type;
    typedef OIndex<useracct_key, useracct_row>                           user_tbl_type;
    typedef OIndex<page_idx_key, page_idx_row>                           page_idx_type;
    //typedef OIndex<page_restrictions_key, page_restrictions_row>         pr_tbl_type;
    //typedef OIndex<page_restrictions_idx_key, page_restrictions_idx_row> pr_idx_type;
    typedef OIndex<recentchanges_key, recentchanges_row>                 rc_tbl_type;
    typedef OIndex<revision_key, revision_row>                           rev_tbl_type;
    typedef OIndex<text_key, text_row>                                   text_tbl_type;
    typedef OIndex<useracct_idx_key, useracct_idx_row>                   user_idx_type;
    //typedef OIndex<user_groups_key, user_groups_row>                     ug_tbl_type;
    typedef OIndex<watchlist_key, watchlist_row>                         wl_tbl_type;
    typedef OIndex<watchlist_idx_key, watchlist_idx_row>                 wl_idx_type;

    explicit wikipedia_db() :
        //tbl_ipb_(),
        //idx_ipb_addr_(),
        //idx_ipb_user_(),
        tbl_log_(),
        tbl_page_(),
        idx_page_(),
        //tbl_pr_(),
        //idx_pr_(),
        tbl_rc_(),
        tbl_rev_(),
        tbl_text_(),
        tbl_user_(),
        idx_user_(),
        //tbl_ug_(),
        tbl_wl_(),
        idx_wl_() {}

    /*
    ipb_tbl_type& tbl_ipblocks() {
        return tbl_ipb_;
    }
    ipb_addr_idx_type& idx_ipblocks_addr() {
        return idx_ipb_addr_;
    }
    ipb_user_idx_type& idx_ipblocks_user() {
        return idx_ipb_user_;
    }
    */
    log_tbl_type& tbl_logging() {
        return tbl_log_;
    }

    page_tbl_type& tbl_page() {
        return tbl_page_;
    }
    user_tbl_type& tbl_useracct() {
        return tbl_user_;
    }
    page_idx_type& idx_page() {
        return idx_page_;
    }
    /*
    pr_tbl_type& tbl_page_restrictions() {
        return tbl_pr_;
    }
    pr_idx_type& idx_page_restrictions() {
        return idx_pr_;
    }
    */
    rc_tbl_type& tbl_recentchanges() {
        return tbl_rc_;
    }
    rev_tbl_type& tbl_revision() {
        return tbl_rev_;
    }
    text_tbl_type& tbl_text() {
        return tbl_text_;
    }
    user_idx_type& idx_useracct() {
        return idx_user_;
    }
    /*
    ug_tbl_type& tbl_user_groups() {
        return tbl_ug_;
    }
    */
    wl_tbl_type& tbl_watchlist() {
        return tbl_wl_;
    }
    wl_idx_type& idx_watchlist() {
        return idx_wl_;
    }

    void thread_init_all() {
        //tbl_ipb_.thread_init();
        //idx_ipb_addr_.thread_init();
        //idx_ipb_user_.thread_init();
        tbl_log_.thread_init();
        tbl_page_.thread_init();
        idx_page_.thread_init();
        //tbl_pr_.thread_init();
        //idx_pr_.thread_init();
        tbl_rc_.thread_init();
        tbl_rev_.thread_init();
        tbl_text_.thread_init();
        tbl_user_.thread_init();
        idx_user_.thread_init();
        //tbl_ug_.thread_init();
        tbl_wl_.thread_init();
        idx_wl_.thread_init();
    }

private:
    //ipb_tbl_type      tbl_ipb_;
    //ipb_addr_idx_type idx_ipb_addr_;
    //ipb_user_idx_type idx_ipb_user_;
    log_tbl_type      tbl_log_;
    page_tbl_type     tbl_page_;
    page_idx_type     idx_page_;
    //pr_tbl_type       tbl_pr_;
    //pr_idx_type       idx_pr_;
    rc_tbl_type       tbl_rc_;
    rev_tbl_type      tbl_rev_;
    text_tbl_type     tbl_text_;
    user_tbl_type     tbl_user_;
    user_idx_type     idx_user_;
    //ug_tbl_type       tbl_ug_;
    wl_tbl_type       tbl_wl_;
    wl_idx_type       idx_wl_;
};

extern const char *txn_names[6];

enum class TxnType : int { AddWatchList = 0, GetPageAnon, GetPageAuth, RemoveWatchList, ListPageNameSpace, UpdatePage };

inline std::ostream& operator<<(std::ostream& os, const TxnType& t) {
    os << txn_names[static_cast<int>(t)];
    return os;
}

typedef sampling::StoCustomDistribution<TxnType>::weightgram_type workload_mix_type;

struct run_params {
    uint64_t num_users;
    uint64_t num_pages;
    double time_limit;
    workload_mix_type workload_mix;

    run_params(size_t nu, size_t np, double t, const workload_mix_type& wl) :
        num_users(nu), num_pages(np), time_limit(t), workload_mix(wl) {}
};

struct load_params {
    uint64_t num_users;
    uint64_t num_pages;
};

// Pre-processed input distribution from wikibench trace (from OLTPBench)
// Defined in Wikipedia_data.cc
using rng_type = sampling::StoRandomDistribution<>::rng_type;

template <typename Type>
using hdist_type = sampling::StoCustomDistribution<Type>;

using ui_hdist_type = hdist_type<uint64_t>;
using si_hdist_type = hdist_type<int64_t>;
using txn_dist_type = hdist_type<TxnType>;
using str_hdist_type = hdist_type<std::string>;

using unif_dist_type = sampling::StoUniformDistribution<>;
using zipf_dist_type = sampling::StoZipfDistribution<>;

using ui_hist_type = ui_hdist_type::histogram_type;
using si_hist_type = si_hdist_type::histogram_type;
using str_hist_type = str_hdist_type::histogram_type;

extern const workload_mix_type workload_weightgram;

extern const ui_hist_type page_title_len_hist;
extern const ui_hist_type revisions_per_page_hist;
extern const ui_hist_type page_namespace_hist;
extern const str_hist_type page_restrictions_hist;
extern const ui_hist_type rev_comment_len_hist;
extern const std::vector<size_t> rev_delta_sizes;
extern const std::vector<si_hist_type> rev_deltas;
extern const ui_hist_type rev_minor_edit_hist;
extern const ui_hist_type text_len_hist;
extern const ui_hist_type user_name_len_hist;
extern const ui_hist_type user_real_name_len_hist;
extern const ui_hist_type user_rev_count_hist;

struct basic_dists {
    rng_type thread_rng;

    ui_hdist_type page_title_len_dist;
    ui_hdist_type page_namespace_dist;
    ui_hdist_type rev_minor_edit_dist;
    std::vector<si_hdist_type> rev_deltas_dists;
    zipf_dist_type page_id_dist;
    std::uniform_int_distribution<int> std_user_id_dist;
    std::uniform_int_distribution<int> std_ipv4_dist;
    std::uniform_int_distribution<int> std_char_dist;

    basic_dists(int id, uint64_t num_users, uint64_t num_pages)
            : thread_rng(id),
              page_title_len_dist(thread_rng, page_title_len_hist),
              page_namespace_dist(thread_rng, page_namespace_hist),
              rev_minor_edit_dist(thread_rng, rev_minor_edit_hist),
              rev_deltas_dists(),
              page_id_dist(thread_rng, 1, num_pages, constants::page_id_sigma),
              std_user_id_dist(1, static_cast<int>(num_users)),
              std_ipv4_dist(0, 255),
              std_char_dist(32, 126) {
        for (auto& h : rev_deltas) {
            rev_deltas_dists.emplace_back(thread_rng, h);
        }
    }
};

struct runtime_dists {
    txn_dist_type txn_dist;

    runtime_dists(rng_type& thread_rng, const workload_mix_type& workload_mix)
        : txn_dist(thread_rng, workload_mix) {}
};

struct loadtime_dists {
    str_hdist_type page_restrictions_dist;
    ui_hdist_type user_name_len_dist;
    ui_hdist_type user_real_name_len_dist;
    ui_hdist_type user_revcount_dist;
    ui_hdist_type revisions_per_page_dist;
    ui_hdist_type text_len_dist;
    zipf_dist_type user_watch_dist;
    std::uniform_real_distribution<float> std_pg_rnd_dist;

    loadtime_dists(rng_type& thread_rng, size_t num_pages)
        : page_restrictions_dist(thread_rng, page_restrictions_hist),
          user_name_len_dist(thread_rng, user_name_len_hist),
          user_real_name_len_dist(thread_rng, user_real_name_len_hist),
          user_revcount_dist(thread_rng, user_rev_count_hist),
          revisions_per_page_dist(thread_rng, revisions_per_page_hist),
          text_len_dist(thread_rng, text_len_hist),
          user_watch_dist(thread_rng,
                          0, std::min(num_pages, static_cast<size_t>(constants::max_watches_per_user)),
                          constants::num_watches_per_user_sigma),
          std_pg_rnd_dist(0.0, 100.0) {}
};

class input_generator {
public:
    typedef typename sampling::StoRandomDistribution<>::rng_type rng_type;

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
            ss << dists.std_ipv4_dist(dists.thread_rng);
            if (i != 3)
                ss << '.';
        }
        return ss.str();
    }

    int32_t generate_user_id() {
        return dists.std_user_id_dist(dists.thread_rng);
    }
    int32_t generate_page_id() {
        return (int32_t)dists.page_id_dist.sample();
    }
    int32_t generate_page_namespace(int page_id) {
        rng_type g(page_id);
        return (int32_t)dists.page_namespace_dist.sample(g);
    }
    int32_t generate_rev_minor_edit() {
        return (int32_t)dists.rev_minor_edit_dist.sample();
    }
    std::string generate_rev_comment() {
        // returning a constant string for now
        return "this is a comment";
    }
    std::string generate_rev_text(const std::string& old_text) {
        std::string str;
        int rev_delta_id = 0;
        for (auto sz : rev_delta_sizes) {
            if (old_text.length() <= sz)
                break;
            ++rev_delta_id;
        }
        if (rev_delta_id == (int)rev_delta_sizes.size())
            --rev_delta_id;
        assert((rev_delta_id >= 0) && (rev_delta_id < (int)rev_delta_sizes.size()));

        auto& d = dists.rev_deltas_dists[rev_delta_id];
        int64_t delta = d.sample();
        if ((int64_t)old_text.length() + delta <= 0) {
            delta = -1l * std::lround((double)old_text.length() / 1.5);
            if ((std::abs(delta) == (int64_t)old_text.length()) && delta < 0) {
                delta /= 2;
            }
        }

        if (delta != 0) {
            str = resize_text(old_text, delta);
        } else {
            str = old_text;
        }

        return permute_text(std::move(str));
    }
    std::string generate_page_title(int page_id) {
        rng_type g(page_id);
        auto title_len = dists.page_title_len_dist.sample(g);

        std::stringstream ss;
        ss << random_string(g, title_len);
        ss << '[' << page_id << ']';

        return ss.str();
    }

protected:
    input_generator(int runner_id, size_t num_users, size_t num_pages)
            : dists(runner_id, num_users, num_pages) {}

    std::string random_string(rng_type& rng, size_t len) {
        std::string rs;
        rs.resize(len);
        for (size_t i = 0; i < len; ++i)
            rs[i] = (char)dists.std_char_dist(rng);
        return rs;
    }
    std::string random_string(size_t len) {
        std::string rs;
        rs.resize(len);
        for (size_t i = 0; i < len; ++i)
            rs[i] = (char)dists.std_char_dist(dists.thread_rng);
        return rs;
    }

    std::string resize_text(const std::string& old_text, int64_t delta) {
        int64_t new_len = (int64_t)old_text.length() + delta;
        assert(new_len > 0);

        std::string rs;
        rs.resize((size_t)new_len);

        for (size_t i = 0; i < (size_t)new_len; ++i) {
            rs[i] = (i < old_text.length()) ? old_text[i]
                                            :(char)dists.std_char_dist(dists.thread_rng);
        }
        return rs;
    }
    std::string permute_text(std::string&& orig_text) {
        // XXX simply return the original text for now
        return orig_text;
    }

    basic_dists dists;
};

class runtime_input_generator : public input_generator {
public:
    runtime_input_generator(int runner_id, size_t num_users, size_t num_pages, const workload_mix_type& workload_mix)
        : input_generator(runner_id+1040, num_users, num_pages),
          r_dists(dists.thread_rng, workload_mix) {}

    TxnType next_transaction() {
        return r_dists.txn_dist.sample();
    }

private:
    runtime_dists r_dists;
};

class loadtime_input_generator : public input_generator {
public:
    loadtime_input_generator(int runner_id, size_t num_users, size_t num_pages)
        : input_generator(runner_id, num_users, num_pages),
          l_dists(dists.thread_rng, num_pages) {}

    float generate_page_random() {
        return l_dists.std_pg_rnd_dist(dists.thread_rng);
    }
    std::string generate_page_restrictions() {
        return l_dists.page_restrictions_dist.sample();
    }
    std::string generate_user_name() {
        return random_string(l_dists.user_name_len_dist.sample());
    }
    std::string generate_user_real_name() {
        return random_string(l_dists.user_real_name_len_dist.sample());
    }
    std::string generate_user_token() {
        return random_string(constants::token_length);
    }
    std::string generate_random_old_text() {
        auto old_text_len = l_dists.text_len_dist.sample();
        return random_string(old_text_len);
    }
    int generate_user_editcount() {
        return (int)l_dists.user_revcount_dist.sample();
    }
    int generate_num_revisions() {
        return (int)l_dists.revisions_per_page_dist.sample();
    }
    int generate_num_watches() {
        return (int)l_dists.user_watch_dist.sample();
    }

private:

    loadtime_dists l_dists;
};

template <typename DBParams>
class wikipedia_runner {
public:
    typedef wikipedia_db<DBParams> db_type;

    static constexpr bool Commute = DBParams::Commute;

    wikipedia_runner(int runner_id, db_type& database, const run_params& params)
        : id(runner_id), db(database),
          ig(runner_id, params.num_users, params.num_pages, params.workload_mix),
          tsc_elapse_limit(), stats_aborts_by_txn(workload_weightgram.size(), 0ul) {
        tsc_elapse_limit =
                (uint64_t)(params.time_limit * db_params::constants::processor_tsc_frequency * db_params::constants::billion);
    }

    size_t run_txn_addWatchList(int user_id, int name_space, const std::string& page_title);
    std::pair<size_t, article_type> run_txn_getPageAnonymous(bool for_select, const std::string& user_ip,
                                                             int name_space, const std::string& page_title);
    std::pair<size_t, article_type> run_txn_getPageAuthenticated(bool for_select, const std::string& user_ip,
                                                                 int user_id, int name_space, const std::string& page_title);
    size_t run_txn_removeWatchList(int user_id, int name_space, const std::string& page_title);
    size_t run_txn_listPageNameSpace(int name_space);
    size_t run_txn_updatePage(int text_id, int page_id, const std::string& page_title,
                            const std::string& page_text, int page_name_space, int user_id,
                            const std::string& user_ip, const std::string& user_text,
                            int rev_id, const std::string& rev_comment, int rev_minor_edit);

    // returns number of transactions committed
    void run();

    size_t total_commits() const {
        return stats_total_commits;
    }

    const std::vector<size_t>& aborts_by_txn() const {
        return stats_aborts_by_txn;
    }

private:
    bool txn_updatePage_inner(int text_id, int page_id, const std::string& page_title,
                              const std::string& page_text, int page_name_space, int user_id,
                              const std::string& user_ip, const std::string& user_text,
                              int rev_id, const std::string& rev_comment, int rev_minor_edit,
                              size_t& nstarts);
    int id;
    db_type& db;
    runtime_input_generator ig;
    uint64_t tsc_elapse_limit;
    size_t stats_total_commits;
    std::vector<size_t> stats_aborts_by_txn;
};

template <typename DBParams>
class wikipedia_loader {
public:
    static int *user_revision_cnts;
    static int *page_last_rev_ids;
    static int *page_last_rev_lens;

    explicit wikipedia_loader(wikipedia_db<DBParams>& wdb, const load_params& params)
            : num_users((int)params.num_users), num_pages((int)params.num_pages),
              db(wdb), ig(6332, params.num_users, params.num_pages) {}

    void load();

    static void initialize_scratch_space(size_t num_users, size_t num_pages) {
        assert(user_revision_cnts == nullptr);
        user_revision_cnts = new int[num_users];
        page_last_rev_ids = new int[num_pages];
        page_last_rev_lens = new int[num_pages];

        for (size_t i = 0; i < num_users; ++i)
            user_revision_cnts[i] = 0;
        for (size_t i = 0; i < num_pages; ++i) {
            page_last_rev_ids[i] = 0;
            page_last_rev_lens[i] = 0;
        }
    }

    static void free_scratch_space() {
        delete[] user_revision_cnts;
        delete[] page_last_rev_ids;
        delete[] page_last_rev_lens;

        user_revision_cnts = nullptr;
        page_last_rev_ids = nullptr;
        page_last_rev_lens = nullptr;
    }

private:
    void load_useracct();
    void load_page();
    void load_watchlist();
    void load_revision();

    int num_users;
    int num_pages;
    wikipedia_db<DBParams>& db;
    loadtime_input_generator ig;
};

template <typename DBParams>
void wikipedia_runner<DBParams>::run() {
    ::TThread::set_id(id);
    set_affinity(id);
    db.thread_init_all();

    auto tsc_begin = read_tsc();
    size_t cnt = 0;
    while (true) {
        auto t_type = ig.next_transaction();
        auto user_id = ig.generate_user_id();
        auto page_id = ig.generate_page_id();
        auto page_ns = ig.generate_page_namespace(page_id);
        auto page_title = ig.generate_page_title(page_id);
        size_t retries = 0;
        switch (t_type) {
            case TxnType::AddWatchList:
                retries = run_txn_addWatchList(user_id, page_ns, page_title);
                break;
            case TxnType::RemoveWatchList:
                retries = run_txn_removeWatchList(user_id, page_ns, page_title);
                break;
            case TxnType::GetPageAnon:
                std::tie(retries, std::ignore) = run_txn_getPageAnonymous(false, ig.generate_ip_address(), page_ns, page_title);
                break;
            case TxnType::GetPageAuth:
                std::tie(retries, std::ignore) = run_txn_getPageAuthenticated(false, ig.generate_ip_address(), user_id, page_ns, page_title);
                break;
            case TxnType::ListPageNameSpace:
                retries = run_txn_listPageNameSpace(page_ns);
                break;
            case TxnType::UpdatePage: {
                auto ipaddr = ig.generate_ip_address();
                size_t rt1;
                article_type article;
                std::tie(rt1, article) = run_txn_getPageAnonymous(false, ipaddr, page_ns, page_title);
                if (article.text_id == 0 || article.page_id == 0)
                    break;
                retries = run_txn_updatePage(article.text_id, article.page_id, page_title, ig.generate_rev_text(article.old_text),
                                             page_ns, user_id, ipaddr, article.user_text, article.rev_id,
                                             ig.generate_rev_comment(), ig.generate_rev_minor_edit());
                retries += rt1;
                break;
            }
            default:
                always_assert(false, "unknown transaction type");
                break;
        }

        stats_aborts_by_txn.at(static_cast<size_t>(t_type)) += retries;

        ++cnt;
        if ((read_tsc() - tsc_begin) >= tsc_elapse_limit)
            break;
    }
    stats_total_commits = cnt;
}

}; // namespace wikipedia
