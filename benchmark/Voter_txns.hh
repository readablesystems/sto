#pragma once

#include "Voter_bench.hh"

namespace voter {

template <typename DBParams>
void voter_runner<DBParams>::run_txn_vote(const phone_number_str& tel, int32_t contestant_number) {
    TRANSACTION {
        bool success = vote_inner(tel, contestant_number);
        TXN_DO(success);
    } RETRY(true);
}

template <typename DBParams>
bool voter_runner<DBParams>::vote_inner(const phone_number_str& tel, int32_t id) {
    bool success, result;
    uintptr_t row;
    const void *value;

    // check contestant number
    std::tie(success, result, std::ignore, std::ignore) = db.tbl_contestant().select_row(contestant_key(id), RowAccess::None);
    if (!success)
        return false;
    if (!result)
        return true;

    // check and maintain view: votes by phone number
    std::tie(success, result, row, value) = db.view_votes_by_phone().select_row(v_votes_phone_key(tel), RowAccess::UpdateValue);
    if (!success)
        return false;
    if (result) {
        auto v_ph_row = reinterpret_cast<const v_votes_phone_row *>(value);
        auto new_row = Sto::tx_alloc(v_ph_row);
        if (new_row->count >= constants::max_votes_per_phone_number) {
            db.view_votes_by_phone().update_row(row, new_row);
            return true;
        } else {
            new_row->count += 1;
            db.view_votes_by_phone().update_row(row, new_row);
        }
    } else {
        auto temp_row = Sto::tx_alloc<v_votes_phone_row>();
        temp_row->count = 0;
        std::tie(success, result) = db.view_votes_by_phone().insert_row(v_votes_phone_key(tel), temp_row);
        if (!success)
            return false;
        assert(!result);
    }

    // look up state
    std::tie(success, result, std::ignore, value) = db.tbl_areacode_state().select_row(area_code_state_key(tel.area_code), RowAccess::None);
    if (!success)
        return false;
    assert(result);
    auto st_row = reinterpret_cast<const area_code_state_row *>(value);

    // insert vote

    auto vote_id = (int32_t)db.tbl_votes().gen_key();
    votes_key vk(vote_id);
    auto vr = Sto::tx_alloc<votes_row>();
    vr->state = st_row->state;
    vr->tel.area_code = tel.area_code;
    vr->tel.number = tel.number;
    vr->contestant_number = id;
    vr->created = "null";

    std::tie(success, result) = db.tbl_votes().insert_row(vk, vr);
    if (!success)
        return false;
    assert(!result);

    // maintain view: votes by id and state
    std::tie(success, result, row, value) = db.view_votes_by_id_state().select_row(v_votes_id_state_key(id, vr->state), RowAccess::UpdateValue);
    if (!success)
        return false;
    if (result) {
        auto new_row = Sto::tx_alloc(reinterpret_cast<const v_votes_id_state_row *>(value));
        new_row->count += 1;
        db.view_votes_by_id_state().update_row(row, new_row);
    } else {
        auto new_row = Sto::tx_alloc<v_votes_id_state_row>();
        std::tie(success, result) = db.view_votes_by_id_state().insert_row(v_votes_id_state_key(id, vr->state), new_row);
        if (!success)
            return false;
        assert(!result);
    }

    return true;
}

};
