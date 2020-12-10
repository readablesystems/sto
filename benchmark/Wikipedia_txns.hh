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
    typedef useracct_row::NamedColumn nc;
    size_t nexecs = 0;

    RWTRANSACTION {

    ++nexecs;

    auto wv = Sto::tx_alloc<watchlist_row>();
    auto wiv = Sto::tx_alloc<watchlist_idx_row>();
    bzero(wv, sizeof(watchlist_row));
    bzero(wiv, sizeof(watchlist_idx_row));

    {
    bool abort;
    std::tie(abort, std::ignore) = db.tbl_watchlist().insert_row(watchlist_key(user_id, name_space, page_title), wv);
    TXN_DO(abort);

    std::tie(abort, std::ignore) = db.idx_watchlist().insert_row(watchlist_idx_key(name_space, page_title, user_id), wiv);
    TXN_DO(abort);
    }

    if (name_space == 0) {
        bool abort;
        std::tie(abort, std::ignore) = db.tbl_watchlist().insert_row(watchlist_key(user_id, 1, page_title), wv);
        TXN_DO(abort);
    }

    auto [abort, result, row, value] = db.tbl_useracct().select_split_row(useracct_key(user_id),
        {{nc::user_touched, Commute ? access_t::write : access_t::update}}
    );
    (void)value; (void)result;
    TXN_DO(abort);
    assert(result);
    if constexpr (Commute) {
        commutators::Commutator<useracct_row> comm(false, ig.curr_timestamp_string());
        db.tbl_useracct().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc<useracct_row>();
        value.copy_into(new_uv);
        new_uv->user_touched = ig.curr_timestamp_string();
        db.tbl_useracct().update_row(row, new_uv);
    }

    } RETRY(true);

    return (nexecs - 1);
}

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run_txn_removeWatchList(int user_id,
                                                           int name_space,
                                                           const std::string& page_title) {
    typedef useracct_row::NamedColumn nc;
    size_t nexecs = 0;

    RWTRANSACTION {

    ++nexecs;

    {
    bool abort;
    std::tie(abort, std::ignore) = db.tbl_watchlist().delete_row(watchlist_key(user_id, name_space, page_title));
    TXN_DO(abort);

    std::tie(abort, std::ignore) = db.idx_watchlist().delete_row(watchlist_idx_key(name_space, page_title, user_id));
    TXN_DO(abort);
    }

    auto [abort, result, row, value] = db.tbl_useracct().select_split_row(useracct_key(user_id),
        {{nc::user_touched, Commute ? access_t::write : access_t::update}}
    );
    (void)value; (void)result;
    TXN_DO(abort);
    assert(result);
    if constexpr (Commute) {
        commutators::Commutator<useracct_row> comm(false, ig.curr_timestamp_string());
        db.tbl_useracct().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc<useracct_row>();
        value.copy_into(new_uv);
        new_uv->user_touched = ig.curr_timestamp_string();
        db.tbl_useracct().update_row(row, new_uv);
    }

    } RETRY(true);

    return (nexecs - 1);
}

template <typename DBParams>
std::pair<size_t, article_type> wikipedia_runner<DBParams>::run_txn_getPageAnonymous(bool for_select,
                                                          const std::string& user_ip,
                                                          int name_space,
                                                          const std::string& page_title) {
    typedef page_row::NamedColumn page_nc;
    typedef page_idx_row::NamedColumn pi_nc;
    //typedef page_restrictions_row::NamedColumn pr_nc;
    //typedef ipblocks_row::NamedColumn ipb_nc;
    typedef revision_row::NamedColumn rev_nc;
    typedef text_row::NamedColumn text_nc;

    (void)for_select;
    size_t nexecs = 0;
    article_type art;

    TRANSACTION {

    ++nexecs;

    int32_t page_id;
    int32_t rev_id;
    int32_t rev_text_id;

    {
    auto [abort, result, row, value] = db.idx_page().select_split_row(page_idx_key(name_space, page_title), {{pi_nc::page_id, access_t::read}});
    (void)row; (void)result;
    TXN_DO(abort);
    assert(result);
    page_id = value.page_id();
    }

    {
    auto [abort, result, row, value] = db.tbl_page().select_split_row(page_key(page_id),
        {{page_nc::page_latest, access_t::read}}
    );
    (void)row; (void)result;
    TXN_DO(abort);
    assert(result);
    rev_id = value.page_latest();
    }

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

    {
    auto [abort, result, row, value] = db.tbl_revision().select_split_row(revision_key(rev_id),
        {{rev_nc::rev_text_id, access_t::read}});
    (void)row; (void)result;
    TXN_DO(abort);
    assert(result);
    rev_text_id = value.rev_text_id();
    }

    {
    auto [abort, result, row, value] = db.tbl_text().select_split_row(text_key(rev_text_id),
        {{text_nc::old_text, access_t::read}});
    (void)row; (void)result;
    TXN_DO(abort);
    assert(result);

    art.text_id = rev_text_id;
    art.page_id = page_id;
    art.rev_id = rev_id;
    art.old_text = std::string(value.old_text());
    art.user_text = user_ip;
    }

    } RETRY(true);

    return {nexecs - 1, art};
}

template <typename DBParams>
std::pair<size_t, article_type> wikipedia_runner<DBParams>::run_txn_getPageAuthenticated(bool for_select,
                                                              const std::string& user_ip,
                                                              int user_id,
                                                              int name_space,
                                                              const std::string& page_title) {
    typedef useracct_row::NamedColumn user_nc;
    typedef page_row::NamedColumn page_nc;
    typedef page_idx_row::NamedColumn pi_nc;
    typedef revision_row::NamedColumn rev_nc;
    typedef text_row::NamedColumn text_nc;
    //typedef page_restrictions_row::NamedColumn pr_nc;
    //typedef ipblocks_row::NamedColumn ipb_nc;

    (void)for_select;
    size_t nexecs = 0;
    article_type art;

    TRANSACTION {

    ++nexecs;

    int32_t page_id;
    int32_t rev_id;
    int32_t rev_text_id;

    {
    auto [abort, result, row, value] = db.tbl_useracct().select_split_row(useracct_key(user_id),
        {{user_nc::user_name, access_t::read}}
    );
    (void)result; (void)row; (void)value;
    TXN_DO(abort);
    assert(result);
    }

    //std::tie(abort, result, std::ignore, std::ignore) = db.tbl_user_groups().select_row(user_groups_key(user_id), RowAccess::ObserveValue);
    //TXN_DO(abort);
    //assert(result);

    // From this point is pretty much the same as getPageAnonymous 

    {
    auto [abort, result, row, value] = db.idx_page().select_split_row(page_idx_key(name_space, page_title),
        {{pi_nc::page_id, access_t::read}});
    (void)result; (void)row;
    TXN_DO(abort);
    assert(result);
    page_id = value.page_id();
    }

    {
    auto [abort, result, row, value] = db.tbl_page().select_split_row(page_key(page_id),
        {{page_nc::page_latest, access_t::read}});
    (void)result; (void)row;
    TXN_DO(abort);
    assert(result);
    rev_id = value.page_latest();
    }

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

    {
    auto [abort, result, row, value] = db.tbl_revision().select_split_row(revision_key(rev_id),
        {{rev_nc::rev_text_id, access_t::read}});
    (void)result; (void)row;
    TXN_DO(abort);
    assert(result);
    rev_text_id = value.rev_text_id();
    }

    {
    auto [abort, result, row, value] = db.tbl_text().select_split_row(text_key(rev_text_id),
        {{text_nc::old_text, access_t::read}});
    (void)result; (void)row;
    TXN_DO(abort);
    assert(result);

    art.text_id = rev_text_id;
    art.page_id = page_id;
    art.rev_id = rev_id;
    art.old_text = std::string(value.old_text());
    art.user_text = user_ip;
    }

    } RETRY(true);

    return {nexecs - 1, art};
}

template <typename DBParams>
size_t wikipedia_runner<DBParams>::run_txn_listPageNameSpace(int name_space) {
    typedef page_row::NamedColumn page_nc;
    size_t nexecs = 0;

    TRANSACTION {

    ++nexecs;

    std::vector<std::pair<int, std::string>> pages;

    auto scan_callback = [&](const page_idx_key& key, const auto& scan_value) {
        auto row = (typename std::remove_reference_t<decltype(db)>::page_idx_type::accessor_t)(scan_value);
        pages.push_back({row.page_id(), std::string(key.page_title.c_str())});
        return true;
    };

    page_idx_key pk0(name_space, std::string());
    page_idx_key pk1(name_space, std::string(255, (unsigned char)0xff));
    {
    bool abort = db.idx_page().template range_scan<decltype(scan_callback), false>(pk0, pk1, scan_callback, RowAccess::ObserveValue, false, 20/*retrieve 20 items*/);
    TXN_DO(abort);
    }

    for (auto pair : pages) {
        auto [abort, result, row, value]
                = db.tbl_page().select_split_row(page_key(pair.first),
                    {{page_nc::page_title, access_t::read}});
        (void)result; (void)row;
        TXN_DO(abort);
        assert(result);
        always_assert(pair.second == std::string(value.page_title().c_str()));
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
    typedef page_row::NamedColumn page_nc;
    typedef useracct_row::NamedColumn user_nc;
    typedef watchlist_row::NamedColumn wl_nc;

    INTERACTIVE_RWTXN_START;

    ++nstarts;

    auto timestamp_str = ig.curr_timestamp_string();

    // INSERT NEW TEXT
    text_key new_text_k((int32_t)(db.tbl_text().gen_key()));

    auto text_v = Sto::tx_alloc<text_row>();
    text_v->old_page = page_id;
    text_v->old_flags = std::string("utf-8");
    text_v->old_text = new char[page_text.length() + 1];
    memcpy(text_v->old_text, page_text.c_str(), page_text.length() + 1);

    {
    auto [abort, result] = db.tbl_text().insert_row(new_text_k, text_v);
    (void)result;
    TXN_CHECK(abort);
    assert(!result);
    }

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

    {
    auto [abort, result] = db.tbl_revision().insert_row(new_rev_k, rev_v);
    (void)result;
    TXN_CHECK(abort);
    assert(!result);
    }

    {
    // UPDATE PAGE TABLE
    auto [abort, result, row, value] =
        db.tbl_page().select_split_row(page_key(page_id),
            {{page_nc::page_latest, Commute ? access_t::write : access_t::update},
             {page_nc::page_touched, Commute ? access_t::write : access_t::update},
             {page_nc::page_is_new, Commute ? access_t::write : access_t::update},
             {page_nc::page_is_redirect, Commute ? access_t::write : access_t::update},
             {page_nc::page_len, Commute ? access_t::write : access_t::update}}
        );
    (void)result;
    TXN_CHECK(abort);
    assert(result);

    if constexpr (Commute) {
        (void)value;
        commutators::Commutator<page_row> comm(0, 0, timestamp_str, bswap(new_rev_k.rev_id), (int32_t)page_text.length());
        db.tbl_page().update_row(row, comm);
    } else {
        auto new_pv = Sto::tx_alloc<page_row>();
        value.copy_into(new_pv);
        new_pv->page_latest = bswap(new_rev_k.rev_id);
        new_pv->page_touched = timestamp_str;
        new_pv->page_is_new = 0;
        new_pv->page_is_redirect = 0;
        new_pv->page_len = (int32_t)page_text.length();

        db.tbl_page().update_row(row, new_pv);
    }
    }

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

    {
    auto [abort, result] = db.tbl_recentchanges().insert_row(rc_k, rc_v);
    (void)result;
    TXN_CHECK(abort);
    assert(!result);
    }

    // SELECT WATCHING USERS
    watchlist_idx_key k0(page_name_space, page_title, 0);
    watchlist_idx_key k1(page_name_space, page_title, std::numeric_limits<int32_t>::max());

    std::vector<int32_t> watching_users;
    auto scan_cb = [&](const watchlist_idx_key& key, const auto&) {
        watching_users.push_back(bswap(key.wl_user));
        return true;
    };

    {
    bool abort = db.idx_watchlist().template range_scan<decltype(scan_cb), false>(k0, k1, scan_cb, RowAccess::ObserveExists, false/*! phantom protection*/);
    TXN_CHECK(abort);
    }

    if (!watching_users.empty()) {
        // update watchlist for each user watching
        INTERACTIVE_TXN_COMMIT;

        RWTRANSACTION {

        for (auto& u : watching_users) {
            auto [abort, result, row, value] = db.tbl_watchlist().select_split_row(watchlist_key(u, page_name_space, page_title),
                {{wl_nc::wl_notificationtimestamp, access_t::write}});
            TXN_DO(abort);
            if (result) {
                auto new_wlv = Sto::tx_alloc<watchlist_row>();
                value.copy_into(new_wlv);
                new_wlv->wl_notificationtimestamp = timestamp_str;
                db.tbl_watchlist().update_row(row, new_wlv);
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

        {
        auto [abort, result] = db.tbl_logging().insert_row(lg_k, lg_v);
        (void)result;
        TXN_DO(abort);
        assert(!result);
        }

        {
        // UPDATE USER
        auto [abort, result, row, value] = db.tbl_useracct().select_split_row(useracct_key(user_id),
            {{user_nc::user_editcount, Commute ? access_t::write : access_t::update},
             {user_nc::user_touched,   Commute ? access_t::write : access_t::update}}
        );
        (void)result;
        TXN_DO(abort);
        assert(result);
        if constexpr (Commute) {
            (void)value;
            commutators::Commutator<useracct_row> comm(true, timestamp_str);
            db.tbl_useracct().update_row(row, comm);
        } else {
            auto new_uv = Sto::tx_alloc<useracct_row>();
            value.copy_into(new_uv);
            new_uv->user_editcount += 1;
            new_uv->user_touched = timestamp_str;
            db.tbl_useracct().update_row(row, new_uv);
        }
        }

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

    {
    auto [abort, result] = db.tbl_logging().insert_row(lg_k, lg_v);
    (void)result;
    TXN_CHECK(abort);
    assert(!result);
    }

    {
    // UPDATE USER
    auto [abort, result, row, value] = db.tbl_useracct().select_split_row(useracct_key(user_id),
        {{user_nc::user_editcount, Commute ? access_t::write : access_t::update},
         {user_nc::user_touched,   Commute ? access_t::write : access_t::update}}
    );
    (void)result;
    TXN_CHECK(abort);
    assert(result);
    if constexpr (Commute) {
        (void)value;
        commutators::Commutator<useracct_row> comm(true, timestamp_str);
        db.tbl_useracct().update_row(row, comm);
    } else {
        auto new_uv = Sto::tx_alloc<useracct_row>();
        value.copy_into(new_uv);
        new_uv->user_editcount += 1;
        new_uv->user_touched = timestamp_str;
        db.tbl_useracct().update_row(row, new_uv);
    }
    }

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
