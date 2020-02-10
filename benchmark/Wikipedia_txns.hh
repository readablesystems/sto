#pragma once

#include "Wikipedia_bench.hh"

#define INTERACTIVE_TXN_START Sto::start_transaction()

#define INTERACTIVE_RWTXN_START {Sto::start_transaction(); Sto::mvcc_rw_upgrade();}

#define INTERACTIVE_TXN_COMMIT \
    always_assert(Sto::transaction()->in_progress(), "transaction not in progress"); \
    if (!Sto::transaction()->try_commit()) { \
        return false; \
    }

#define TXN_CHECK(op) \
    if (!(op)) { \
        Sto::transaction()->silent_abort(); \
        return false; \
    }

namespace wikipedia {

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run_txn_addWatchList(int user_id,
                                                      int name_space,
                                                      const std::string& page_title) {
#if TABLE_FINE_GRAINED
    typedef useracct_row::NamedColumn nc;
#endif
    size_t nexecs = 0;

    RWTRANSACTION {

    bool abort, result;
    uintptr_t row;
    const void *value;

    ++nexecs;

    auto wv = Sto::tx_alloc<watchlist_row>();
    auto wiv = Sto::tx_alloc<watchlist_idx_row>();
    bzero(wv, sizeof(watchlist_row));
    bzero(wiv, sizeof(watchlist_idx_row));
    std::tie(abort, result) = db.tbl_watchlist().insert_row(watchlist_key(user_id, name_space, page_title), wv);
    TXN_DO(abort);

    std::tie(abort, result) = db.idx_watchlist().insert_row(watchlist_idx_key(name_space, page_title, user_id), wiv);
    TXN_DO(abort);

    if (name_space == 0) {
        std::tie(abort, std::ignore) = db.tbl_watchlist().insert_row(watchlist_key(user_id, 1, page_title), wv);
        TXN_DO(abort);
    }

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) = db.tbl_useracct_comm().select_row(useracct_key(user_id),
        Commute ? RowAccess::None : RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    if (Commute) {
        commutators::Commutator<useracct_comm_row> comm(false, ig.curr_timestamp_string());
        db.tbl_useracct_comm().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_comm_row *>(value));
        new_uv->user_touched = ig.curr_timestamp_string();
        db.tbl_useracct_comm().update_row(row, new_uv);
    }
#else
    std::tie(abort, result, row, value) = db.tbl_useracct().select_row(useracct_key(user_id),
#if TABLE_FINE_GRAINED
        {{nc::user_touched, Commute ? access_t::write : access_t::update}}
#else
        Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);
    if (Commute) {
        commutators::Commutator<useracct_row> comm(false, ig.curr_timestamp_string());
        db.tbl_useracct().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_row *>(value));
        new_uv->user_touched = ig.curr_timestamp_string();
        db.tbl_useracct().update_row(row, new_uv);
    }
#endif

    } RETRY(true);

    return (nexecs - 1);
}

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run_txn_removeWatchList(int user_id,
                                                           int name_space,
                                                           const std::string& page_title) {
#if TABLE_FINE_GRAINED
    typedef useracct_row::NamedColumn nc;
#endif
    size_t nexecs = 0;

    RWTRANSACTION {

    bool abort, result;
    uintptr_t row;
    const void *value;

    ++nexecs;

    std::tie(abort, result) = db.tbl_watchlist().delete_row(watchlist_key(user_id, name_space, page_title));
    TXN_DO(abort);
    //assert(result);

    std::tie(abort, result) = db.idx_watchlist().delete_row(watchlist_idx_key(name_space, page_title, user_id));
    TXN_DO(abort);
    //assert(result);
#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) = db.tbl_useracct_comm().select_row(useracct_key(user_id),
        Commute ? RowAccess::None : RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    if (Commute) {
        commutators::Commutator<useracct_comm_row> comm(false, ig.curr_timestamp_string());
        db.tbl_useracct_comm().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_comm_row *>(value));
        new_uv->user_touched = ig.curr_timestamp_string();
        db.tbl_useracct_comm().update_row(row, new_uv);
    }
#else
    std::tie(abort, result, row, value) = db.tbl_useracct().select_row(useracct_key(user_id),
#if TABLE_FINE_GRAINED
        {{nc::user_touched, Commute ? access_t::write : access_t::update}}
#else
        Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);
    if (Commute) {
        commutators::Commutator<useracct_row> comm(false, ig.curr_timestamp_string());
        db.tbl_useracct().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_row *>(value));
        new_uv->user_touched = ig.curr_timestamp_string();
        db.tbl_useracct().update_row(row, new_uv);
    }
#endif

    } RETRY(true);

    return (nexecs - 1);
}

template <typename DBParams>
std::pair<size_t, article_type> wikipedia_runner<DBParams>::run_txn_getPageAnonymous(bool for_select,
                                                          const std::string& user_ip,
                                                          int name_space,
                                                          const std::string& page_title) {
#if TABLE_FINE_GRAINED
    typedef page_row::NamedColumn page_nc;
#endif
    //typedef page_restrictions_row::NamedColumn pr_nc;
    //typedef ipblocks_row::NamedColumn ipb_nc;
    //typedef revision_row::NamedColumn rev_nc;
    //typedef text_row::NamedColumn text_nc;

    (void)for_select;
    size_t nexecs = 0;
    article_type art;

    TRANSACTION {

    bool abort, result;
    const void *value;

    ++nexecs;

    std::tie(abort, result, std::ignore, value) = db.idx_page().select_row(page_idx_key(name_space, page_title), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    auto page_id = reinterpret_cast<const page_idx_row *>(value)->page_id;

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, std::ignore, value) = db.tbl_page_comm().select_row(page_key(page_id), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    auto page_v = reinterpret_cast<const page_comm_row *>(value);
#else
    std::tie(abort, result, std::ignore, value) = db.tbl_page().select_row(page_key(page_id),
#if TABLE_FINE_GRAINED
        {{page_nc::page_latest, access_t::read}}
#else
        RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);
    auto page_v = reinterpret_cast<const page_row *>(value);
#endif

    /*
    std::vector<int32_t> pr_ids;
    auto pr_scan_cb = [&](const page_restrictions_idx_key&, const page_restrictions_idx_row& row) {
        pr_ids.push_back(row.pr_id);
        return true;
    };

    page_restrictions_idx_key pr_k0(page_id, std::string());
    page_restrictions_idx_key pr_k1(page_id, std::string(60, (unsigned char)0xff));
    abort = db.idx_page_restrictions().template range_scan<decltype(pr_scan_cb), false>(pr_k0, pr_k1, pr_scan_cb, RowAccess::ObserveValue);
    TXN_DO(abort);

    for (auto pr_id : pr_ids) {
        std::tie(abort, result, std::ignore, std::ignore) = db.tbl_page_restrictions().select_row(page_restrictions_key(pr_id), {{pr_nc::pr_type, false}});
        TXN_DO(abort);
        assert(result);
    }

    std::vector<int32_t> ipb_ids;
    auto ipb_scan_cb = [&](const ipblocks_addr_idx_key& key, const ipblocks_addr_idx_row&) {
        ipb_ids.push_back(bswap(key.ipb_id));
        return true;
    };

    constexpr int32_t m = std::numeric_limits<int32_t>::max();
    ipblocks_addr_idx_key ipb_k0(user_ip, 0, 0, 0, 0);
    ipblocks_addr_idx_key ipb_k1(user_ip, m, m, m, m);
    abort = db.idx_ipblocks_addr().template range_scan<decltype(ipb_scan_cb), false>(ipb_k0, ipb_k1, ipb_scan_cb, RowAccess::ObserveValue);
    TXN_DO(abort);

    for (auto ipb_id : ipb_ids) {
        std::tie(abort, result, std::ignore, std::ignore) = db.tbl_ipblocks().select_row(ipblocks_key(ipb_id), {{ipb_nc::ipb_expiry, false}});
        TXN_DO(abort);
        assert(result);
    }
    */

    auto rev_id = page_v->page_latest;
    std::tie(abort, result, std::ignore, value) = db.tbl_revision().select_row(revision_key(rev_id), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    auto rev_text_id = reinterpret_cast<const revision_row *>(value)->rev_text_id;

    std::tie(abort, result, std::ignore, value) = db.tbl_text().select_row(text_key(rev_text_id), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);

    art.text_id = rev_text_id;
    art.page_id = page_id;
    art.rev_id = rev_id;
    art.old_text = std::string(reinterpret_cast<const text_row *>(value)->old_text);
    art.user_text = user_ip;

    } RETRY(true);

    return {nexecs - 1, art};
}

template <typename DBParams>
std::pair<size_t, article_type> wikipedia_runner<DBParams>::run_txn_getPageAuthenticated(bool for_select,
                                                              const std::string& user_ip,
                                                              int user_id,
                                                              int name_space,
                                                              const std::string& page_title) {
#if TABLE_FINE_GRAINED
    typedef useracct_row::NamedColumn user_nc;
    typedef page_row::NamedColumn page_nc;
#endif
    //typedef page_restrictions_row::NamedColumn pr_nc;
    //typedef ipblocks_row::NamedColumn ipb_nc;

    (void)for_select;
    size_t nexecs = 0;
    article_type art;

    TRANSACTION {

    bool abort, result;
    const void *value;

    ++nexecs;

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, std::ignore, std::ignore) = db.tbl_useracct_const().select_row(useracct_key(user_id),
        RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
#else
    std::tie(abort, result, std::ignore, std::ignore) = db.tbl_useracct().select_row(useracct_key(user_id),
#if TABLE_FINE_GRAINED
        {{user_nc::user_name, access_t::read}}
#else
        RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);
#endif

    //std::tie(abort, result, std::ignore, std::ignore) = db.tbl_user_groups().select_row(user_groups_key(user_id), RowAccess::ObserveValue);
    //TXN_DO(abort);
    //assert(result);

    // From this point is pretty much the same as getPageAnonymous 

    std::tie(abort, result, std::ignore, value) = db.idx_page().select_row(page_idx_key(name_space, page_title), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    auto page_id = reinterpret_cast<const page_idx_row *>(value)->page_id;

#if TPCC_SPLIT_TABLE
    std::tie(abort, result, std::ignore, value) = db.tbl_page_comm().select_row(page_key(page_id), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    auto page_v = reinterpret_cast<const page_comm_row *>(value);
#else
    std::tie(abort, result, std::ignore, value) = db.tbl_page().select_row(page_key(page_id),
#if TABLE_FINE_GRAINED
        {{page_nc::page_latest, access_t::read}}
#else
        RowAccess::ObserveValue
#endif
    );
    TXN_DO(abort);
    assert(result);
    auto page_v = reinterpret_cast<const page_row *>(value);
#endif

    /*
    std::vector<int32_t> pr_ids;
    auto pr_scan_cb = [&](const page_restrictions_idx_key&, const page_restrictions_idx_row& row) {
        pr_ids.push_back(row.pr_id);
        return true;
    };

    page_restrictions_idx_key pr_k0(page_id, std::string());
    page_restrictions_idx_key pr_k1(page_id, std::string(60, (unsigned char)0xff));
    abort = db.idx_page_restrictions().template range_scan<decltype(pr_scan_cb), false>(pr_k0, pr_k1, pr_scan_cb, RowAccess::ObserveValue);
    TXN_DO(abort);

    for (auto pr_id : pr_ids) {
        std::tie(abort, result, std::ignore, std::ignore) = db.tbl_page_restrictions().select_row(page_restrictions_key(pr_id), {{pr_nc::pr_type, false}});
        TXN_DO(abort);
        assert(result);
    }

    std::vector<int32_t> ipb_ids;
    auto ipb_scan_cb = [&](const ipblocks_user_idx_key& key, const ipblocks_user_idx_row&) {
        ipb_ids.push_back(bswap(key.ipb_id));
        return true;
    };

    constexpr int32_t m = std::numeric_limits<int32_t>::max();
    ipblocks_user_idx_key ipb_k0(user_id, std::string(), 0, 0, 0);
    ipblocks_user_idx_key ipb_k1(user_id, std::string(255, (unsigned char)0xff), m, m, m);
    abort = db.idx_ipblocks_user().template range_scan<decltype(ipb_scan_cb), false>(ipb_k0, ipb_k1, ipb_scan_cb, RowAccess::ObserveExists);
    TXN_DO(abort);

    for (auto ipb_id : ipb_ids) {
        std::tie(abort, result, std::ignore, std::ignore) = db.tbl_ipblocks().select_row(ipblocks_key(ipb_id), {{ipb_nc::ipb_expiry, false}});
        TXN_DO(abort);
        assert(result);
    }
    */

    auto rev_id = page_v->page_latest;
    std::tie(abort, result, std::ignore, value) = db.tbl_revision().select_row(revision_key(rev_id), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);
    auto rev_text_id = reinterpret_cast<const revision_row *>(value)->rev_text_id;

    std::tie(abort, result, std::ignore, value) = db.tbl_text().select_row(text_key(rev_text_id), RowAccess::ObserveValue);
    TXN_DO(abort);
    assert(result);

    art.text_id = rev_text_id;
    art.page_id = page_id;
    art.rev_id = rev_id;
    art.old_text = std::string(reinterpret_cast<const text_row *>(value)->old_text);
    art.user_text = user_ip;

    } RETRY(true);

    return {nexecs - 1, art};
}

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run_txn_listPageNameSpace(int name_space) {
#if TABLE_FINE_GRAINED
    typedef page_row::NamedColumn page_nc;
#endif
    size_t nexecs = 0;

    TRANSACTION {

    bool abort, result;
    const void *value;

    ++nexecs;

    std::vector<std::pair<int, std::string>> pages;

    auto scan_callback = [&](const page_idx_key& key, const page_idx_row& row) {
        pages.push_back({row.page_id, std::string(key.page_title.c_str())});
        return true;
    };

    page_idx_key pk0(name_space, std::string());
    page_idx_key pk1(name_space, std::string(255, (unsigned char)0xff));
    abort = db.idx_page().template range_scan<decltype(scan_callback), false>(pk0, pk1, scan_callback, RowAccess::ObserveValue, false, 20/*retrieve 20 items*/);
    TXN_DO(abort);

    for (auto pair : pages) {
#if TPCC_SPLIT_TABLE
        std::tie(abort, result, std::ignore, value)
                = db.tbl_page_const().select_row(page_key(pair.first), RowAccess::ObserveValue);
        TXN_DO(abort);
        assert(result);
        auto pr = reinterpret_cast<const page_const_row *>(value);
        always_assert(pair.second == std::string(pr->page_title.c_str()));
#else
        std::tie(abort, result, std::ignore, value)
                = db.tbl_page().select_row(page_key(pair.first),
#if TABLE_FINE_GRAINED
                    {{page_nc::page_title, access_t::read}}
#else
                    RowAccess::ObserveValue
#endif
                );
        TXN_DO(abort);
        assert(result);
        auto pr = reinterpret_cast<const page_row *>(value);
        always_assert(pair.second == std::string(pr->page_title.c_str()));
#endif
    }

    } RETRY(true);

    return (nexecs - 1);
}

template <typename DBParams>
bool wikipedia_runner<DBParams>::txn_updatePage_inner(int text_id,
                                                      int page_id,
                                                      const std::string& page_title,
                                                      const std::string& page_text,
                                                      int page_name_space,
                                                      int user_id,
                                                      const std::string& user_ip,
                                                      const std::string& user_text,
                                                      int rev_id,
                                                      const std::string& rev_comment,
                                                      int rev_minor_edit,
                                                      size_t& nstarts) {
#if TABLE_FINE_GRAINED
    typedef page_row::NamedColumn page_nc;
    typedef useracct_row::NamedColumn user_nc;
#endif

    INTERACTIVE_RWTXN_START;

    bool abort, result;
    uintptr_t row;
    const void *value;

    ++nstarts;

    auto timestamp_str = ig.curr_timestamp_string();

    // INSERT NEW TEXT
    text_key new_text_k((int32_t)(db.tbl_text().gen_key()));

    auto text_v = Sto::tx_alloc<text_row>();
    text_v->old_page = page_id;
    text_v->old_flags = std::string("utf-8");
    text_v->old_text = new char[page_text.length() + 1];
    memcpy(text_v->old_text, page_text.c_str(), page_text.length() + 1);

    std::tie(abort,result) = db.tbl_text().insert_row(new_text_k, text_v);
    TXN_CHECK(abort);
    assert(!result);

    // INSERT NEW REVISION
    revision_key new_rev_k((int32_t)(db.tbl_revision().gen_key()));

    auto rev_v = Sto::tx_alloc<revision_row>();
    rev_v->rev_page = page_id;
    rev_v->rev_text_id = bswap(new_text_k.old_id);
    rev_v->rev_comment = rev_comment;
    rev_v->rev_minor_edit = rev_minor_edit;
    rev_v->rev_user = user_id;
    rev_v->rev_user_text = user_text;
    rev_v->rev_timestamp = timestamp_str;
    rev_v->rev_deleted = 0;
    rev_v->rev_len = (int32_t)page_text.length();
    rev_v->rev_parent_id = rev_id;

    std::tie(abort, result) = db.tbl_revision().insert_row(new_rev_k, rev_v);
    TXN_CHECK(abort);
    assert(!result);

    // UPDATE PAGE TABLE
#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) =
        db.tbl_page_comm().select_row(page_key(page_id), Commute ? RowAccess::None : RowAccess::ObserveValue);
    TXN_CHECK(abort);
    assert(result);

    if (Commute) {
        commutators::Commutator<page_comm_row> comm(0, 0, timestamp_str,
            bswap(new_rev_k.rev_id), (int32_t)page_text.length());
        db.tbl_page_comm().update_row(row, comm);
    } else {
        auto new_pv = Sto::tx_alloc(reinterpret_cast<const page_comm_row *>(value));
        new_pv->page_latest = bswap(new_rev_k.rev_id);
        new_pv->page_touched = timestamp_str;
        new_pv->page_is_new = 0;
        new_pv->page_is_redirect = 0;
        new_pv->page_len = (int32_t)page_text.length();

        db.tbl_page_comm().update_row(row, new_pv);
    }
#else
    std::tie(abort, result, row, value) =
        db.tbl_page().select_row(page_key(page_id),
#if TABLE_FINE_GRAINED
            {{page_nc::page_latest, Commute ? access_t::write : access_t::update},
             {page_nc::page_touched, Commute ? access_t::write : access_t::update},
             {page_nc::page_is_new, Commute ? access_t::write : access_t::update},
             {page_nc::page_is_redirect, Commute ? access_t::write : access_t::update},
             {page_nc::page_len, Commute ? access_t::write : access_t::update}}
#else
            Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
        );
    TXN_CHECK(abort);
    assert(result);

    if (Commute) {
        commutators::Commutator<page_row> comm(0, 0, timestamp_str,
            bswap(new_rev_k.rev_id), (int32_t)page_text.length());
        db.tbl_page().update_row(row, comm);
    } else {
        auto new_pv = Sto::tx_alloc(reinterpret_cast<const page_row *>(value));
        new_pv->page_latest = bswap(new_rev_k.rev_id);
        new_pv->page_touched = timestamp_str;
        new_pv->page_is_new = 0;
        new_pv->page_is_redirect = 0;
        new_pv->page_len = (int32_t)page_text.length();

        db.tbl_page().update_row(row, new_pv);
    }
#endif

    // INSERT RECENT CHANGES
    recentchanges_key rc_k((int32_t)(db.tbl_recentchanges().gen_key()));

    auto rc_v = Sto::tx_alloc<recentchanges_row>();
    bzero(rc_v, sizeof(recentchanges_row));
    rc_v->rc_timestamp = timestamp_str;
    rc_v->rc_cur_time = timestamp_str;
    rc_v->rc_namespace = page_name_space;
    rc_v->rc_title = page_title;
    rc_v->rc_type = 0;
    rc_v->rc_minor = 0;
	rc_v->rc_cur_id = page_id;
    rc_v->rc_user = user_id;
    rc_v->rc_user_text = user_text;
    rc_v->rc_comment = rev_comment;
    rc_v->rc_this_oldid = bswap(new_text_k.old_id);
    rc_v->rc_last_oldid = text_id;
    rc_v->rc_bot = 0;
    rc_v->rc_moved_to_ns = 0;
    rc_v->rc_moved_to_title = std::string();
    rc_v->rc_ip = user_ip;
    rc_v->rc_old_len = (int32_t)page_text.length();
    rc_v->rc_new_len = (int32_t)page_text.length();

    std::tie(abort, result) = db.tbl_recentchanges().insert_row(rc_k, rc_v);
    TXN_CHECK(abort);
    assert(!result);

    // SELECT WATCHING USERS
    watchlist_idx_key k0(page_name_space, page_title, 0);
    watchlist_idx_key k1(page_name_space, page_title, std::numeric_limits<int32_t>::max());

    std::vector<int32_t> watching_users;
    auto scan_cb = [&](const watchlist_idx_key& key, const watchlist_idx_row&) {
        watching_users.push_back(bswap(key.wl_user));
        return true;
    };

    abort = db.idx_watchlist().template range_scan<decltype(scan_cb), false>(k0, k1, scan_cb, RowAccess::None, false/*! phantom protection*/);
    TXN_CHECK(abort);

    if (!watching_users.empty()) {
        // update watchlist for each user watching
        INTERACTIVE_TXN_COMMIT;

        RWTRANSACTION {

        for (auto& u : watching_users) {
            std::tie(abort, result, row, value) = db.tbl_watchlist().select_row(watchlist_key(u, page_name_space, page_title),
                Commute ? RowAccess::None : RowAccess::ObserveValue);
            TXN_DO(abort);
            //assert(result);
            if (result) {
                if (Commute) {
                    commutators::Commutator<watchlist_row> comm(timestamp_str);
                    db.tbl_watchlist().update_row(row, comm);
                } else {
                    auto new_wlv = Sto::tx_alloc(reinterpret_cast<const watchlist_row *>(value));
                    new_wlv->wl_notificationtimestamp = timestamp_str;
                    db.tbl_watchlist().update_row(row, new_wlv);
                }
            }
        }

        } RETRY(true);

        RWTRANSACTION {

        // INSERT LOG
        logging_key lg_k(db.tbl_logging().gen_key());

        auto lg_v = Sto::tx_alloc<logging_row>();
        lg_v->log_type = std::string("patrol");
        lg_v->log_action = std::string("patrol");
        lg_v->log_timestamp = timestamp_str;
        lg_v->log_user = user_id;
        lg_v->log_namespace = page_name_space;
        lg_v->log_title = page_title;
        lg_v->log_comment = rev_comment;
        lg_v->log_params = std::string();
        lg_v->log_deleted = 0;
        lg_v->log_user_text = user_text;
        lg_v->log_page = page_id;

        std::tie(abort, result) = db.tbl_logging().insert_row(lg_k, lg_v);
        TXN_DO(abort);
        assert(!result);

        // UPDATE USER
#if TPCC_SPLIT_TABLE
        std::tie(abort, result, row, value) = db.tbl_useracct_comm().select_row(useracct_key(user_id),
            Commute ? RowAccess::None : RowAccess::ObserveValue);
        TXN_DO(abort);
        assert(result);
        if (Commute) {
            commutators::Commutator<useracct_comm_row> comm(true, timestamp_str);
            db.tbl_useracct_comm().update_row(row, comm);
        } else {
            auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_comm_row *>(value));
            new_uv->user_editcount += 1;
            new_uv->user_touched = timestamp_str;
            db.tbl_useracct_comm().update_row(row, new_uv);
        }
#else
        std::tie(abort, result, row, value) = db.tbl_useracct().select_row(useracct_key(user_id),
#if TABLE_FINE_GRAINED
            {{user_nc::user_editcount, Commute ? access_t::write : access_t::update},
             {user_nc::user_touched,   Commute ? access_t::write : access_t::update}}
#else
            RowAccess::ObserveValue
#endif
        );
        TXN_DO(abort);
        assert(result);
        if (Commute) {
            commutators::Commutator<useracct_row> comm(true, timestamp_str);
            db.tbl_useracct().update_row(row, comm);
        } else {
            auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_row *>(value));
            new_uv->user_editcount += 1;
            new_uv->user_touched = timestamp_str;
            db.tbl_useracct().update_row(row, new_uv);
        }
#endif

        } RETRY(true);

        return true;
    }

    // INSERT LOG
    logging_key lg_k(db.tbl_logging().gen_key());

    auto lg_v = Sto::tx_alloc<logging_row>();
    lg_v->log_type = std::string("patrol");
    lg_v->log_action = std::string("patrol");
    lg_v->log_timestamp = timestamp_str;
    lg_v->log_user = user_id;
    lg_v->log_namespace = page_name_space;
    lg_v->log_title = page_title;
    lg_v->log_comment = rev_comment;
    lg_v->log_params = std::string();
    lg_v->log_deleted = 0;
    lg_v->log_user_text = user_text;
    lg_v->log_page = page_id;

    std::tie(abort, result) = db.tbl_logging().insert_row(lg_k, lg_v);
    TXN_CHECK(abort);
    assert(!result);

    // UPDATE USER
#if TPCC_SPLIT_TABLE
    std::tie(abort, result, row, value) = db.tbl_useracct_comm().select_row(useracct_key(user_id),
        Commute ? RowAccess::None : RowAccess::ObserveValue);
    TXN_CHECK(abort);
    assert(result);
    if (Commute) {
        commutators::Commutator<useracct_comm_row> comm(true, timestamp_str);
        db.tbl_useracct_comm().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_comm_row *>(value));
        new_uv->user_editcount += 1;
        new_uv->user_touched = timestamp_str;
        db.tbl_useracct_comm().update_row(row, new_uv);
    }
#else
    std::tie(abort, result, row, value) = db.tbl_useracct().select_row(useracct_key(user_id),
#if TABLE_FINE_GRAINED
        {{user_nc::user_editcount, Commute ? access_t::write : access_t::update},
         {user_nc::user_touched,   Commute ? access_t::write : access_t::update}}
#else
        Commute ? RowAccess::None : RowAccess::ObserveValue
#endif
    );
    TXN_CHECK(abort);
    assert(result);
    if (Commute) {
        commutators::Commutator<useracct_row> comm(true, timestamp_str);
        db.tbl_useracct().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc(reinterpret_cast<const useracct_row *>(value));
        new_uv->user_editcount += 1;
        new_uv->user_touched = timestamp_str;
        db.tbl_useracct().update_row(row, new_uv);
    }
#endif

    INTERACTIVE_TXN_COMMIT;

    return true;
}

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run_txn_updatePage(int text_id,
                                                    int page_id,
                                                    const std::string& page_title,
                                                    const std::string& page_text,
                                                    int page_name_space,
                                                    int user_id,
                                                    const std::string& user_ip,
                                                    const std::string& user_text,
                                                    int rev_id,
                                                    const std::string& rev_comment,
                                                    int rev_minor_edit) {
    size_t nexecs = 0;
    while (true) {
        bool success =
            txn_updatePage_inner(text_id, page_id, page_title, page_text,
                                 page_name_space, user_id, user_ip, user_text,
                                 rev_id, rev_comment, rev_minor_edit, nexecs);
        if (success)
            break;
    }
    return (nexecs - 1);
}

}; // namespace wikipedia
