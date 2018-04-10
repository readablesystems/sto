#pragma once

#include "Sto.hh"
#include "VersionSelector.hh"
#include "Wikipedia_structs.hh"

namespace ver_sel {

template <typename VersImpl>
class VerSel<wikipedia::page_row, VersImpl> : public VerSelBase<VerSel<wikipedia::page_row, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 3;

    explicit VerSel(type v) : vers_() { (void)v; }
    VerSel(type v, bool insert) : vers_() { (void)v; (void)insert; }

    static int map_impl(int col_n) {
        uint64_t mask = ~(~0ul << vidx_width) ;
        int shift = col_n * vidx_width;
        return ((col_cell_map & (mask << shift)) >> shift);
    }

    version_type& version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(wikipedia::page_row *dst, const wikipedia::page_row *src, int cell) {
        switch (cell) {
            case 0:
                dst->page_namespace = src->page_namespace;
                dst->page_title = src->page_title;
                dst->page_restrictions = src->page_restrictions;
                dst->page_random = src->page_random;
                break;
            case 1:
                dst->page_counter = src->page_counter;
                break;
            case 2:
                dst->page_is_redirect = src->page_is_redirect;
                dst->page_is_new = src->page_is_new;
                dst->page_touched = src->page_touched;
                dst->page_latest = src->page_latest;
                dst->page_len = src->page_len;
                break;
            default:
                always_assert(false, "cell id out of bound\n");
                break;
        }
    }

private:
    version_type vers_[num_versions];
    static constexpr unsigned int vidx_width = 2u;
    static constexpr uint64_t col_cell_map = 690752ul;
};

template<typename VersImpl>
class VerSel<wikipedia::useracct_row, VersImpl> : public VerSelBase<VerSel<wikipedia::useracct_row, VersImpl>, VersImpl> {
public:
    typedef VersImpl version_type;
    static constexpr size_t num_versions = 3;

    explicit VerSel(type v) : vers_() { (void) v; }

    VerSel(type v, bool insert) : vers_() {
        (void) v;
        (void) insert;
    }

    static int map_impl(int col_n) {
        uint64_t mask = ~(~0ul << vidx_width);
        int shift = col_n * vidx_width;
        return ((col_cell_map & (mask << shift)) >> shift);
    }

    version_type &version_at_impl(int cell) {
        return vers_[cell];
    }

    void install_by_cell_impl(wikipedia::useracct_row *dst, const wikipedia::useracct_row *src, int cell) {
        switch (cell) {
            case 0:
                dst->user_name = src->user_name;
                dst->user_real_name = src->user_real_name;
                dst->user_password = src->user_password;
                dst->user_newpassword = src->user_newpassword;
                dst->user_newpass_time = src->user_newpass_time;
                dst->user_email = src->user_email;
                dst->user_options = src->user_options;
                dst->user_token = src->user_token;
                dst->user_email_authenticated = src->user_email_authenticated;
                dst->user_email_token = src->user_email_token;
                dst->user_email_token_expires = src->user_email_token_expires;
                dst->user_registration = src->user_registration;
                break;
            case 1:
                dst->user_touched = src->user_touched;
                break;
            case 2:
                dst->user_editcount = src->user_editcount;
                break;
            default:
                always_assert(false, "cell id out of bound\n");
                break;
        }
    }

private:
    version_type vers_[num_versions];
    static constexpr unsigned int vidx_width = 2u;
    static constexpr uint64_t col_cell_map = 134234112ul;
};

}; // namespace ver_sel