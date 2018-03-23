#pragma once

#include <iostream>
#include <iomanip>
#include <ctime>
#include <sstream>

#include "compiler.hh"
#include "clp.h"
#include "Wikipedia_structs.hh"
#include "DB_index.hh"
#include "DB_params.hh"

namespace wikipedia {

template <typename DBParams>
class wikipedia_db {
public:
    template <typename K, typename V>
    using OIndex = ordered_index<K, V, DBParams>;

    typedef OIndex<ipblocks_key, ipblocks_row>                           ipb_tbl_type;
    typedef OIndex<ipblocks_addr_idx_key, ipblocks_addr_idx_row>         ipb_addr_idx_type;
    typedef OIndex<ipblocks_user_idx_key, ipblocks_user_idx_row>         ipb_user_idx_type;
    typedef OIndex<logging_key, logging_row>                             log_tbl_type;
    typedef OIndex<page_key, page_row>                                   page_tbl_type;
    typedef OIndex<page_idx_key, page_idx_row>                           page_idx_type;
    typedef OIndex<page_restrictions_key, page_restrictions_row>         pr_tbl_type;
    typedef OIndex<page_restrictions_idx_key, page_restrictions_idx_row> pr_idx_type;
    typedef OIndex<recentchanges_key, recentchanges_row>                 rc_tbl_type;
    typedef OIndex<revision_key, revision_row>                           rev_tbl_type;
    typedef OIndex<text_key, text_row>                                   text_tbl_type;
    typedef OIndex<useracct_key, useracct_row>                           user_tbl_type;
    typedef OIndex<useracct_idx_key, useracct_idx_row>                   user_idx_type;
    typedef OIndex<user_groups_key, user_groups_row>                     ug_tbl_type;
    typedef OIndex<watchlist_key, watchlist_row>                         wl_tbl_type;
    typedef OIndex<watchlist_idx_key, watchlist_idx_row>                 wl_idx_type;

    ipb_tbl_type& tbl_ipblocks() {
        return tbl_ipb_;
    }
    ipb_addr_idx_type& idx_ipblocks_addr() {
        return idx_ipb_addr_;
    }
    ipb_user_idx_type& idx_ipblocks_user() {
        return idx_ipb_user_;
    }
    log_tbl_type& tbl_logging() {
        return tbl_log_;
    }
    page_tbl_type& tbl_page() {
        return tbl_page_;
    }
    page_idx_type& idx_page() {
        return idx_page_;
    }
    pr_tbl_type& tbl_page_restrictions() {
        return tbl_pr_;
    }
    pr_idx_type& idx_page_restrictions() {
        return idx_pr_;
    }
    rc_tbl_type& tbl_recentchanges() {
        return tbl_rc_;
    }
    rev_tbl_type& tbl_revision() {
        return tbl_rev_;
    }
    text_tbl_type& tbl_text() {
        return tbl_text_;
    }
    user_tbl_type& tbl_useracct() {
        return tbl_user_;
    }
    user_idx_type& idx_useracct() {
        return idx_user_;
    }
    ug_tbl_type& tbl_user_groups() {
        return tbl_ug_;
    }
    wl_tbl_type& tbl_watchlist() {
        return tbl_wl_;
    }
    wl_idx_type& idx_watchlist() {
        return idx_wl_;
    }

private:

    ipb_tbl_type      tbl_ipb_;
    ipb_addr_idx_type idx_ipb_addr_;
    ipb_user_idx_type idx_ipb_user_;
    log_tbl_type      tbl_log_;
    page_tbl_type     tbl_page_;
    page_idx_type     idx_page_;
    pr_tbl_type       tbl_pr_;
    pr_idx_type       idx_pr_;
    rc_tbl_type       tbl_rc_;
    rev_tbl_type      tbl_rev_;
    text_tbl_type     tbl_text_;
    user_tbl_type     tbl_user_;
    user_idx_type     idx_user_;
    ug_tbl_type       tbl_ug_;
    wl_tbl_type       tbl_wl_;
    wl_idx_type       idx_wl_;
};

class wikipedia_input_generator {
public:
    std::string curr_timestamp_string() {
        auto t = std::time(nullptr);
        auto tm = *std::localtime(&t);
        std::stringstream ss;
        ss << std::put_time(&tm, "%d%m%Y%H-%M");
        return ss.str();
    }
};

template <typename DBParams>
class wikipedia_runner {
public:
    void run_txn_addWatchList(int user_id, int name_space, const std::string& page_title);
    void run_txn_getPageAnonymous(bool for_select, const std::string& user_ip,
                                  int name_space, const std::string& page_title);
    void run_txn_getPageAuthenticated(bool for_select, const std::string& user_ip,
                                      int user_id, int name_space, const std::string& page_title);
    void run_txn_removeWatchList(int user_id, int name_space, const std::string& page_title);
    void run_txn_updatePage(int text_id, int page_id, const std::string& page_title,
                            const std::string& page_text, int page_name_space, int user_id,
                            const std::string& user_ip, const std::string& user_text,
                            int rev_id, const std::string& rev_comment, int rev_minor_edit);

private:
    bool txn_updatePage_inner(int text_id, int page_id, const std::string& page_title,
                              const std::string& page_text, int page_name_space, int user_id,
                              const std::string& user_ip, const std::string& user_text,
                              int rev_id, const std::string& rev_comment, int rev_minor_edit);
    wikipedia_db<DBParams> db;
    wikipedia_input_generator ig;
};


template <typename DBParams>
class wikipedia_loader {
public:
    explicit wikipedia_loader(wikipedia_db<DBParams>& wdb) : db(wdb) {}
    void load();
private:
    void load_useracct();
    void load_page();
    void load_watchlist();
    void load_revision();

    wikipedia_db<DBParams>& db;
};

}; // namespace wikipedia