#pragma once

#include <set>
#include "Wikipedia_bench.hh"

namespace wikipedia {

template <typename DBParams>
void wikipedia_loader<DBParams>::load() {
    std::cout << "Loading database..." << std::endl;

    wikipedia_loader::initialize_scratch_space((size_t)num_users, (size_t)num_pages);
    load_revision();
    load_useracct();
    load_page();
    load_watchlist();
    wikipedia_loader::free_scratch_space();

    std::cout << "Loaded." << std::endl;
}

template <typename DBParams>
void wikipedia_loader<DBParams>::load_useracct() {
    for (int uid = 1; uid <= num_users; ++uid) {
        useracct_row u_r;
        u_r.user_name = ig.generate_user_name();
        u_r.user_real_name = ig.generate_user_real_name();
        u_r.user_password = "password";
        u_r.user_newpassword = "newpassword";
        u_r.user_newpass_time = ig.curr_timestamp_string();
        u_r.user_email = "user@example.com";
        u_r.user_options = "fake_longoptionslist";
        u_r.user_touched = ig.curr_timestamp_string();
        u_r.user_token = ig.generate_user_token();
        u_r.user_email_authenticated = "null";
        u_r.user_email_token = "null";
        u_r.user_email_token_expires = "null";
        u_r.user_registration = "null";
        u_r.user_editcount = user_revision_cnts[uid - 1];

        db.tbl_useracct().nontrans_put(useracct_key(uid), u_r);
    }
}

template <typename DBParams>
void wikipedia_loader<DBParams>::load_page() {
    for (int pid = 1; pid <= num_pages; ++pid) {
        int page_ns = ig.generate_page_namespace(pid);
        auto page_title = ig.generate_page_title(pid);
        auto page_restrictions = ig.generate_page_restrictions();
        page_row pg_r;
        pg_r.page_namespace = page_ns;
        pg_r.page_title = page_title;
        pg_r.page_restrictions = page_restrictions;
        pg_r.page_counter = 0;
        pg_r.page_is_redirect = 0;
        pg_r.page_is_new = 0;
        pg_r.page_random = ig.generate_page_random();
        pg_r.page_touched = ig.curr_timestamp_string();
        pg_r.page_latest = page_last_rev_ids[pid - 1];
        pg_r.page_len = page_last_rev_lens[pid - 1];

        db.tbl_page().nontrans_put(page_key(pid), pg_r);

        page_idx_row pi_r{};
        pi_r.page_id = pid;
        db.idx_page().nontrans_put(page_idx_key(page_ns, page_title), pi_r);
    }
}

template <typename DBParams>
void wikipedia_loader<DBParams>::load_watchlist() {
    std::set<int> user_pages;
    for (int uid = 1; uid <= num_users; ++uid) {
        user_pages.clear();
        auto num_watches = ig.generate_num_watches();
        for (int wid = 1; wid <= num_watches; ++wid) {
            int page_id;
            if (num_watches == constants::max_watches_per_user) {
                page_id = wid + 1;
            } else {
                while (true) {
                    page_id = ig.generate_page_id();
                    if (user_pages.find(page_id) == user_pages.end()) {
                        break;
                    }
                }
            }

            user_pages.insert(page_id);

            auto ns = ig.generate_page_namespace(page_id);
            auto title = ig.generate_page_title(page_id);
            watchlist_key wl_k(uid, ns, title);
            watchlist_idx_key wl_i_k(ns, title, uid);
            watchlist_row wl_r;
            wl_r.wl_notificationtimestamp = "null";

            db.tbl_watchlist().nontrans_put(wl_k, wl_r);
            db.idx_watchlist().nontrans_put(wl_i_k, watchlist_idx_row());
        }
    }
}

template <typename DBParams>
void wikipedia_loader<DBParams>::load_revision() {

    for (int pid = 1; pid <= num_pages; ++pid) {
        auto num_revs = ig.generate_num_revisions();
        auto old_text = ig.generate_random_old_text();
        auto old_text_len = old_text.length();

        for (int i = 0; i < num_revs; ++i) {
            auto tr_id = (int)db.tbl_revision().gen_key(); // rev id
            auto tx_id = (int)db.tbl_text().gen_key(); // text id
            always_assert(tx_id == tr_id, "text_id and rev_id should be the same at load time");

            auto uid = ig.generate_user_id();
            ++user_revision_cnts[uid - 1];
            if (i > 0) {
                old_text = ig.generate_rev_text(old_text);
                old_text_len = old_text.length();
            }

            text_key t_k(tx_id);
            text_row t_r;
            t_r.old_text = new char[old_text_len + 1];
            memcpy(t_r.old_text, old_text.c_str(), old_text_len + 1);
            t_r.old_flags = "utf-8";
            t_r.old_page = pid;
            db.tbl_text().nontrans_put(t_k, t_r);

            revision_key r_k(tr_id);
            revision_row r_r;
            r_r.rev_page = pid;
            r_r.rev_text_id = tr_id;
            r_r.rev_comment = ig.generate_rev_comment();
            r_r.rev_user = uid;
            r_r.rev_user_text = "I am a good user";
            r_r.rev_timestamp = ig.curr_timestamp_string();
            r_r.rev_minor_edit = ig.generate_rev_minor_edit();
            r_r.rev_deleted = 0;
            r_r.rev_len = (int)old_text_len;
            r_r.rev_parent_id = 0;

            db.tbl_revision().nontrans_put(r_k, r_r);

            page_last_rev_ids[pid - 1] = tr_id;
            page_last_rev_lens[pid - 1] = tr_id;
        }
    }
}

}; // namespace wikipedia
