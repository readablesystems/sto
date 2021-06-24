#pragma once

#include "Dynamic_bench.hh"
#include <set>

namespace dynamic {

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_read(uint64_t depth) {
if constexpr (!DBParams::MVCC) {
    typedef ordered_value::NamedColumn ov_nc;
    typedef unordered_value::NamedColumn uv_nc;

    ov_nc range_end;
    switch (depth) {
        case 0: range_end = ov_nc::rw; break;
        case 1: range_end = (ov_nc::rw + ov_nc::wo) / 2; break;
        case 2: range_end = ov_nc::wo; break;
        default:
            std::cerr << "Invalid depth parameter: " << depth << std::endl;
            assert(false);
    }

    size_t starts = 0;

    adapters_treset();

    // begin txn
    RWTXN {
    if (starts) {
        adapters_commit();
        adapters_treset();
    }
    ++starts;

    const auto start_key = ig.random(db.keymin(), db.keymax());
    for (auto index = 0; index < 500; ++index) {
        auto key = (start_key + index) % (db.keymax() - db.keymin() + 1) + db.keymin();
    //for (const auto key : ig.nrandom(db.keymin(), db.keymax(), 1000)) {
        ordered_key ok(key);
        std::vector<o_table_access> accesses;
        std::vector<uint64_t> ro_indices;
        std::vector<uint64_t> rw_indices;
        if (sample) {
            ro_indices = ig.nrandom(ov_nc::ro, ov_nc::rw - 1, 4);
            rw_indices = ig.nrandom(
                    ov_nc::rw - ov_nc::rw, range_end - ov_nc::rw - 1, 16);
            for (auto index : ro_indices) {
                accesses.push_back({ov_nc::ro + index, AccessType::read});
            }
            for (auto index : rw_indices) {
                accesses.push_back({ov_nc::rw + index, AccessType::read});
            }
        } else {
            for (auto ca = ov_nc::ro; ca < range_end; ca += 1) {
                accesses.push_back({ca, AccessType::read});
            }
        }
        auto [abort, result, row, value] = db.tbl_ordered().select_row(
                ok, accesses);
        (void)row; (void)result;
        CHK(abort);
        assert(result);

        if (sample) {
            for (auto index : ro_indices) {
                (void) value->ro[index];
            }
            for (auto index : rw_indices) {
                (void) value->rw[index];
            }
        } else {
            for (int index = 0; ov_nc::ro + index < ov_nc::rw; ++index) {
                (void) value->ro[index];
            }
            for (int index = 0; ov_nc::rw + index < range_end; ++index) {
                (void) value->rw[index];
            }
        }
    }


    // commit txn
    // retry until commits
    } TEND(true);
    adapters_treset();

    switch (depth) {
        case 0:
            TXP_INCREMENT(txp_dyn_r_commits);
            TXP_ACCOUNT(txp_dyn_r_aborts, starts - 1);
            break;
        case 1:
            TXP_INCREMENT(txp_dyn_rm_commits);
            TXP_ACCOUNT(txp_dyn_rm_aborts, starts - 1);
            break;
        case 2:
            TXP_INCREMENT(txp_dyn_rh_commits);
            TXP_ACCOUNT(txp_dyn_rh_aborts, starts - 1);
            break;
    }
} else {
}
}

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_write(uint64_t depth) {
if constexpr (!DBParams::MVCC) {
    typedef ordered_value::NamedColumn ov_nc;
    typedef unordered_value::NamedColumn uv_nc;

    ov_nc range_start;
    switch (depth) {
        case 0: range_start = ov_nc::rw; break;
        case 1: range_start = (ov_nc::rw + ov_nc::wo) / 2; break;
        case 2: range_start = ov_nc::wo; break;
        default:
            std::cerr << "Invalid depth parameter: " << depth << std::endl;
            assert(false);
    }

    size_t starts = 0;

    adapters_treset();

    // begin txn
    RWTXN {
    if (starts) {
        adapters_commit();
        adapters_treset();
    }
    ++starts;

    for (const auto key : ig.nrandom(db.keymin(), db.keymax(), 50)) {
        ordered_key ok(key);
        std::vector<o_table_access> accesses;
        std::vector<uint64_t> rw_indices;
        std::vector<uint64_t> wo_indices;
        if (sample) {
            rw_indices = ig.nrandom(
                    range_start - ov_nc::rw, ov_nc::wo - ov_nc::rw - 1, 16);
            wo_indices = ig.nrandom(
                    ov_nc::wo - ov_nc::wo, ov_nc::COLCOUNT - ov_nc::wo - 1, 4);
            for (auto index : rw_indices) {
                accesses.push_back({ov_nc::rw + index, AccessType::update});
            }
            for (auto index : wo_indices) {
                accesses.push_back({ov_nc::wo + index, AccessType::update});
            }
        } else {
            for (auto ca = range_start; ca < ov_nc::COLCOUNT; ca += 1) {
                accesses.push_back({ca, AccessType::update});
            }
        }
        auto [abort, result, row, value] = db.tbl_ordered().select_row(
                ok, accesses);
        (void)result;
        CHK(abort);
        assert(result);

        ordered_value* new_val = Sto::tx_alloc<ordered_value>();
        new (new_val) ordered_value();
        new_val->init(value);
        if (sample) {
            for (auto index : rw_indices) {
                new_val->rw(index) = ig.gen_value();
            }
            for (auto index : wo_indices) {
                new_val->wo(index) = ig.gen_value();
            }
        } else {
            for (int index = static_cast<int>(range_start - ov_nc::rw); ov_nc::rw + index < ov_nc::wo; ++index) {
                new_val->rw(index) = ig.gen_value();
            }
            for (int index = 0; ov_nc::wo + index < ov_nc::COLCOUNT; ++index) {
                new_val->wo(index) = ig.gen_value();
            }
        }
        db.tbl_ordered().update_row(row, new_val);
    }


    // commit txn
    // retry until commits
    } TEND(true);
    adapters_treset();

    switch (depth) {
        case 0:
            TXP_INCREMENT(txp_dyn_w_commits);
            TXP_ACCOUNT(txp_dyn_w_aborts, starts - 1);
            break;
        case 1:
            TXP_INCREMENT(txp_dyn_wm_commits);
            TXP_ACCOUNT(txp_dyn_wm_aborts, starts - 1);
            break;
        case 2:
            TXP_INCREMENT(txp_dyn_wh_commits);
            TXP_ACCOUNT(txp_dyn_wh_aborts, starts - 1);
            break;
    }
} else {
}
}

}; // namespace dynamic
