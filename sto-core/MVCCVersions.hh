#pragma once

#include "VersionBase.hh"
#include "MVCCStructs.hh"

// MVCC version, with opacity by default
template <typename T>
class TMvVersion : public BasicVersion<TMvVersion<T>> {
public:
    typedef MvHistory<T> history_type;
    typedef MvObject<T> object_type;
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
        history_type *hprev = item.read_value<history_type*>();
        object_type *obj = item.mvcc_object<object_type>();
        return obj->cp_check(Sto::commit_tid(), hprev);
    }

    inline bool acquire_write_impl(TransItem& item);
    inline bool acquire_write_impl(TransItem& item, const T& wdata);
    inline bool acquire_write_impl(TransItem& item, T&& wdata);
    template <typename... Args>
    inline bool acquire_write_impl(TransItem& item, Args&&... args);

    static inline type& cp_access_tid_impl(Transaction& txn);
    inline type cp_commit_tid_impl(Transaction& txn);
    bool cp_try_lock_impl(TransItem& item, int threadid) {
        object_type *obj = item.mvcc_object<object_type>();
        history_type *hprev = nullptr;
        if (item.has_read()) {
            hprev = item.read_value<history_type*>();
        } else {
            hprev = MvObjectProxy<T>::current_record(obj);
        }
        if (Sto::commit_tid() < hprev->rtid()) {
            return false;
        }
        history_type *h = new history_type(
            Sto::commit_tid(), obj, item.write_value<T>(), hprev);
        return obj->cp_lock(Sto::commit_tid(), h);
    }

    inline bool observe_read_impl(TransItem& item, const history_type *h);

    inline type snapshot(const TransItem& item, const Transaction& txn);
    inline type snapshot(TransProxy& item);

private:
    using BV = BasicVersion<TMvVersion<T>>;
    using BV::v_;
    using BV::check_version;

//    typedef std::pair<object_type*, history_type*> rv_type;
//    typedef std::pair<const object_type*, const history_type*> const_rv_type;
};

