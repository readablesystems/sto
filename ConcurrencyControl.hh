#pragma once

#include "Transaction.hh"
#include "TransItem.hh"
#include "VersionBase.hh"
#include "EagerVersions.hh"
#include "OCCVersions.hh"

class VersionDelegate {
    friend class TVersion;
    friend class TNonopaqueVersion;
    friend class TCommutativeVersion;
    friend class TLockVersion;
    template <bool Opaque>
    friend class TSwissVersion;

    static void item_or_flags(TransItem& item, TransItem::flags_type flags) {
        item.__or_flags(flags);
    }
    template <typename T, typename... Args>
    static void item_pack_to_wdata(Transaction& txn, TransItem& item, Args&&... args) {
        item.wdata_ = Packer<T>::pack(txn.buf_, std::forward<Args>(args)...);
    };
    template <typename T, typename... Args>
    static void item_repack_to_wdata(Transaction& txn, TransItem& item, Args&&... args) {
        item.wdata_ = Packer<T>::repack(txn.buf_, item.wdata_, std::forward<Args>(args)...);
    };

    template <typename T>
    static void item_pack_to_rdata(Transaction& txn, TransItem& item, const T& val) {
        item_pack_to_rdata<T, const T&>(txn, item, val);
    }
    template <typename T>
    static void item_pack_to_rdata(Transaction& txn, TransItem& item, T&& val) {
        typedef typename std::decay<T>::type V;
        item_pack_to_rdata<V, V&&>(txn, item, std::move(val));
    }
    template <typename T, typename... Args>
    static void item_pack_to_rdata(Transaction& txn, TransItem& item, Args&&... args) {
        item.rdata_ = Packer<T>::pack(txn.buf_, std::forward<Args>(args)...);
    };

    static void txn_set_any_writes(Transaction& txn, bool val) {
        txn.any_writes_ = val;
    }
    static void txn_set_any_nonopaque(Transaction& txn, bool val) {
        txn.any_nonopaque_ = val;
    }

    static TransactionTid::type& standard_tid(Transaction& txn) {
        return txn.commit_tid_;
    }
    // reserved for TicToc
    //static WideTransactionTid::type& wide_tid(Transaction& txn) {
    //    return txn.w_commit_tid_;
    //}
};

// Registering writes without passing the version (handled internally by TItem)
inline TransProxy& TransProxy::add_write() {
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        t()->any_writes_ = true;
    }
    return *this;
}

template <typename T>
inline TransProxy& TransProxy::add_write(const T& wdata) {
    return add_write<T, const T&>(wdata);
}

template <typename T>
inline TransProxy& TransProxy::add_write(T&& wdata) {
    typedef typename std::decay<T>::type V;
    return add_write<V, V&&>(std::move(wdata));
}

template <typename T, typename... Args>
inline TransProxy& TransProxy::add_write(Args&&... args) {
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        item().wdata_ = Packer<T>::pack(t()->buf_, std::forward<Args>(args)...);
        t()->any_writes_ = true;
    } else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);
    return *this;
}

// Helper function accessing the transaction object on this thread
static inline Transaction& t() {
    return *TThread::txn;
}

// STO basic concurrency control (optimistic)

template <typename VersImpl>
inline bool BasicVersion<VersImpl>::acquire_write_impl(TransItem& item) {
    TransProxy(t(), item).add_write();
    return true;
}
template <typename VersImpl> template <typename T>
inline bool BasicVersion<VersImpl>::acquire_write_impl(TransItem& item, const T& wdata) {
    TransProxy(t(), item).add_write(wdata);
    return true;
}
template <typename VersImpl> template <typename T>
inline bool BasicVersion<VersImpl>::acquire_write_impl(TransItem& item, T&& wdata) {
    TransProxy(t(), item).add_write(wdata);
    return true;
}
template <typename VersImpl> template <typename T, typename... Args>
inline bool BasicVersion<VersImpl>::acquire_write_impl(TransItem& item, Args&&... args) {
    TransProxy(t(), item).add_write<T, Args...>(std::forward<Args>(args)...);
    return true;
}

// STO opaque optimistic concurrency control

inline bool TVersion::observe_read_impl(TransItem &item, bool add_read){
    assert(!item.has_stash());
    TVersion version = *this;
    fence();

    if (version.is_locked_elsewhere(TThread::id())) {
        t().mark_abort_because(&item, "locked", version.value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (!t().check_opacity(item, version.value()))
        return false;
    if (add_read && !item.has_read()) {
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        VersionDelegate::item_pack_to_rdata(t(), item, std::move(version));
        //item().__or_flags(TransItem::read_bit);
        //item().rdata_ = Packer<TVersion>::pack(t()->buf_, std::move(version));
    }

    return true;
}

// STO nonopaque optimistic concurrency control

inline bool TNonopaqueVersion::observe_read_impl(TransItem& item, bool add_read) {
    assert(!item.has_stash());
    TNonopaqueVersion version = *this;
    fence();
    if (version.is_locked_elsewhere(TThread::id())) {
        t().mark_abort_because(&item, "locked", version.value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (add_read && !item.has_read()) {
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        VersionDelegate::item_pack_to_rdata(t(), item, std::move(version));
        VersionDelegate::txn_set_any_nonopaque(t(), true);
        //item().__or_flags(TransItem::read_bit);
        //item().rdata_ = Packer<TNonopaqueVersion>::pack(t()->buf_, std::move(version));
        //t()->any_nonopaque_ = true;
    }
    return true;
}


// Adaptive Reader/Writer lock concurrency control

inline bool TLockVersion::try_upgrade_with_spin() {
    uint64_t n = 0;
    while (true) {
        if (try_upgrade() == LockResponse::locked)
            return true;
        ++n;
        if (n == (1 << STO_SPIN_BOUND_WRITE))
            return false;
        relax_fence();
    }
}

inline bool TLockVersion::try_lock_write_with_spin() {
    uint64_t n = 0;
    while (true) {
        auto r = try_lock_write();
        if (r == LockResponse::locked)
            return true;
        else if (r == LockResponse::spin)
            ++n;
        else
            return false;
        if (n == (1 << STO_SPIN_BOUND_WRITE))
            return false;
        relax_fence();
    }
}

inline std::pair<LockResponse, TLockVersion::type>
TLockVersion::try_lock_read_with_spin() {
    uint64_t n = 0;
    while (true) {
        auto r = try_lock_read();
        if (r.first != LockResponse::spin) {
            return r;
        }
        ++n;
        if (n == (1 << STO_SPIN_BOUND_WRITE))
            return {LockResponse::failed, type()};
    }
}

inline bool TLockVersion::lock_for_write(TransItem& item) {
    if (item.has_read() && item.needs_unlock()) {
        // already holding read lock; upgrade to write lock
        if (!try_upgrade_with_spin()) {
            t().mark_abort_because(&item, "upgrade_lock", value());
            return false;
        }
    } else {
        if (!try_lock_write_with_spin()) {
            t().mark_abort_because(&item, "write_lock", value());
            return false;
        }
        VersionDelegate::item_or_flags(item, TransItem::lock_bit);
    }
    return true;
    // Invariant: after this function returns, item::lock_bit is set and the
    // write lock is held on the corresponding TVersion
}

inline bool TLockVersion::acquire_write_impl(TransItem& item) {
    if (!item.has_write()) {
        if (!lock_for_write(item)) {
            TXP_INCREMENT(txp_lock_aborts);
            return false;
        }
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        VersionDelegate::txn_set_any_writes(t(), true);
    }
    return true;
}

template <typename T>
inline bool TLockVersion::acquire_write_impl(TransItem& item, const T& wdata) {
    return acquire_write_impl<T, const T&>(item, wdata);
}

template <typename T>
inline bool TLockVersion::acquire_write_impl(TransItem& item, T&& wdata) {
    typedef typename std::decay<T>::type V;
    return acquire_write_impl<V, V&&>(item, std::move(wdata));
}

template <typename T, typename... Args>
inline bool TLockVersion::acquire_write_impl(TransItem& item, Args&&... args) {
    if (!item.has_write()) {
        if (!lock_for_write(item)) {
            TXP_INCREMENT(txp_lock_aborts);
            return false;
        }
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        VersionDelegate::item_pack_to_wdata<T, Args...>(t(), item, std::forward<Args>(args)...);
        VersionDelegate::txn_set_any_writes(t(), true);
    } else {
        VersionDelegate::item_repack_to_wdata<T, Args...>(t(), item, std::forward<Args>(args)...);
    }
    return true;
}

inline bool TLockVersion::observe_read_impl(TransItem& item, bool add_read) {
    assert(!item.has_stash());

    TLockVersion occ_version;
    bool optimistic = hint_optimistic();

    optimistic = item.cc_mode_is_optimistic(optimistic);

    if (optimistic) {
        acquire_fence();
        occ_version = *this;
    }

    if (!optimistic && !item.needs_unlock() && add_read && !item.has_read()) {
        auto response = try_lock_read_with_spin();
        if (response.first == LockResponse::optimistic) {
            // fall back to optimistic mode if no more read
            // locks can be acquired
            optimistic = true;
            occ_version = TLockVersion(response.second);
        } else if (response.first == LockResponse::locked) {
            VersionDelegate::item_or_flags(item, TransItem::lock_bit);
            VersionDelegate::item_or_flags(item, TransItem::read_bit);
            // XXX hack to prevent the commit protocol from skipping unlocks
            VersionDelegate::txn_set_any_nonopaque(t(), true);
        } else {
            assert(response.first == LockResponse::failed);
            t().mark_abort_because(&item, "observe_rlock_failed", value());
            TXP_INCREMENT(txp_lock_aborts);
            return false;
        }
    }

    // Invariant: at this point, if optimistic, then occ_version is set to the
    // approperiate version observed at the time the concurrency control policy
    // has been finally deteremined
    if (optimistic) {
        // abort if not locked here
        if (occ_version.is_locked() && !item.has_write()) {
            t().mark_abort_because(&item, "locked", occ_version.value());
            TXP_INCREMENT(txp_observe_lock_aborts);
            return false;
        }
        //if (!t()->check_opacity(item(), occ_version.value()))
        //    return false;
        if (add_read && !item.has_read()) {
            VersionDelegate::item_or_flags(item, TransItem::read_bit);
            VersionDelegate::item_pack_to_rdata(t(), item, std::move(occ_version));
            //item().rdata_ = Packer<TLockVersion>::pack(t()->buf_, std::move(occ_version));
        }
    }

    return true;
}

// SwissTM concurrency control

template <bool Opaque>
inline bool TSwissVersion<Opaque>::acquire_write_impl(TransItem& item) {
    if (BV::is_locked_here())
        return true;

    while(true) {
        if (BV::is_locked()) {
            if (ContentionManager::should_abort(&t(), *this)) {
                TXP_INCREMENT(txp_lock_aborts);
                return false;
            } else {
                relax_fence();
                continue;
            }
        }

        if (try_lock()){
            break;
        }
    }

    VersionDelegate::item_or_flags(item, TransItem::write_bit | TransItem::lock_bit);
    VersionDelegate::txn_set_any_writes(t(), true);
    //item().__or_flags(TransItem::write_bit | TransItem::lock_bit);
    //t()->any_writes_ = true;
    ContentionManager::on_write(&t());

    return true;
}

template <bool Opaque> template <typename T>
inline bool TSwissVersion<Opaque>::acquire_write_impl(TransItem& item, const T &wdata) {
    return acquire_write_impl<T, const T&>(item, wdata);
}
template <bool Opaque> template <typename T>
inline bool TSwissVersion<Opaque>::acquire_write_impl(TransItem& item, T&& wdata) {
    typedef typename std::decay<T>::type V;
    return acquire_write_impl<V, V&&>(item, std::move(wdata));
}
template <bool Opaque> template <typename T, typename... Args>
inline bool TSwissVersion<Opaque>::acquire_write_impl(TransItem& item, Args&&... args) {
    if (BV::is_locked_here()) {
        VersionDelegate::item_repack_to_wdata<T, Args...>(t(), item, std::forward<Args>(args)...);
        //item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);
        return true;
    }

    while(true) {
        if (BV::is_locked()) {
            if (ContentionManager::should_abort(&t(), *this)) {
                TXP_INCREMENT(txp_lock_aborts);
                return false;
            } else {
                relax_fence();
                continue;
            }
        }

        if (try_lock()){
            break;
        }
    }

    VersionDelegate::item_or_flags(item, TransItem::write_bit | TransItem::lock_bit);
    VersionDelegate::item_pack_to_wdata<T, Args...>(t(), item, std::forward<Args>(args)...);
    VersionDelegate::txn_set_any_writes(t(), true);
    //item().__or_flags(TransItem::write_bit | TransItem::lock_bit);
    //item().wdata_ = Packer<T>::pack(t()->buf_, std::forward<Args>(args)...);
    //t()->any_writes_ = true;
    ContentionManager::on_write(&t());

    return true;
}

template <bool Opaque>
inline bool TSwissVersion<Opaque>::observe_read_impl(TransItem& item, bool add_read) {
    assert(!item.has_stash());
    auto version = *this;
    fence();
    if (version.is_read_locked()) {
        t().mark_abort_because(&item, "swiss_read_locked", version.value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (is_opaque && !t().check_opacity(item, version.value())) {
        return false;
    }
    if (add_read && !item.has_read()) {
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        VersionDelegate::item_pack_to_rdata(t(), item, std::move(version));
        //item().__or_flags(TransItem::read_bit);
        //item().rdata_ = Packer<TSwissVersion<Opaque>>::pack(t()->buf_, std::move(version));
    }
    return true;
}

#if 0
inline auto TVersion::snapshot(TransProxy& item) -> type {
    type v = value();
    item.observe_opacity(TVersion(v));
    return v;
}

inline auto TVersion::snapshot(const TransItem& item, const Transaction& txn) -> type {
    type v = value();
    const_cast<Transaction&>(txn).check_opacity(const_cast<TransItem&>(item), v);
    return v;
}

inline auto TNonopaqueVersion::snapshot(TransProxy& item) -> type {
    item.transaction().any_nonopaque_ = true;
    return value();
}

inline auto TNonopaqueVersion::snapshot(const TransItem&, const Transaction& txn) -> type {
    const_cast<Transaction&>(txn).any_nonopaque_ = true;
    return value();
}
#endif

// Commit TID definitions

TVersion::type& TVersion::cp_access_tid_impl(Transaction &txn) {
    return VersionDelegate::standard_tid(txn);
}
TVersion::type TVersion::cp_commit_tid_impl(Transaction &txn) {
    return txn.commit_tid();
}

TNonopaqueVersion::type& TNonopaqueVersion::cp_access_tid_impl(Transaction &txn) {
    return VersionDelegate::standard_tid(txn);
}
TNonopaqueVersion::type TNonopaqueVersion::cp_commit_tid_impl(Transaction &txn) {
    auto tid = cp_access_tid_impl(txn);
    if (tid != 0)
        return tid;
    else
        return TransactionTid::next_unflagged_nonopaque_version(value());
}

TCommutativeVersion::type& TCommutativeVersion::cp_access_tid_impl(Transaction &txn) {
    return VersionDelegate::standard_tid(txn);
}
TCommutativeVersion::type TCommutativeVersion::cp_commit_tid_impl(Transaction &txn) {
    always_assert(false, "not implemented");
}

TLockVersion::type& TLockVersion::cp_access_tid_impl(Transaction &txn) {
    return VersionDelegate::standard_tid(txn);
}
TLockVersion::type TLockVersion::cp_commit_tid_impl(Transaction &txn) {
    auto tid = cp_access_tid_impl(txn);
    if (tid != 0)
        return tid;
    else
       return TransactionTid::next_unflagged_version(value());
}

template <bool Opaque>
typename TSwissVersion<Opaque>::type&
TSwissVersion<Opaque>::cp_access_tid_impl(Transaction &txn) {
    return VersionDelegate::standard_tid(txn);
}
template <bool Opaque>
typename TSwissVersion<Opaque>::type
TSwissVersion<Opaque>::cp_commit_tid_impl(Transaction &txn) {
    if (Opaque) {
        return txn.commit_tid();
    } else {
        auto tid = cp_access_tid_impl(txn);
        if (tid != 0)
            return tid;
        else
            return TransactionTid::next_unflagged_nonopaque_version(v_);
    }
}

template <bool Opaque>
bool ContentionManager::should_abort(Transaction* tx, TSwissVersion<Opaque> wlock) {
    TXP_INCREMENT(txp_cm_shouldabort);
    int threadid = tx->threadid();
    threadid *= 4;
    if (aborted[threadid] == 1){
        return true;
    }

    // This transaction is still in the timid phase
    if (timestamp[threadid] == MAX_TS) {
        return true;
    }

    int owner_threadid = wlock & TransactionTid::threadid_mask;
    owner_threadid *= 4;
    //if (write_set_size[threadid] < write_set_size[owner_threadid]) {
    if (timestamp[owner_threadid] < timestamp[threadid]) {
        if (aborted[owner_threadid] == 0) {
            return true;
        } else {
            return false;
        }
    } else {
        //FIXME: this might abort a new transaction on that thread
        aborted[owner_threadid] = 1;
        return false;
    }

}
