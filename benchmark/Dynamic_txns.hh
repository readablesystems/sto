#pragma once

#include "Dynamic_bench.hh"
#include <set>

namespace dynamic {

template <typename TableType, typename TableType::NamedColumn StartColumn, size_t... Indexes>
typename TableType::sel_return_type select_templated_row(
        TableType& table, typename TableType::key_type& key, int split, std::index_sequence<Indexes...>) {
    return table.select_row(key, { {
            StartColumn + Indexes,
            TableType::value_type::RecordAccessor::split_of(split, StartColumn + Indexes) ? AccessType::write : AccessType::read
            }... },
            split);
}

template <typename TableType, typename TableType::NamedColumn StartColumn, typename TableType::NamedColumn EndColumn>
typename TableType::sel_return_type select_templated_row(
        TableType& table, typename TableType::key_type& key, int split) {
    return select_templated_row<TableType, StartColumn>(
            table, key, split,
            std::make_index_sequence<static_cast<size_t>(EndColumn - StartColumn)>());
}

template <typename TableType, typename TableType::NamedColumn StartColumn, size_t... Indexes>
typename TableType::sel_split_return_type select_templated_split_row(
        TableType& table, typename TableType::key_type& key, int split, std::index_sequence<Indexes...>) {
    return table.select_split_row(key, { {
            StartColumn + Indexes,
            TableType::value_type::RecordAccessor::split_of(split, StartColumn + Indexes) ? access_t::write : access_t::read
            }... });
}

template <typename TableType, typename TableType::NamedColumn StartColumn, typename TableType::NamedColumn EndColumn>
typename TableType::sel_split_return_type select_templated_split_row(
        TableType& table, typename TableType::key_type& key, int split) {
    return select_templated_split_row<TableType, StartColumn>(
            table, key, split,
            std::make_index_sequence<static_cast<size_t>(EndColumn - StartColumn)>());
}

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_ordered_per_record(
        uint64_t count/*, bool reader, bool writer*/) {
if constexpr (!DBParams::MVCC) {
    typedef ordered_value::NamedColumn ov_nc;
    using RecordAccessor = std::conditional_t<
        DBParams::Split == db_split_type::Adaptive, ordered_value::RecordAccessor,
        bench::UniRecordAccessor<ordered_value>>;

    /*
    (void) reader;
    (void) writer;
    */

    size_t starts = 0;
    auto keys = ig.nrandom(db.keymin(), db.keymax(), count);
    const std::array<ov_nc, 3> write_starts = {ov_nc::COLCOUNT, ov_nc::wo, ov_nc::rw};

    // begin txn
    RWTXN {
    ++starts;

    int imperfect = 0;

    for (const auto key : keys) {
        ordered_key ok(key);
        int preferred_split = key % 3;
        bool abort, result;
        uintptr_t row;
        RecordAccessor value;
        if constexpr (DBParams::Split == db_split_type::Adaptive) {
            std::tie(abort, result, row, value) =
                select_templated_row<typename dynamic_db<DBParams>::o_table_type, ov_nc::ro, ov_nc::COLCOUNT>(
                        db.tbl_ordered(), ok, preferred_split);
            if (TThread::id() == 0 && value.splitindex_ != preferred_split) {
                imperfect++;
            }
        } else {
            std::tie(abort, result, row, value) =
                select_templated_split_row<typename dynamic_db<DBParams>::o_table_type, ov_nc::ro, ov_nc::COLCOUNT>(
                        db.tbl_ordered(), ok, preferred_split);
        }

        (void)row;
        CHK(abort);
        assert(result);
        assert(value);

        auto write_start = write_starts[preferred_split];

        ordered_value* ov = nullptr;
        if (preferred_split > 0) {
            ov = Sto::tx_alloc<ordered_value>();
        }

        for (auto column = ov_nc::ro; column < ov_nc::COLCOUNT; column += 1) {
            if (column < write_start) {
                if (column < ov_nc::rw) {
                    (void) value.ro()[static_cast<int>(column)];
                } else if (column < ov_nc::wo) {
                    (void) value.rw()[static_cast<int>(column - ov_nc::rw)];
                } else {
                    (void) value.wo()[static_cast<int>(column - ov_nc::wo)];
                }
            } else {
                if (column < ov_nc::rw) {
                    ov->ro[static_cast<int>(column)] =
                        value.ro()[static_cast<int>(column)];
                } else if (column < ov_nc::wo) {
                    ov->rw[static_cast<int>(column - ov_nc::rw)] =
                        value.rw()[static_cast<int>(column - ov_nc::rw)];
                } else {
                    ov->wo[static_cast<int>(column - ov_nc::wo)] =
                        value.wo()[static_cast<int>(column - ov_nc::wo)];
                }
            }
        }

        if (preferred_split || ov) {
            assert(ov && preferred_split);
        }

        if (ov) {
            db.tbl_ordered().update_row(row, ov);
        }

#if 0
        auto range_boundary = static_cast<ov_nc>(
                key % static_cast<uint64_t>(ov_nc::COLCOUNT));
        std::vector<o_table_access> accesses;
        for (auto ca = static_cast<ov_nc>(0); ca < ov_nc::COLCOUNT; ca += 1) {
            auto access = ca <= range_boundary ? AccessType::read : AccessType::update;
            if ((reader && access == AccessType::read)
                    || (writer && access == AccessType::update)) {
                accesses.push_back({ca, access});
            }
        }
        auto [abort, result, row, value] = db.tbl_ordered().select_row(
                ok, accesses);
        (void)result;
        CHK(abort);
        assert(result);
        assert(value);
        /*
        if (TThread::id() == 0 && std::abs(static_cast<int64_t>(range_boundary - value->splitindex_)) > 0) {
            printf("Still adapting: %p %ld %ld\n", value, range_boundary, value->splitindex_);
        }
        */

        if (reader) {
            for (auto index = static_cast<ov_nc>(0); index <= range_boundary; ++index) {
                if (index < ov_nc::rw) {
                    (void) value.ro[static_cast<uint64_t>(index - ov_nc::ro)];
                } else if (index < ov_nc::wo) {
                    (void) value.rw[static_cast<uint64_t>(index - ov_nc::rw)];
                } else if (index < ov_nc::COLCOUNT) {
                    (void) value.wo[static_cast<uint64_t>(index - ov_nc::wo)];
                }
            }
        }
        if (writer) {
            //ordered_value* new_val = Sto::tx_alloc<ordered_value>();
            //new (new_val) ordered_value();
            //value.init(value);
            for (auto index = range_boundary + 1; index < ov_nc::COLCOUNT; ++index) {
                if (index < ov_nc::rw) {
                    value.ro(static_cast<uint64_t>(index - ov_nc::ro)) = ig.gen_value();
                } else if (index < ov_nc::wo) {
                    value.rw(static_cast<uint64_t>(index - ov_nc::rw)) = ig.gen_value();
                } else if (index < ov_nc::COLCOUNT) {
                    value.wo(static_cast<uint64_t>(index - ov_nc::wo)) = ig.gen_value();
                }
            }
            db.tbl_ordered().update_row(row, value);
        }
#endif
    }

    /*
    if (imperfect) {
        printf("Imperfect splits: %d\n", imperfect);
    }
    */


    // commit txn
    // retry until commits
    } TEND(true);

    /*
    if (reader) {
        //TXP_INCREMENT(txp_dyn_r_commits);
        //TXP_ACCOUNT(txp_dyn_r_aborts, starts - 1);
    }
    if (writer) {
        //TXP_INCREMENT(txp_dyn_w_commits);
        //TXP_ACCOUNT(txp_dyn_w_aborts, starts - 1);
    }
    */
} else {
}
}

template <typename DBParams>
void dynamic_runner<DBParams>::run_txn_unordered_per_record(
        uint64_t count, bool reader, bool writer) {
if constexpr (!DBParams::MVCC) {
    typedef unordered_value::NamedColumn uv_nc;

    (void) reader;
    (void) writer;

    size_t starts = 0;
    auto keys = ig.nrandom(db.keymin(), db.keymax(), count);

    // begin txn
    RWTXN {
    ++starts;

    for (const auto key : keys) {
        unordered_key ok(key);
        CHK(true);
#if 0
        auto range_boundary = static_cast<uv_nc>(
                key % static_cast<uint64_t>(uv_nc::COLCOUNT));
        std::vector<u_table_access> accesses;
        for (auto ca = static_cast<uv_nc>(0); ca < uv_nc::COLCOUNT; ca += 1) {
            auto access = ca <= range_boundary ? AccessType::read : AccessType::update;
            if ((reader && access == AccessType::read)
                    || (writer && access == AccessType::update)) {
                accesses.push_back({ca, access});
            }
        }
        auto [abort, result, row, value] = db.tbl_unordered().select_row(
                ok, accesses);
        (void)result;
        CHK(abort);
        assert(result);

        //unordered_value* new_val = Sto::tx_alloc<unordered_value>();
        //new (new_val) unordered_value();
        //new_val->init(value);
        for (auto index = static_cast<uv_nc>(0); index < uv_nc::COLCOUNT; ++index) {
            if (reader && index <= range_boundary) {
                if (index < uv_nc::rw) {
                    (void) value.ro(static_cast<uint64_t>(index - uv_nc::ro));
                } else if (index < uv_nc::wo) {
                    (void) value.rw(static_cast<uint64_t>(index - uv_nc::rw));
                } else if (index < uv_nc::COLCOUNT) {
                    (void) value.wo(static_cast<uint64_t>(index - uv_nc::wo));
                }
            }
            if (writer && index > range_boundary) {
                if (index < uv_nc::rw) {
                    value.ro(static_cast<uint64_t>(index - uv_nc::ro)) = ig.gen_value();
                } else if (index < uv_nc::wo) {
                    value.rw(static_cast<uint64_t>(index - uv_nc::rw)) = ig.gen_value();
                } else if (index < uv_nc::COLCOUNT) {
                    value.wo(static_cast<uint64_t>(index - uv_nc::wo)) = ig.gen_value();
                }
            }
        }
        db.tbl_unordered().update_row(row, value);
#endif
    }


    // commit txn
    // retry until commits
    } TEND(true);
} else {
}
}

}; // namespace dynamic
