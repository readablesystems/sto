#pragma once

#include "Dynamic_bench.hh"
#include <set>

namespace dynamic {

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_ordered_per_record(
        uint64_t count, bool reader, bool writer) {
if constexpr (!DBParams::MVCC) {
    typedef ordered_value::NamedColumn ov_nc;

    size_t starts = 0;

    adapters_treset();

    // begin txn
    RWTXN {
    if (starts) {
        adapters_commit();
        adapters_treset();
    }
    ++starts;

    for (const auto key : ig.nrandom(db.keymin(), db.keymax(), count)) {
        ordered_key ok(key);
        auto range_boundary = static_cast<ov_nc>(
                key % static_cast<uint64_t>(ov_nc::COLCOUNT));
        std::vector<o_table_access> accesses;
        std::vector<uint64_t> read_indices;
        std::vector<uint64_t> write_indices;
        if (params.sample) {
            if (reader) {
                read_indices = ig.nrandom(
                        0, range_boundary,
                        static_cast<uint64_t>(range_boundary + 1) / 2);
            }
            if (writer) {
                write_indices = ig.nrandom(
                        range_boundary + 1, ov_nc::COLCOUNT - 1,
                        static_cast<uint64_t>(ov_nc::COLCOUNT - range_boundary) / 2);
            }
            for (auto index : read_indices) {
                accesses.push_back({static_cast<ov_nc>(index), AccessType::read});
            }
            for (auto index : write_indices) {
                accesses.push_back({static_cast<ov_nc>(index), AccessType::update});
            }
        } else {
            for (auto ca = static_cast<ov_nc>(0); ca < ov_nc::COLCOUNT; ca += 1) {
                auto access = ca <= range_boundary ? AccessType::read : AccessType::update;
                if ((reader && access == AccessType::read)
                        || (writer && access == AccessType::update)) {
                    accesses.push_back({ca, access});
                }
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
        if (params.sample) {
            if (reader) {
                for (auto int_index : write_indices) {
                    auto index = static_cast<ov_nc>(int_index);
                    if (index < ov_nc::rw) {
                        (void) new_val->ro[static_cast<uint64_t>(index - ov_nc::ro)];
                    } else if (index < ov_nc::wo) {
                        (void) new_val->rw[static_cast<uint64_t>(index - ov_nc::rw)];
                    } else if (index < ov_nc::COLCOUNT) {
                        (void) new_val->wo[static_cast<uint64_t>(index - ov_nc::wo)];
                    }
                }
            }
            if (writer) {
                for (auto int_index : write_indices) {
                    auto index = static_cast<ov_nc>(int_index);
                    if (index < ov_nc::rw) {
                        new_val->ro(static_cast<uint64_t>(index - ov_nc::ro)) = ig.gen_value();
                    } else if (index < ov_nc::wo) {
                        new_val->rw(static_cast<uint64_t>(index - ov_nc::rw)) = ig.gen_value();
                    } else if (index < ov_nc::COLCOUNT) {
                        new_val->wo(static_cast<uint64_t>(index - ov_nc::wo)) = ig.gen_value();
                    }
                }
            }
        } else {
            for (auto index = static_cast<ov_nc>(0); index < ov_nc::COLCOUNT; ++index) {
                if (reader && index <= range_boundary) {
                    if (index < ov_nc::rw) {
                        (void) new_val->ro[static_cast<uint64_t>(index - ov_nc::ro)];
                    } else if (index < ov_nc::wo) {
                        (void) new_val->rw[static_cast<uint64_t>(index - ov_nc::rw)];
                    } else if (index < ov_nc::COLCOUNT) {
                        (void) new_val->wo[static_cast<uint64_t>(index - ov_nc::wo)];
                    }
                }
                if (writer && index > range_boundary) {
                    if (index < ov_nc::rw) {
                        new_val->ro(static_cast<uint64_t>(index - ov_nc::ro)) = ig.gen_value();
                    } else if (index < ov_nc::wo) {
                        new_val->rw(static_cast<uint64_t>(index - ov_nc::rw)) = ig.gen_value();
                    } else if (index < ov_nc::COLCOUNT) {
                        new_val->wo(static_cast<uint64_t>(index - ov_nc::wo)) = ig.gen_value();
                    }
                }
            }
        }
        db.tbl_ordered().update_row(row, new_val);
    }


    // commit txn
    // retry until commits
    } TEND(true);
    adapters_treset();
} else {
}
}

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_ordered_read(uint64_t depth) {
if constexpr (!DBParams::MVCC) {
    typedef ordered_value::NamedColumn ov_nc;

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
    const auto end_key = (start_key + 500) % (db.keymax() - db.keymin() + 1) + db.keymin();
    std::vector<o_table_access> accesses;
    std::vector<uint64_t> ro_indices;
    std::vector<uint64_t> rw_indices;
    if (params.sample) {
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

    auto scan_callback = [&] (const ordered_key&, const auto& value) -> bool {
        if (params.sample) {
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
        return true;
    };

    bool scan_success;

    if (start_key < end_key) {
        scan_success = db.tbl_ordered()
                .template range_scan<decltype(scan_callback), false/*reverse*/>(
                        start_key, end_key, scan_callback, accesses);
        CHK(scan_success);
    } else {
        scan_success = db.tbl_ordered()
                .template range_scan<decltype(scan_callback), false/*reverse*/>(
                        start_key, db.keymax(), scan_callback, accesses);
        CHK(scan_success);
        scan_success = db.tbl_ordered()
                .template range_scan<decltype(scan_callback), false/*reverse*/>(
                        db.keymin(), end_key, scan_callback, accesses);
        CHK(scan_success);
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
void dynamic_runner<DBParams>::run_txn_ordered_write(uint64_t depth) {
if constexpr (!DBParams::MVCC) {
    typedef ordered_value::NamedColumn ov_nc;

    ov_nc range_start;
    switch (depth) {
        case 0: range_start = ov_nc::wo; break;
        case 1: range_start = (ov_nc::rw + ov_nc::wo) / 2; break;
        case 2: range_start = ov_nc::rw; break;
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
        if (params.sample) {
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
        if (params.sample) {
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

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_unordered_per_record(
        uint64_t count, bool reader, bool writer) {
if constexpr (!DBParams::MVCC) {
    typedef unordered_value::NamedColumn uv_nc;

    size_t starts = 0;

    adapters_treset();

    // begin txn
    RWTXN {
    if (starts) {
        adapters_commit();
        adapters_treset();
    }
    ++starts;

    for (const auto key : ig.nrandom(db.keymin(), db.keymax(), count)) {
        unordered_key ok(key);
        auto range_boundary = static_cast<uv_nc>(
                key % static_cast<uint64_t>(uv_nc::COLCOUNT));
        std::vector<u_table_access> accesses;
        std::vector<uint64_t> read_indices;
        std::vector<uint64_t> write_indices;
        if (params.sample) {
            if (reader) {
                read_indices = ig.nrandom(
                        0, range_boundary,
                        static_cast<uint64_t>(range_boundary + 1) / 2);
            }
            if (writer) {
                write_indices = ig.nrandom(
                        range_boundary + 1, uv_nc::COLCOUNT - 1,
                        static_cast<uint64_t>(uv_nc::COLCOUNT - range_boundary) / 2);
            }
            for (auto index : read_indices) {
                accesses.push_back({static_cast<uv_nc>(index), AccessType::read});
            }
            for (auto index : write_indices) {
                accesses.push_back({static_cast<uv_nc>(index), AccessType::update});
            }
        } else {
            for (auto ca = static_cast<uv_nc>(0); ca < uv_nc::COLCOUNT; ca += 1) {
                auto access = ca <= range_boundary ? AccessType::read : AccessType::update;
                if ((reader && access == AccessType::read)
                        || (writer && access == AccessType::update)) {
                    accesses.push_back({ca, access});
                }
            }
        }
        auto [abort, result, row, value] = db.tbl_unordered().select_row(
                ok, accesses);
        (void)result;
        CHK(abort);
        assert(result);

        unordered_value* new_val = Sto::tx_alloc<unordered_value>();
        new (new_val) unordered_value();
        new_val->init(value);
        if (params.sample) {
            if (reader) {
                for (auto int_index : write_indices) {
                    auto index = static_cast<uv_nc>(int_index);
                    if (index < uv_nc::rw) {
                        (void) new_val->ro[static_cast<uint64_t>(index - uv_nc::ro)];
                    } else if (index < uv_nc::wo) {
                        (void) new_val->rw[static_cast<uint64_t>(index - uv_nc::rw)];
                    } else if (index < uv_nc::COLCOUNT) {
                        (void) new_val->wo[static_cast<uint64_t>(index - uv_nc::wo)];
                    }
                }
            }
            if (writer) {
                for (auto int_index : write_indices) {
                    auto index = static_cast<uv_nc>(int_index);
                    if (index < uv_nc::rw) {
                        new_val->ro(static_cast<uint64_t>(index - uv_nc::ro)) = ig.gen_value();
                    } else if (index < uv_nc::wo) {
                        new_val->rw(static_cast<uint64_t>(index - uv_nc::rw)) = ig.gen_value();
                    } else if (index < uv_nc::COLCOUNT) {
                        new_val->wo(static_cast<uint64_t>(index - uv_nc::wo)) = ig.gen_value();
                    }
                }
            }
        } else {
            for (auto index = static_cast<uv_nc>(0); index < uv_nc::COLCOUNT; ++index) {
                if (reader && index <= range_boundary) {
                    if (index < uv_nc::rw) {
                        (void) new_val->ro[static_cast<uint64_t>(index - uv_nc::ro)];
                    } else if (index < uv_nc::wo) {
                        (void) new_val->rw[static_cast<uint64_t>(index - uv_nc::rw)];
                    } else if (index < uv_nc::COLCOUNT) {
                        (void) new_val->wo[static_cast<uint64_t>(index - uv_nc::wo)];
                    }
                }
                if (writer && index > range_boundary) {
                    if (index < uv_nc::rw) {
                        new_val->ro(static_cast<uint64_t>(index - uv_nc::ro)) = ig.gen_value();
                    } else if (index < uv_nc::wo) {
                        new_val->rw(static_cast<uint64_t>(index - uv_nc::rw)) = ig.gen_value();
                    } else if (index < uv_nc::COLCOUNT) {
                        new_val->wo(static_cast<uint64_t>(index - uv_nc::wo)) = ig.gen_value();
                    }
                }
            }
        }
        db.tbl_unordered().update_row(row, new_val);
    }


    // commit txn
    // retry until commits
    } TEND(true);
    adapters_treset();
} else {
}
}

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_unordered_read(uint64_t depth) {
if constexpr (!DBParams::MVCC) {
    typedef unordered_value::NamedColumn uv_nc;

    uv_nc range_end;
    switch (depth) {
        case 0: range_end = uv_nc::rw; break;
        case 1: range_end = (uv_nc::rw + uv_nc::wo) / 2; break;
        case 2: range_end = uv_nc::wo; break;
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
        unordered_key uk(key);
        std::vector<u_table_access> accesses;
        std::vector<uint64_t> ro_indices;
        std::vector<uint64_t> rw_indices;
        if (params.sample) {
            ro_indices = ig.nrandom(uv_nc::ro, uv_nc::rw - 1, 4);
            rw_indices = ig.nrandom(
                    uv_nc::rw - uv_nc::rw, range_end - uv_nc::rw - 1, 16);
            for (auto index : ro_indices) {
                accesses.push_back({uv_nc::ro + index, AccessType::read});
            }
            for (auto index : rw_indices) {
                accesses.push_back({uv_nc::rw + index, AccessType::read});
            }
        } else {
            for (auto ca = uv_nc::ro; ca < range_end; ca += 1) {
                accesses.push_back({ca, AccessType::read});
            }
        }
        auto [abort, result, row, value] = db.tbl_unordered().select_row(
                uk, accesses);
        (void)row; (void)result;
        CHK(abort);
        assert(result);

        if (params.sample) {
            for (auto index : ro_indices) {
                (void) value->ro[index];
            }
            for (auto index : rw_indices) {
                (void) value->rw[index];
            }
        } else {
            for (int index = 0; uv_nc::ro + index < uv_nc::rw; ++index) {
                (void) value->ro[index];
            }
            for (int index = 0; uv_nc::rw + index < range_end; ++index) {
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
void dynamic_runner<DBParams>::run_txn_unordered_write(uint64_t depth) {
if constexpr (!DBParams::MVCC) {
    typedef unordered_value::NamedColumn uv_nc;

    uv_nc range_start;
    switch (depth) {
        case 0: range_start = uv_nc::wo; break;
        case 1: range_start = (uv_nc::rw + uv_nc::wo) / 2; break;
        case 2: range_start = uv_nc::rw; break;
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
        unordered_key uk(key);
        std::vector<u_table_access> accesses;
        std::vector<uint64_t> rw_indices;
        std::vector<uint64_t> wo_indices;
        if (params.sample) {
            rw_indices = ig.nrandom(
                    range_start - uv_nc::rw, uv_nc::wo - uv_nc::rw - 1, 16);
            wo_indices = ig.nrandom(
                    uv_nc::wo - uv_nc::wo, uv_nc::COLCOUNT - uv_nc::wo - 1, 4);
            for (auto index : rw_indices) {
                accesses.push_back({uv_nc::rw + index, AccessType::update});
            }
            for (auto index : wo_indices) {
                accesses.push_back({uv_nc::wo + index, AccessType::update});
            }
        } else {
            for (auto ca = range_start; ca < uv_nc::COLCOUNT; ca += 1) {
                accesses.push_back({ca, AccessType::update});
            }
        }
        auto [abort, result, row, value] = db.tbl_unordered().select_row(
                uk, accesses);
        (void)result;
        CHK(abort);
        assert(result);

        unordered_value* new_val = Sto::tx_alloc<unordered_value>();
        new (new_val) unordered_value();
        new_val->init(value);
        if (params.sample) {
            for (auto index : rw_indices) {
                new_val->rw(index) = ig.gen_value();
            }
            for (auto index : wo_indices) {
                new_val->wo(index) = ig.gen_value();
            }
        } else {
            for (int index = static_cast<int>(range_start - uv_nc::rw); uv_nc::rw + index < uv_nc::wo; ++index) {
                new_val->rw(index) = ig.gen_value();
            }
            for (int index = 0; uv_nc::wo + index < uv_nc::COLCOUNT; ++index) {
                new_val->wo(index) = ig.gen_value();
            }
        }
        db.tbl_unordered().update_row(row, new_val);
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
