#pragma once

#include "Commutators.hh"
#include "Wikipedia_structs.hh"

namespace commutators {

#if TPCC_SPLIT_TABLE
using useracct_row = wikipedia::useracct_comm_row;
using page_row = wikipedia::page_comm_row;
#else
using useracct_row = wikipedia::useracct_row;
using page_row = wikipedia::page_row;
#endif

using watchlist_row = wikipedia::watchlist_row;

template<>
class Commutator<useracct_row> {
public:
    Commutator() = default;
    explicit Commutator(bool update_page, const std::string& ts)
        : inc_edit_count(update_page), new_timestamp(ts) {}

    useracct_row& operate(useracct_row& val) const {
        if (inc_edit_count)
            val.user_editcount += 1;
        val.user_touched = new_timestamp;
        return val;
    }

private:
    bool inc_edit_count;
    bench::var_string<14> new_timestamp;
};

template <>
class Commutator<page_row> {
public:
    Commutator() = default;
    explicit Commutator(int32_t is_redir, int32_t is_new, const std::string& ts, int32_t latest, int32_t len)
        : new_is_redirect(is_redir), new_is_new(is_new), new_touched(ts), new_latest(latest), new_len(len) {}

    page_row& operate(page_row& val) const {
        val.page_is_redirect = new_is_redirect;
        val.page_is_new = new_is_new;
        val.page_touched = new_touched;
        val.page_latest = new_latest;
        val.page_len = new_len;
        return val;
    }

private:
    int32_t new_is_redirect;
    int32_t new_is_new;
    bench::var_string<14> new_touched;
    int32_t new_latest;
    int32_t new_len;
};

template <>
class Commutator<watchlist_row> {
public:
    Commutator() = default;
    explicit Commutator(const std::string& ts) : new_timestamp(ts) {}

    watchlist_row& operate(watchlist_row& val) const {
        val.wl_notificationtimestamp = new_timestamp;
        return val;
    }
private:
    bench::var_string<14> new_timestamp;
};

}
