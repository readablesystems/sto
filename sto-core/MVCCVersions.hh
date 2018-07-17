#pragma once

#include "VersionBase.hh"
#include "MVCCHistory.hh"

// Default MVCC version (opaque-only)
template <typename T, bool Trivial>
class TMvVersion : public BasicVersion<TMvVersion<T, Trivial>> {
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
        return item.read_value<MvHistory<T, Trivial>>().check_version();
    }

    inline bool observe_read_impl(TransItem&, bool) {
        return false;
    }

    inline bool observe_read_impl(TransItem &item, bool add_read, MvHistory<T, Trivial>& h);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);

private:
//    template <typename W>
//    typename W::history *h_;
};
