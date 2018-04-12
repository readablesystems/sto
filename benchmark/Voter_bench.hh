#pragma once

#include <iomanip>
#include <PlatformFeatures.hh>
#include "sampling.hh"
#include "Voter_structs.hh"
#include "DB_index.hh"
#include "DB_params.hh"

namespace voter {

struct constants {
    static constexpr int num_contestants = 6;
    static constexpr int64_t max_votes_per_phone_number = 1000;
};

template<typename DBParams>
class voter_db {
public:
    template<typename K, typename V>
    using OIndex = ordered_index<K, V, DBParams>;

    typedef OIndex<contestant_key, contestant_row> contestant_tbl_type;
    typedef OIndex<area_code_state_key, area_code_state_row> areacodestate_tbl_type;
    typedef OIndex<votes_key, votes_row> votes_tbl_type;
    typedef OIndex<v_votes_phone_key, v_votes_phone_row> v_votesphone_idx_type;
    typedef OIndex<v_votes_id_state_key, v_votes_id_state_row> v_votesidst_idx_type;

    explicit voter_db()
        : tbl_contestant_(),
          tbl_areacodestate_(),
          tbl_votes_(),
          idx_votesphone_(),
          idx_votesidst_() {}

    contestant_tbl_type& tbl_contestant() {
        return tbl_contestant_;
    }
    areacodestate_tbl_type& tbl_areacode_state() {
        return tbl_areacodestate_;
    }
    votes_tbl_type& tbl_votes() {
        return tbl_votes_;
    }
    v_votesphone_idx_type& view_votes_by_phone() {
        return idx_votesphone_;
    }
    v_votesidst_idx_type& view_votes_by_id_state() {
        return idx_votesidst_;
    }

    void thread_init_all() {
        tbl_contestant_.thread_init();
        tbl_areacodestate_.thread_init();
        tbl_votes_.thread_init();
        idx_votesphone_.thread_init();
        idx_votesidst_.thread_init();
    }

private:
    contestant_tbl_type    tbl_contestant_;
    areacodestate_tbl_type tbl_areacodestate_;
    votes_tbl_type         tbl_votes_;
    v_votesphone_idx_type  idx_votesphone_;
    v_votesidst_idx_type   idx_votesidst_;
};

extern std::vector<std::string> area_codes;
extern std::vector<std::string> area_code_state_map;
extern std::vector<std::string> contestant_names;

extern void initialize_data();

typedef typename sampling::StoRandomDistribution<>::rng_type rng_type;

struct basic_dists {

    rng_type thread_rng;
    std::uniform_int_distribution<size_t> area_code_id_dist;
    std::uniform_int_distribution<int32_t> contestant_num_dist;
    std::uniform_int_distribution<uint32_t> phone_num_dist;
    std::uniform_int_distribution<int> coin_flip;
    std::uniform_int_distribution<int> percent_sampler;

    explicit basic_dists(int seed)
        : thread_rng(seed),
          area_code_id_dist(0ul, area_codes.size() - 1ul),
          contestant_num_dist(1, constants::num_contestants),
          phone_num_dist(0u, 9999999u),
          coin_flip(0, 1), percent_sampler(0, 99) {}
};

class input_generator {
public:
    explicit input_generator(int rng_seed) : dists(rng_seed) {
        constexpr int num_conts = constants::num_contestants;
        for(size_t i = 0; i < area_codes.size(); ++i) {
            int32_t contestant_choice = 1;
            if (dists.percent_sampler(dists.thread_rng) < 30) {
                contestant_choice = (std::abs((int)(std::sin((double)i) * num_conts)) % num_conts) + 1;
            }
            contestant_heat_map.push_back(contestant_choice);
        }
    }

    // returns contestant number + phone number
    std::pair<int32_t, phone_number_str> generate_phone_call() {
        phone_number_str tel;
        std::stringstream ss;

        auto idx = dists.area_code_id_dist(dists.thread_rng);
        int32_t contestant_n = contestant_heat_map[idx];
        if (dists.coin_flip(dists.thread_rng) == 0) {
            contestant_n = dists.contestant_num_dist(dists.thread_rng);
        }
        if (dists.percent_sampler(dists.thread_rng) == 0) {
            contestant_n = 999;
        }

        tel.area_code = area_codes[idx];
        ss << std::setw(7) << std::setfill('0') << dists.phone_num_dist(dists.thread_rng);
        tel.number = ss.str();

        return {contestant_n, tel};
    }

private:
    basic_dists dists;
    std::vector<int32_t> contestant_heat_map;
};

template <typename DBParams>
class voter_runner {
public:
    typedef voter_db<DBParams> db_type;

    explicit voter_runner(int rid, db_type& database, double time_limit)
        : id(rid), db(database), ig(rid+1040), tsc_elapse_limit(),
          stat_committed_txns() {
        tsc_elapse_limit = static_cast<uint64_t>(time_limit
                                                 * db_params::constants::processor_tsc_frequency
                                                 * db_params::constants::billion);
    }

    void run();

    size_t committed_txns() const {
        return stat_committed_txns;
    }

private:
    void run_txn_vote(const phone_number_str& tel, int32_t contestant_number);
    bool vote_inner(const phone_number_str& tel, int32_t contestant_number);

    int id;
    db_type& db;
    input_generator ig;
    uint64_t tsc_elapse_limit;

    size_t stat_committed_txns;
};

template <typename DBParams>
class voter_loader {
public:
    typedef voter_db<DBParams> db_type;
    explicit voter_loader(db_type& database) : db(database) {}

    void load() {
        std::cout << "Loading..." << std::endl;
        always_assert(!area_codes.empty());
        always_assert(area_codes.size() == area_code_state_map.size());

        for (int i = 0; i < constants::num_contestants; ++i) {
            contestant_key ck(i);
            contestant_row cr;
            cr.name = contestant_names[i];
            db.tbl_contestant().nontrans_put(ck, cr);
        }

        for (size_t i = 0; i < area_codes.size(); ++i) {
            area_code_state_key acs_k(area_codes[i]);
            area_code_state_row acs_r;
            acs_r.state = area_code_state_map[i];
            db.tbl_areacode_state().nontrans_put(acs_k, acs_r);
        }
        std::cout << "Loaded." << std::endl;
    }

private:
    db_type& db;
};

template <typename DBParams>
void voter_runner<DBParams>::run() {
    ::TThread::set_id(id);
    set_affinity(id);
    db.thread_init_all();
    size_t cnt = 0;

    auto begin_tsc = read_tsc();

    while (true) {
        int32_t cn;
        phone_number_str tel;
        std::tie(cn, tel) = ig.generate_phone_call();

        run_txn_vote(tel, cn);

        ++cnt;
        if (((cnt & 0xfffu) == 0) && ((read_tsc() - begin_tsc) >= tsc_elapse_limit))
            break;
    }
    stat_committed_txns = cnt;
}

}; // namespace voter
