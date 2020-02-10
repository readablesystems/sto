#pragma once

#include <string>
#include <cassert>
#include "DB_structs.hh"
#include "str.hh"

namespace wikipedia {

using namespace bench;

// New table: ipblocks

struct __attribute__((packed)) ipblocks_key_bare {
    int32_t ipb_id;
    explicit ipblocks_key_bare(int32_t p_ipb_id)
        : ipb_id(bswap(p_ipb_id)) {}

    friend masstree_key_adapter<ipblocks_key_bare>;
private:
    ipblocks_key_bare() = default;
};

typedef masstree_key_adapter<ipblocks_key_bare> ipblocks_key;

struct ipblocks_row {
    enum class NamedColumn : int { ipb_address = 0,
                                   ipb_user,
                                   ipb_by,
                                   ipb_by_text,
                                   ipb_reason,
                                   ipb_timestamp,
                                   ipb_auto,
                                   ipb_anon_only,
                                   ipb_create_account,
                                   ipb_enable_autoblock,
                                   ipb_expiry,
                                   ipb_range_start,
                                   ipb_range_end,
                                   ipb_deleted,
                                   ipb_block_email,
                                   ipb_allow_usertalk };

    var_string<255> ipb_address;
    int32_t ipb_user;
    int32_t ipb_by;
    var_string<255> ipb_by_text;
    var_string<255> ipb_reason;
    var_string<14> ipb_timestamp;
    int32_t ipb_auto;
    int32_t ipb_anon_only;
    int32_t ipb_create_account;
    int32_t ipb_enable_autoblock;
    var_string<14> ipb_expiry;
    var_string<8> ipb_range_start;
    var_string<8> ipb_range_end;
    int32_t ipb_deleted;
    int32_t ipb_block_email;
    int32_t ipb_allow_usertalk;
};

struct __attribute__((packed)) ipblocks_addr_idx_key_bare {
    var_string<255> ipb_address;
    int32_t ipb_user;
    int32_t ipb_auto;
    int32_t ipb_anon_only;
    int32_t ipb_id;

    explicit ipblocks_addr_idx_key_bare(const std::string& p_ipb_address,
                                        int32_t p_ipb_user,
                                        int32_t p_ipb_auto,
                                        int32_t p_ipb_anon_only,
                                        int32_t p_ipb_id)
        : ipb_address(p_ipb_address), ipb_user(bswap(p_ipb_user)),
          ipb_auto(bswap(p_ipb_auto)), ipb_anon_only(bswap(p_ipb_anon_only)), ipb_id(p_ipb_id) {}

    friend masstree_key_adapter<ipblocks_addr_idx_key_bare>;
private:
    ipblocks_addr_idx_key_bare() = default;
};

typedef masstree_key_adapter<ipblocks_addr_idx_key_bare> ipblocks_addr_idx_key;

using ipblocks_addr_idx_row = dummy_row;

struct __attribute__((packed)) ipblocks_user_idx_key_bare {
    int32_t ipb_user;
    var_string<255> ipb_address;
    int32_t ipb_auto;
    int32_t ipb_anon_only;
    int32_t ipb_id;

    explicit ipblocks_user_idx_key_bare(int32_t p_ipb_user,
                                        const std::string& p_ipb_address,
                                        int32_t p_ipb_auto,
                                        int32_t p_ipb_anon_only,
                                        int32_t p_ipb_id)
        : ipb_user(bswap(p_ipb_user)), ipb_address(p_ipb_address),
          ipb_auto(bswap(p_ipb_auto)), ipb_anon_only(bswap(p_ipb_anon_only)), ipb_id(p_ipb_id) {}

    friend masstree_key_adapter<ipblocks_user_idx_key_bare>;
private:
    ipblocks_user_idx_key_bare() = default;
};

typedef masstree_key_adapter<ipblocks_user_idx_key_bare> ipblocks_user_idx_key;

using ipblocks_user_idx_row  = dummy_row;

// New table: logging

struct __attribute__((packed)) logging_key_bare {
    int32_t log_id;
    explicit logging_key_bare(int32_t p_log_id)
        : log_id(bswap(p_log_id)) {}
};

typedef masstree_key_adapter<logging_key_bare> logging_key;

struct logging_row {
    enum class NamedColumn : int { log_type = 0,
                                   log_action,
                                   log_timestamp,
                                   log_user,
                                   log_namespace,
                                   log_title,
                                   log_comment,
                                   log_params,
                                   log_deleted,
                                   log_user_text,
                                   log_page };

    var_string<32> log_type;
    var_string<32> log_action;
    var_string<14> log_timestamp;
    int32_t log_user;
    int32_t log_namespace;
    var_string<255> log_title;
    var_string<255> log_comment;
    var_string<255> log_params;
    int32_t log_deleted;
    var_string<255> log_user_text;
    int32_t log_page;
};

// New table: page

struct __attribute__((packed)) page_key_bare {
    int32_t page_id;
    explicit page_key_bare(int32_t p_page_id)
        : page_id(bswap(p_page_id)) {}
};

typedef masstree_key_adapter<page_key_bare> page_key;

struct page_row_infreq {
    enum class NamedColumn : int { page_namespace = 0,
                                   page_title,
                                   page_restrictions,
                                   page_counter,
                                   page_random };

    int32_t page_namespace;
    var_string<255> page_title;
    var_string<255> page_restrictions;
    int64_t page_counter;
    float page_random;
};

struct page_row_frequpd {
    enum class NamedColumn : int { page_is_redirect = 0,
                                   page_is_new,
                                   page_touched,
                                   page_latest,
                                   page_len };

    int32_t page_is_redirect;
    int32_t page_is_new;
    var_string<14> page_touched;
    int32_t page_latest;
    int32_t page_len;
};

struct page_row {
    enum class NamedColumn : int { page_namespace = 0,
                                   page_title,
                                   page_restrictions,
                                   page_counter,
                                   page_random,
                                   page_is_redirect,
                                   page_is_new,
                                   page_touched,
                                   page_latest,
                                   page_len };

    int32_t page_namespace;
    var_string<255> page_title;
    var_string<255> page_restrictions;
    int64_t page_counter;
    float page_random;
    int32_t page_is_redirect;
    int32_t page_is_new;
    var_string<14> page_touched;
    int32_t page_latest;
    int32_t page_len;
};

struct __attribute__((packed)) page_idx_key_bare {
    int32_t page_namespace;
    var_string<255> page_title;

    explicit page_idx_key_bare(int32_t p_page_namespace,
                               const std::string& p_page_title)
        : page_namespace(bswap(p_page_namespace)), page_title(p_page_title) {}

    friend masstree_key_adapter<page_idx_key_bare>;
private:
    page_idx_key_bare() = default;
};

typedef masstree_key_adapter<page_idx_key_bare> page_idx_key;

struct page_idx_row {
    enum class NamedColumn : int { page_id = 0 };
    int32_t page_id;
};

// New table: page_restrictions

struct __attribute__((packed)) page_restrictions_key_bare {
    int32_t pr_id;
    explicit page_restrictions_key_bare(int32_t p_pr_id)
        : pr_id(bswap(p_pr_id)) {}
};

typedef masstree_key_adapter<page_restrictions_key_bare> page_restrictions_key;

struct page_restrictions_row {
    enum class NamedColumn : int { pr_page = 0,
                                   pr_type,
                                   pr_level,
                                   pr_cascade,
                                   pr_user,
                                   pr_expiry };
    int32_t pr_page;
    var_string<60> pr_type;
    var_string<60> pr_level;
    int32_t pr_cascade;
    int32_t pr_user;
    var_string<14> pr_expiry;
};

struct __attribute__((packed)) page_restrictions_idx_key_bare {
    int32_t pr_page;
    var_string<60> pr_type;
    explicit page_restrictions_idx_key_bare(int32_t p_pr_page,
                                            const std::string& p_pr_type)
        : pr_page(bswap(p_pr_page)), pr_type(p_pr_type) {}

    friend masstree_key_adapter<page_restrictions_idx_key_bare>;
private:
    page_restrictions_idx_key_bare() = default;
};

typedef masstree_key_adapter<page_restrictions_idx_key_bare> page_restrictions_idx_key;

struct page_restrictions_idx_row {
    enum class NamedColumn : int { pr_id = 0 };
    int32_t pr_id;
};

// New table: recentchanges

struct __attribute__((packed)) recentchanges_key_bare {
    int32_t rc_id;
    explicit recentchanges_key_bare(int32_t p_rc_id)
        : rc_id(bswap(p_rc_id)) {}
};

typedef masstree_key_adapter<recentchanges_key_bare> recentchanges_key;

struct recentchanges_row {
    enum class NamedColumn : int { rc_timestamp = 0,
                                   rc_cur_time,
                                   rc_user,
                                   rc_user_text,
                                   rc_namespace,
                                   rc_title,
                                   rc_comment,
                                   rc_minor,
                                   rc_bot,
                                   rc_new,
                                   rc_cur_id,
                                   rc_this_oldid,
                                   rc_last_oldid,
                                   rc_type,
                                   rc_moved_to_ns,
                                   rc_moved_to_title,
                                   rc_patrolled,
                                   rc_ip,
                                   rc_old_len,
                                   rc_new_len,
                                   rc_deleted,
                                   rc_logid,
                                   rc_log_type,
                                   rc_log_action,
                                   rc_params };

    var_string<14> rc_timestamp;
    var_string<14> rc_cur_time;
    int32_t rc_user;
    var_string<255> rc_user_text;
    int32_t rc_namespace;
    var_string<255> rc_title;
    var_string<255> rc_comment;
    int32_t rc_minor;
    int32_t rc_bot;
    int32_t rc_new;
    int32_t rc_cur_id;
    int32_t rc_this_oldid;
    int32_t rc_last_oldid;
    int32_t rc_type;
    int32_t rc_moved_to_ns;
    var_string<255> rc_moved_to_title;
    int32_t rc_patrolled;
    var_string<40> rc_ip;
    int32_t rc_old_len;
    int32_t rc_new_len;
    int32_t rc_deleted;
    int32_t rc_logid;
    var_string<255> rc_log_type;
    var_string<255> rc_log_action;
    var_string<255> rc_params;
};

// New table: revision

struct __attribute__((packed)) revision_key_bare {
    int32_t rev_id;
    explicit revision_key_bare(int32_t p_rev_id)
        : rev_id(bswap(p_rev_id)) {}
};

typedef masstree_key_adapter<revision_key_bare> revision_key;

struct revision_row {
    enum class NamedColumn : int { rev_page = 0,
                                   rev_text_id,
                                   rev_comment,
                                   rev_user,
                                   rev_user_text,
                                   rev_timestamp,
                                   rev_minor_edit,
                                   rev_deleted,
                                   rev_len,
                                   rev_parent_id };

    int32_t rev_page;
    int32_t rev_text_id;
    var_string<1024> rev_comment;
    int32_t rev_user;
    var_string<255> rev_user_text;
    var_string<14> rev_timestamp;
    int32_t rev_minor_edit;
    int32_t rev_deleted;
    int32_t rev_len;
    int32_t rev_parent_id;
};

// New table: text

struct __attribute__((packed)) text_key_bare {
    int32_t old_id;
    explicit text_key_bare(int32_t p_old_id)
        : old_id(bswap(p_old_id)) {}
};

typedef masstree_key_adapter<text_key_bare> text_key;

struct text_row {
    enum class NamedColumn : int { old_text = 0,
                                   old_flags,
                                   old_page };

    char * old_text;
    var_string<30> old_flags;
    int32_t old_page;
};

// New table: useracct

struct __attribute__((packed)) useracct_key_bare {
    int32_t user_id;
    explicit useracct_key_bare(int32_t p_user_id)
        : user_id(bswap(p_user_id)) {}
};

typedef masstree_key_adapter<useracct_key_bare> useracct_key;

struct useracct_row_infreq {
    enum class NamedColumn : int { user_name = 0,
                                   user_real_name,
                                   user_password,
                                   user_newpassword,
                                   user_newpass_time,
                                   user_email,
                                   user_options,
                                   user_token,
                                   user_email_authenticated,
                                   user_email_token,
                                   user_email_token_expires,
                                   user_registration };

    var_string<255> user_name;
    var_string<255> user_real_name;
    var_string<32> user_password;
    var_string<32> user_newpassword;
    var_string<14> user_newpass_time;
    var_string<40> user_email;
    var_string<255> user_options;
    var_string<32> user_token;
    var_string<32> user_email_authenticated;
    var_string<32> user_email_token;
    var_string<14> user_email_token_expires;
    var_string<14> user_registration;
};

struct useracct_row_frequpd {
    enum class NamedColumn : int { user_touched = 0,
                                   user_editcount };

    var_string<14> user_touched;
    int32_t user_editcount;
};

struct useracct_row {
    enum class NamedColumn : int { user_name = 0,
                                   user_real_name,
                                   user_password,
                                   user_newpassword,
                                   user_newpass_time,
                                   user_email,
                                   user_options,
                                   user_token,
                                   user_email_authenticated,
                                   user_email_token,
                                   user_email_token_expires,
                                   user_registration,
                                   user_touched,
                                   user_editcount };

    var_string<255> user_name;
    var_string<255> user_real_name;
    var_string<32> user_password;
    var_string<32> user_newpassword;
    var_string<14> user_newpass_time;
    var_string<40> user_email;
    var_string<255> user_options;
    var_string<32> user_token;
    var_string<32> user_email_authenticated;
    var_string<32> user_email_token;
    var_string<14> user_email_token_expires;
    var_string<14> user_registration;
    var_string<14> user_touched;
    int32_t user_editcount;
};

struct __attribute__((packed)) useracct_idx_key_bare {
    var_string<255> user_name;
    explicit useracct_idx_key_bare(const std::string& p_user_name)
        : user_name(p_user_name) {}
};

typedef masstree_key_adapter<useracct_idx_key_bare> useracct_idx_key;

struct useracct_idx_row {
    enum class NamedColumn : int { user_id = 0 };
    int32_t user_id;
};

// New table: user_groups

struct __attribute__((packed)) user_groups_key_bare {
    int32_t ug_user;
    explicit user_groups_key_bare(int32_t p_ug_user)
        : ug_user(bswap(p_ug_user)) {}
};

typedef masstree_key_adapter<user_groups_key_bare> user_groups_key;

struct user_groups_row {
    enum class NamedColumn : int { ug_group = 0 };
    var_string<16> ug_group;
};

// New table: watchlist

struct __attribute__((packed)) watchlist_key_bare {
    int32_t wl_user;
    int32_t wl_namespace;
    var_string<255> wl_title;
    explicit watchlist_key_bare(int32_t p_wl_user,
                                int32_t p_wl_namespace,
                                const std::string& p_wl_title)
        : wl_user(bswap(p_wl_user)), wl_namespace(bswap(p_wl_namespace)),
          wl_title(p_wl_title) {}
};

typedef masstree_key_adapter<watchlist_key_bare> watchlist_key;

struct watchlist_row {
    enum class NamedColumn : int { wl_notificationtimestamp = 0 };

    var_string<14> wl_notificationtimestamp;
};

struct __attribute__((packed)) watchlist_idx_key_bare {
    int32_t wl_namespace;
    var_string<255> wl_title;
    int32_t wl_user;
    explicit watchlist_idx_key_bare(int32_t p_wl_namespace,
                                    const std::string& p_wl_title,
                                    int32_t p_wl_user)
        : wl_namespace(bswap(p_wl_namespace)), wl_title(p_wl_title),
          wl_user(bswap(p_wl_user)) {}

    friend masstree_key_adapter<watchlist_idx_key_bare>;
private:
    watchlist_idx_key_bare() = default;
};

typedef masstree_key_adapter<watchlist_idx_key_bare> watchlist_idx_key;

using watchlist_idx_row = dummy_row;

// Article type returned by getPageAnonymous transactions

struct article_type {
    int32_t text_id;
    int32_t page_id;
    int32_t rev_id;
    std::string old_text;
    std::string user_text;
};

}; // namespace wikipedia
