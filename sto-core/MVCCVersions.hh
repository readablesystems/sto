#pragma once

#include "VersionBase.hh"

// Default MVCC version (opaque-only)
class TMvVersion : public BasicVersion<TMvVersion> {
public:
    TMvVersion() = default;
    explicit TMvVersion(type v)
            : BasicVersion(v) {}
    TMvVersion(type v, bool insert)
            : BasicVersion(v) {(void)insert;}

    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        (void)txn;
        assert(item.has_read());
        if (TransactionTid::is_locked(v_) && !item.has_write())
            return false;
        return check_version(item.read_value<TMvVersion>());
    }

    // Returns a <success, value> pair
    template <typename T, typename W>
    std::pair<bool, T> read(const typename W::history& h, TransProxy item, bool add_read) {
        
    }

    inline bool observe_read_impl(TransItem& item, bool add_read);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

private:
    template <typename W>
    typename W::history *h_;
};