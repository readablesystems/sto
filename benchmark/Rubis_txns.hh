#pragma once

#include "Rubis_bench.hh"

namespace rubis {

template <typename DBParams>
size_t rubis_runner<DBParams>::run_txn_placebid(uint64_t item_id, uint64_t user_id, uint32_t max_bid, uint32_t qty,
                                                uint32_t bid) {
#if TABLE_FINE_GRAINED
    typedef item_row::NamedColumn nc;
#endif
    size_t execs = 0;

    RWTRANSACTION {

    bool abort, result;
    uintptr_t row;
    const void* value;

    ++execs;

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) = db.tbl_items_comm().select_row(item_key(item_id),
            Commute ? RowAccess::None : RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);

    if (Commute) {
        commutators::Commutator<item_comm_row> comm(max_bid);
        db.tbl_items_comm().update_row(row, comm);
    } else {
        auto iv = reinterpret_cast<const item_comm_row*>(value);
        auto new_iv = Sto::tx_alloc(iv);
        if (max_bid > iv->max_bid) {
            new_iv->max_bid = max_bid;
        }
        new_iv->nb_of_bids += 1;
        db.tbl_items_comm().update_row(row, new_iv);
    }
#else
    std::tie(abort, result, row, value) = db.tbl_items().select_row(item_key(item_id),
#if TABLE_FINE_GRAINED
        {{nc::max_bid,    Commute ? access_t::write : access_t::update},
         {nc::nb_of_bids, Commute ? access_t::write : access_t::update}}
#else
        Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);

    if (Commute) {
        commutators::Commutator<item_row> comm(max_bid);
        db.tbl_items().update_row(row, comm);
    } else {
        auto iv = reinterpret_cast<const item_row*>(value);
        auto new_iv = Sto::tx_alloc(iv);
        if (max_bid > iv->max_bid) {
            new_iv->max_bid = max_bid;
        }
        new_iv->nb_of_bids += 1;
        db.tbl_items().update_row(row, new_iv);
    }
#endif

    bid_key bk(item_id, user_id, db.tbl_bids().gen_key());
    auto br = Sto::tx_alloc<bid_row>();
    br->max_bid = max_bid;
    br->bid = bid;
    br->quantity = qty;
    br->date = ig.generate_date();

    std::tie(abort, result) = db.tbl_bids().insert_row(bk, br);
    TXN_DO(abort);
    assert(!result);

    } RETRY(true);

    return execs - 1;
}

template <typename DBParams>
size_t rubis_runner<DBParams>::run_txn_buynow(uint64_t item_id, uint64_t user_id, uint32_t qty) {
#if TABLE_FINE_GRAINED
    typedef item_row::NamedColumn nc;
#endif
    size_t execs = 0;

    RWTRANSACTION {

    bool abort, result;
    uintptr_t row;
    const void* value;

    ++execs;

    auto curr_date = ig.generate_date();

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) = db.tbl_items_comm().select_row(item_key(item_id),
            Commute ? RowAccess::None : RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);

    if (Commute) {
        commutators::Commutator<item_comm_row> comm(qty, curr_date);
        db.tbl_items_comm().update_row(row, comm);
    } else {
        auto iv = reinterpret_cast<const item_comm_row*>(value);
        auto new_iv = Sto::tx_alloc(iv);
        new_iv->quantity -= qty;
        if (new_iv->quantity == 0) {
            new_iv->end_date = curr_date;
        }
        db.tbl_items_comm().update_row(row, new_iv);
    }
#else
    std::tie(abort, result, row, value) = db.tbl_items().select_row(item_key(item_id),
#if TABLE_FINE_GRAINED
        {{nc::quantity, Commute ? access_t::write : access_t::update},
         {nc::end_date, Commute ? access_t::write : access_t::update}}
#else
        Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);

    if (Commute) {
        commutators::Commutator<item_row> comm(qty, curr_date);
        db.tbl_items().update_row(row, comm);
    } else {
        auto iv = reinterpret_cast<const item_row*>(value);
        auto new_iv = Sto::tx_alloc(iv);
        new_iv->quantity -= qty;
        if (new_iv->quantity == 0) {
            new_iv->end_date = curr_date;
        }
        db.tbl_items().update_row(row, new_iv);
    }
#endif

    buynow_key bnk(item_id, user_id, db.tbl_buynow().gen_key());
    auto bnr = Sto::tx_alloc<buynow_row>();
    bnr->quantity = qty;
    bnr->date = curr_date;

    std::tie(abort, result) = db.tbl_buynow().insert_row(bnk, bnr);
    TXN_DO(abort);
    assert(!result);

    } RETRY(true);

    return execs - 1;
}

template <typename DBParams>
size_t rubis_runner<DBParams>::run_txn_viewitem(uint64_t item_id) {
    size_t execs = 0;

    TRANSACTION {

    bool abort, result;
    uintptr_t row;
    const void* value;

    ++execs;

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) = db.tbl_items_comm().select_row(item_key(item_id), RowAccess::ObserveValue);
#else
    std::tie(abort, result, row, value) = db.tbl_items().select_row(item_key(item_id), RowAccess::ObserveValue);
#endif
    TXN_DO(abort);
    always_assert(result, "Item should always exist.");

    } RETRY(true);

    return execs - 1;
}

template<typename DBParams>
void rubis_runner<DBParams>::run() {
    ::TThread::set_id(id);
    set_affinity(id);
    db.thread_init_all();

    auto tsc_begin = read_tsc();
    size_t cnt = 0;
    while (true) {
        auto t_type = ig.next_transaction();
        auto user_id = ig.generate_user_id();
        auto item_id = ig.generate_item_id();
        size_t retries = 0;
        switch (t_type) {
            case TxnType::PlaceBid: {
                uint32_t max_bid = 40;
                uint32_t bid = max_bid + ig.generate_bid_diff() - 5;
                retries += run_txn_placebid(item_id, user_id, max_bid, 1, bid);
                break;
            }
            case TxnType::BuyNow:
                retries += run_txn_buynow(item_id, user_id, 1);
                break;
            case TxnType::ViewItem:
                retries += run_txn_viewitem(item_id);
                break;
            default:
                always_assert(false, "unknown transaction type");
                break;
        }

        ++cnt;
        if ((read_tsc() - tsc_begin) >= time_limit)
            break;
    }

    total_commits_ = cnt;
}

}
