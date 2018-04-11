#pragma once

#include "sampling.hh"
#include "Voter_structs.hh"
#include "DB_index.hh"
#include "DB_params.hh"

namespace voter {

struct constants {
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

template <typename DBParams>
class voter_runner {
public:
    void run_txn_vote(const phone_number_str& tel, int32_t contestant_number);

private:
    bool vote_inner(const phone_number_str& tel, int32_t contestant_number);
    voter_db<DBParams>& db;
};

}; // namespace voter
