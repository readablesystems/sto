#pragma once

#include "VersionBase.hh"
#include "MVCCStructs.hh"

// MVCC version, with opacity by default
template <typename T>
class TMvVersion : public BasicVersion<TMvVersion<T>> {
public:
    typedef TransactionTid::type type;

    TMvVersion() = default;
    explicit TMvVersion(type v)
            : BV(v) {}
    TMvVersion(type v, bool insert)
            : BV(v) {(void)insert;}

    bool cp_check_version_impl(Transaction& txn, TransItem& item) {
        (void)txn;
        assert(item.has_read());
        if (TransactionTid::is_locked(v_) && !item.has_write())
            return false;
        fence();
        const auto& h = *item.read_value<MvHistory<T>*>();
//        if (Sto::commit_tid() > 100000000 && Sto::commit_tid() < 200000000)
//            printf("CHCK Thread %d: commit_tid(%lu) history(%p) rtid(%lu) hnext(%p)\n",
//                Sto::transaction()->threadid(), Sto::commit_tid(), &h, h.rtid, h.next());
        return Sto::commit_tid() >= h.rtid && !h.next();
        //return Sto::commit_tid() >= h.rtid && (
        //    !h.next() || Sto::commit_tid() < h.next()->wtid);
    }

    inline bool observe_read_impl(TransItem& item, MvHistory<T> *history);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);
private:
    using BV = BasicVersion<TMvVersion<T>>;
    using BV::v_;
    using BV::check_version;
};

