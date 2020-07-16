#pragma once

#include "Transaction.hh"
#include "TransItem.hh"
#include "VersionBase.hh"
#include "EagerVersions.hh"
#include "OCCVersions.hh"
#include "TicTocVersions.hh"

class VersionDelegate {
    friend class TVersion;
    friend class TNonopaqueVersion;
    friend class TCommutativeVersion;
    template <bool Adaptive>
    friend class TLockVersion;
    template <bool Opaque>
    friend class TSwissVersion;
    template <bool Opaque, bool Extend>
    friend class TicTocVersion;
    template <bool Opaque, bool Extend>
    friend class TicTocCompressedVersion;

    static void item_or_flags(TransItem& item, TransItem::flags_type flags) {
        item.__or_flags(flags);
    }
    static void *& item_access_wdata(TransItem& item) {
        return item.wdata_;
    }
    static rdata_t& item_access_rdata(TransItem& item) {
        return item.rdata_;
    }

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
    static TransactionTid::type& tictoc_tid(Transaction& txn) {
        return txn.tictoc_tid_;
    }
};

// Registering commutes without passing the version (handled internally by TItem)
template <typename T>
inline TransProxy& TransProxy::add_commute(const T* comm) {
    if (has_write() && !has_commute()) {
        clear_write();
    }
    add_write(comm);
    item().__or_flags(TransItem::commute_bit);
    return *this;
}

// Registering commutes without passing the version (handled internally by TItem)
template <typename T>
inline TransProxy& TransProxy::add_commute(const T& comm) {
    if (has_write() && !has_commute()) {
        clear_write();
    }
    add_write(comm);
    item().__or_flags(TransItem::commute_bit);
    return *this;
}

// Registering MvHistory elements without passing the version (handled internally by TItem)
template <typename T>
inline TransProxy& TransProxy::add_mvhistory(const T& h) {
    if (has_commute()) {
        clear_commute();
    }
    add_write(h);
    item().__or_flags(TransItem::mvhistory_bit);
    return *this;
}

// Registering writes without passing the version (handled internally by TItem)
inline TransProxy& TransProxy::add_write() {
    if (has_commute()) {
        clear_commute();
    }
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
    if (has_commute()) {
        clear_commute();
    }
    if (!has_write()) {
        item().__or_flags(TransItem::write_bit);
        item().wdata_ = Packer<T>::pack(t()->buf_, std::forward<Args>(args)...);
        t()->any_writes_ = true;
    } else {
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);
    }
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

    if (version.is_locked_elsewhere()) {
        t().mark_abort_because(&item, "locked", version.value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (!t().check_opacity(item, version.value()))
        return false;
    if (add_read && !item.has_read()) {
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        VersionDelegate::item_access_rdata(item).v = Packer<TVersion>::pack(t().buf_, std::move(version));
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
    if (version.is_locked()) {
        t().mark_abort_because(&item, "locked", version.value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (add_read && !item.has_read()) {
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        VersionDelegate::item_access_rdata(item).v = Packer<TNonopaqueVersion>::pack(t().buf_, std::move(version));
        VersionDelegate::txn_set_any_nonopaque(t(), true);
        //item().__or_flags(TransItem::read_bit);
        //item().rdata_ = Packer<TNonopaqueVersion>::pack(t()->buf_, std::move(version));
        //t()->any_nonopaque_ = true;
    }
    return true;
}


// Adaptive Reader/Writer lock concurrency control

template <bool Adaptive>
inline bool TLockVersion<Adaptive>::try_upgrade_with_spin() {
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

template <bool Adaptive>
inline bool TLockVersion<Adaptive>::try_lock_write_with_spin() {
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

template <bool Adaptive>
inline std::pair<LockResponse, typename TLockVersion<Adaptive>::type>
TLockVersion<Adaptive>::try_lock_read_with_spin() {
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

template <bool Adaptive>
inline bool TLockVersion<Adaptive>::lock_for_write(TransItem& item) {
    if (item.has_read() && item.needs_unlock()) {
        // already holding read lock; upgrade to write lock
        if (!try_upgrade_with_spin()) {
            t().mark_abort_because(&item, "upgrade_lock", BV::value());
            return false;
        }
    } else {
        if (!try_lock_write_with_spin()) {
            t().mark_abort_because(&item, "write_lock", BV::value());
            return false;
        }
        VersionDelegate::item_or_flags(item, TransItem::lock_bit);
    }
    return true;
    // Invariant: after this function returns, item::lock_bit is set and the
    // write lock is held on the corresponding TVersion
}

template <bool Adaptive>
inline bool TLockVersion<Adaptive>::acquire_write_impl(TransItem& item) {
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

template <bool Adaptive> template <typename T>
inline bool TLockVersion<Adaptive>::acquire_write_impl(TransItem& item, const T& wdata) {
    return acquire_write_impl<T, const T&>(item, wdata);
}

template <bool Adaptive> template <typename T>
inline bool TLockVersion<Adaptive>::acquire_write_impl(TransItem& item, T&& wdata) {
    typedef typename std::decay<T>::type V;
    return acquire_write_impl<V, V&&>(item, std::move(wdata));
}

template <bool Adaptive> template <typename T, typename... Args>
inline bool TLockVersion<Adaptive>::acquire_write_impl(TransItem& item, Args&&... args) {
    if (!item.has_write()) {
        if (!lock_for_write(item)) {
            TXP_INCREMENT(txp_lock_aborts);
            return false;
        }
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        VersionDelegate::item_access_wdata(item) = Packer<T>::pack(t().buf_, std::forward<Args>(args)...);
        VersionDelegate::txn_set_any_writes(t(), true);
    } else {
        auto old_wdata = VersionDelegate::item_access_wdata(item);
        VersionDelegate::item_access_wdata(item) = Packer<T>::repack(t().buf_, old_wdata, std::forward<Args>(args)...);
    }
    return true;
}

template <bool Adaptive>
inline bool TLockVersion<Adaptive>::observe_read_impl(TransItem& item, bool add_read) {
    assert(!item.has_stash());

    TLockVersion occ_version;

    bool optimistic;
    auto init_mode = item.cc_mode();
    if (init_mode != CCMode::none) {
        optimistic = (init_mode == CCMode::opt);
    } else {
        optimistic = item.cc_mode_is_optimistic(hint_optimistic());
    }

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
            t().mark_abort_because(&item, "observe_rlock_failed", BV::value());
            TXP_INCREMENT(txp_lock_aborts);
            return false;
        }
    }

    // Invariant: at this point, if optimistic, then occ_version is set to the
    // approperiate version observed at the time the concurrency control policy
    // has been finally deteremined
    if (optimistic) {
        // abort if not locked here
        if (occ_version.is_dirty()) {
            t().mark_abort_because(&item, "lock-dirty", occ_version.value());
            TXP_INCREMENT(txp_observe_lock_aborts);
            return false;
        }
        //if (!t()->check_opacity(item(), occ_version.value()))
        //    return false;
        if (add_read && !item.has_read()) {
            VersionDelegate::item_or_flags(item, TransItem::read_bit);
            VersionDelegate::item_access_rdata(item).v = Packer<TLockVersion>::pack(t().buf_, std::move(occ_version));
            VersionDelegate::txn_set_any_nonopaque(t(), true);
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
        auto vv = try_lock_val();
        if (TransactionTid::is_locked_here(vv)) {
            break;
        }

        if (TransactionTid::is_locked_elsewhere(vv)) {
            int owner_id = vv & TransactionTid::threadid_mask;
            if (ContentionManager::should_abort(TThread::id(), owner_id)) {
                TXP_INCREMENT(txp_lock_aborts);
                return false;
            }
        } else {
            TXP_INCREMENT(txp_observe_lock_aborts);
        }

        relax_fence();
    }

    VersionDelegate::item_or_flags(item, TransItem::write_bit | TransItem::lock_bit);
    VersionDelegate::txn_set_any_writes(t(), true);
    //item().__or_flags(TransItem::write_bit | TransItem::lock_bit);
    //t()->any_writes_ = true;
    return ContentionManager::on_write(TThread::id());
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
        auto old_wdata = VersionDelegate::item_access_wdata(item);
        VersionDelegate::item_access_wdata(item) = Packer<T>::repack(t().buf_, old_wdata, std::forward<Args>(args)...);
        //item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);
        return true;
    }

    while(true) {
        auto vv = try_lock_val();
        if (TransactionTid::is_locked_here(vv)) {
            break;
        }

        if (TransactionTid::is_locked_elsewhere(vv)) {
            int owner_id = vv & TransactionTid::threadid_mask;
            if (ContentionManager::should_abort(TThread::id(), owner_id)) {
                TXP_INCREMENT(txp_lock_aborts);
                return false;
            }
        } else {
            TXP_INCREMENT(txp_observe_lock_aborts);
        }

        relax_fence();
    }

    VersionDelegate::item_or_flags(item, TransItem::write_bit | TransItem::lock_bit);
    VersionDelegate::item_access_wdata(item) = Packer<T>::pack(t().buf_, std::forward<Args>(args)...);
    VersionDelegate::txn_set_any_writes(t(), true);
    //item().__or_flags(TransItem::write_bit | TransItem::lock_bit);
    //item().wdata_ = Packer<T>::pack(t()->buf_, std::forward<Args>(args)...);
    //t()->any_writes_ = true;
    return ContentionManager::on_write(TThread::id());
}

template <bool Opaque>
inline bool TSwissVersion<Opaque>::observe_read_impl(TransItem& item, bool add_read) {
    assert(!item.has_stash());
    auto version = *this;
    fence();
    if (version.is_dirty()) {
        t().mark_abort_because(&item, "swiss_read_dirty", version.value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (is_opaque && !t().check_opacity(item, version.value())) {
        return false;
    }
    if (add_read && !item.has_read()) {
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        VersionDelegate::item_access_rdata(item).v = Packer<TSwissVersion<Opaque>>::pack(t().buf_, std::move(version));
        if (!is_opaque)
            VersionDelegate::txn_set_any_nonopaque(t(), true);
        //item().__or_flags(TransItem::read_bit);
        //item().rdata_ = Packer<TSwissVersion<Opaque>>::pack(t()->buf_, std::move(version));
    }
    return true;
}

// TicToc concurrency control, double-word

template <bool Opaque, bool Extend>
inline bool TicTocVersion<Opaque, Extend>::acquire_write_impl(TransItem& item) {
    item.cc_mode_check_tictoc(this, false /* !compressed */);
    if (!item.has_write()) {
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        if (!item.has_read()) {
            TicTocTid::pack_wide(item.wide_read_value(), BV::v_, wts_);
        }
        VersionDelegate::txn_set_any_writes(t(), true);
    }
    return true;
}

template <bool Opaque, bool Extend> template <typename T>
inline bool TicTocVersion<Opaque, Extend>::acquire_write_impl(TransItem& item, const T& wdata) {
    return acquire_write_impl<T, const T&>(item, wdata);
}

template <bool Opaque, bool Extend> template <typename T>
inline bool TicTocVersion<Opaque, Extend>::acquire_write_impl(TransItem& item, T&& wdata) {
    typedef typename std::decay<T>::type V;
    return acquire_write_impl<V, V&&>(item, std::move(wdata));
}

template <bool Opaque, bool Extend> template <typename T, typename... Args>
inline bool TicTocVersion<Opaque, Extend>::acquire_write_impl(TransItem& item, Args&&... args) {
    item.cc_mode_check_tictoc(this, false /* !compressed */);
    if (!item.has_write()) {
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        if (!item.has_read()) {
            TicTocTid::pack_wide(item.wide_read_value(), BV::v_, wts_);
        }
        VersionDelegate::txn_set_any_writes(t(), true);
        VersionDelegate::item_access_wdata(item) = Packer<T>::pack(t().buf_, std::forward<Args>(args)...);
    } else {
        auto old_wdata = VersionDelegate::item_access_wdata(item);
        VersionDelegate::item_access_wdata(item) = Packer<T>::repack(t().buf_, old_wdata, std::forward<Args>(args)...);
    }
    return true;
}

template <bool Opaque, bool Extend>
inline bool TicTocVersion<Opaque, Extend>::observe_read_impl(TransItem& item, bool add_read) {
    item.cc_mode_check_tictoc(this, false /* !compressed */);
    assert(!item.has_stash());
    if (BV::is_locked_elsewhere()) {
        t().mark_abort_because(&item, "locked", BV::value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    //printf("[%d] OBS: R%luW%lu\n", t()->threadid_, version.read_timestamp(), version.write_timestamp());
    if (Opaque) {
        always_assert(false, "opacity not implemented");
        //t()->check_opacity(item(), version);
    }
    if (add_read && !item.has_read()) {
        if (Opaque) {
            always_assert(false, "opacity not implemented 2");
            //t()->min_rts_ = std::min(t()->min_rts_, version.read_timestamp());
        } else {
            VersionDelegate::txn_set_any_nonopaque(t(), true);
        }
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        TicTocTid::pack_wide(item.wide_read_value(), BV::v_, wts_);
        //item().__or_flags(TransItem::observe_bit);
        //item().rdata_ = Packer<TicTocVersion>::pack(t()->buf_, std::move(version));
    }
    return true;
}

template <bool Opaque, bool Extend>
inline bool TicTocVersion<Opaque, Extend>::observe_read_impl(TransItem& item, TicTocVersion<Opaque, Extend>& snapshot) {
    static_assert(!Opaque, "Opacity not implemented.");
    item.cc_mode_check_tictoc(this, false);
    assert(!item.has_stash());
    if (snapshot.is_locked_elsewhere()) {
        t().mark_abort_because(&item, "locked", BV::value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    if (!item.has_read()) {
        VersionDelegate::txn_set_any_nonopaque(t(), true);
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        TicTocTid::pack_wide(item.wide_read_value(), snapshot.v_, snapshot.wts_);
    }
    return true;
}

// TicToc concurrency control, compressed

template <bool Opaque, bool Extend>
inline bool TicTocCompressedVersion<Opaque, Extend>::acquire_write_impl(TransItem& item) {
    item.cc_mode_check_tictoc(this, true /* compressed */);
    if (!item.has_write()) {
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        if (!item.has_read()) {
            item.wide_read_value().v0 = v_;
            acquire_fence();
        }
        VersionDelegate::txn_set_any_writes(t(), true);
    }
    return true;
}

template <bool Opaque, bool Extend> template <typename T>
inline bool TicTocCompressedVersion<Opaque, Extend>::acquire_write_impl(TransItem& item, const T& wdata) {
    return acquire_write_impl<T, const T&>(item, wdata);
}

template <bool Opaque, bool Extend> template <typename T>
inline bool TicTocCompressedVersion<Opaque, Extend>::acquire_write_impl(TransItem& item, T&& wdata) {
    typedef typename std::decay<T>::type V;
    return acquire_write_impl<V, V&&>(item, wdata);
}

template <bool Opaque, bool Extend> template <typename T, typename... Args>
inline bool TicTocCompressedVersion<Opaque, Extend>::acquire_write_impl(TransItem& item, Args&&... args) {
    item.cc_mode_check_tictoc(this, true /* compressed */);
    if (!item.has_write()) {
        VersionDelegate::item_or_flags(item, TransItem::write_bit);
        if (!item.has_read()) {
            item.wide_read_value().v0 = v_;
            acquire_fence();
        }
        VersionDelegate::item_access_wdata(item) = Packer<T>::pack(t().buf_, std::forward<Args>(args)...);
        VersionDelegate::txn_set_any_writes(t(), true);
    } else {
        auto old_wdata = VersionDelegate::item_access_wdata(item);
        VersionDelegate::item_access_wdata(item) = Packer<T>::repack(t().buf_, old_wdata, std::forward<Args>(args)...);
    }
    return true;
};


template <bool Opaque, bool Extend>
inline bool TicTocCompressedVersion<Opaque, Extend>::observe_read_impl(TransItem& item, bool add_read) {
    item.cc_mode_check_tictoc(this, true /* compressed */);
    assert(!item.has_stash());
    if (BV::is_locked_elsewhere()) {
        t().mark_abort_because(&item, "locked", BV::value());
        TXP_INCREMENT(txp_observe_lock_aborts);
        return false;
    }
    //printf("[%d] OBS: R%luW%lu\n", t()->threadid_, version.read_timestamp(), version.write_timestamp());
    if (Opaque) {
        always_assert(false, "opacity not implemented");
        //t()->check_opacity(item(), version);
    }
    if (add_read && !item.has_read()) {
        if (Opaque) {
            always_assert(false, "opacity not implemented 2");
            //t()->min_rts_ = std::min(t()->min_rts_, version.read_timestamp());
        } else {
            VersionDelegate::txn_set_any_nonopaque(t(), true);
        }
        VersionDelegate::item_or_flags(item, TransItem::read_bit);
        acquire_fence();
        item.wide_read_value().v0 = v_;
        //item().__or_flags(TransItem::observe_bit);
        //item().rdata_ = Packer<TicTocVersion>::pack(t()->buf_, std::move(version));
    }
    return true;
}

inline auto TVersion::snapshot(TransProxy& item) -> type {
    type v = value();
    if (!item.observe_opacity(*this)) {
				Transaction::Abort();
		}
    return v;
}

inline auto TVersion::snapshot(const TransItem& item, const Transaction& txn) -> type {
    type v = value();
    if (!const_cast<Transaction&>(txn).check_opacity(const_cast<TransItem&>(item), v)) {
				Transaction::Abort();
		}
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
    (void)txn;
    always_assert(false, "not implemented");
}

template <bool Adaptive>
typename TLockVersion<Adaptive>::type& TLockVersion<Adaptive>::cp_access_tid_impl(Transaction &txn) {
    return VersionDelegate::standard_tid(txn);
}

template <bool Adaptive>
typename TLockVersion<Adaptive>::type TLockVersion<Adaptive>::cp_commit_tid_impl(Transaction &txn) {
    auto tid = cp_access_tid_impl(txn);
    if (tid != 0)
        return tid;
    else
       return TransactionTid::next_unflagged_version(BV::value());
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

template <bool Opaque, bool Extend>
typename TicTocVersion<Opaque, Extend>::type&
TicTocVersion<Opaque, Extend>::cp_access_tid_impl(Transaction& txn) {
    return VersionDelegate::tictoc_tid(txn);
}

template <bool Opaque, bool Extend>
typename TicTocVersion<Opaque, Extend>::type
TicTocVersion<Opaque, Extend>::cp_commit_tid_impl(Transaction& txn) {
    auto tid = cp_access_tid_impl(txn);
    if (tid != 0)
        return tid;
    else
        return txn.compute_tictoc_commit_ts();
}

template <bool Opaque, bool Extend>
typename TicTocCompressedVersion<Opaque, Extend>::type&
TicTocCompressedVersion<Opaque, Extend>::cp_access_tid_impl(Transaction& txn) {
    return VersionDelegate::tictoc_tid(txn);
}

template <bool Opaque, bool Extend>
typename TicTocCompressedVersion<Opaque, Extend>::type
TicTocCompressedVersion<Opaque, Extend>::cp_commit_tid_impl(Transaction& txn) {
    auto tid = cp_access_tid_impl(txn);
    if (tid != 0)
        return tid;
    else
        return txn.compute_tictoc_commit_ts();
}

// Try lock method now also optionally keeps track of commit timestamp

template <typename VersImpl>
inline bool Transaction::try_lock(TransItem& item, VersionBase<VersImpl>& vers) {
    bool locked = false;
#if STO_SORT_WRITESET
    (void) item;
        vers_.cp_try_lock(item, threadid_);
        locked = true;
#else
    // This function will eventually help us track the commit TID when we
    // have no opacity, or for GV7 opacity.
    unsigned n = 0;
    while (true) {
        if (vers.cp_try_lock(item, threadid_)) {
            locked = true;
            break;
        }
        ++n;
# if STO_SPIN_EXPBACKOFF
        if (item.has_read() || n == STO_SPIN_BOUND_WRITE) {
#  if STO_DEBUG_ABORTS
                abort_version_ = vers.value();
#  endif
                locked = false;
                break;
            }
            if (n > 3)
                for (unsigned x = 1 << std::min(15U, n - 2); x; --x)
                    relax_fence();
# else
        if (item.has_read() || n == (1 << STO_SPIN_BOUND_WRITE)) {
#  if STO_DEBUG_ABORTS
            abort_version_ = vers.value();
#  endif
            locked = false;
            break;
        }
# endif
        relax_fence();
    }
#endif
    // start computing tictoc commit ts immediately for writes
    if (locked) {
        if (std::is_base_of<TicTocBase<VersImpl>, VersImpl>::value) {
            vers.compute_commit_ts_step(this->tictoc_tid_, true/* write */);
        } else {
            vers.compute_commit_ts_step(this->commit_tid_, true/* write */);
        }
    }
    return locked;
}

inline Transaction::tid_type Transaction::compute_tictoc_commit_ts() const {
    //assert(state_ == s_committing_locked || state_ == s_committing);
    tid_type commit_ts = 0;
    TransItem* it = nullptr;
    for (unsigned tidx = 0; tidx != tset_size_; ++tidx) {
        it = (tidx % tset_chunk ? it + 1 : tset_[tidx/tset_chunk]);
        if (it->cc_mode() == CCMode::tictoc) {
            bool compressed = it->is_tictoc_compressed();
            tid_type t;
            if (it->has_write()) {
                t = (compressed ? it->tictoc_fetch_ts_origin<TicTocCompressedVersion<>>().read_timestamp()
                                : it->tictoc_fetch_ts_origin<TicTocVersion<>>().read_timestamp())
                    + 1;
            } else {
                t = compressed ? it->tictoc_extract_read_ts<TicTocCompressedVersion<>>().write_timestamp()
                               : it->tictoc_extract_read_ts<TicTocVersion<>>().write_timestamp();
            }
            commit_ts = std::max(commit_ts, t);
        }
    }
    return commit_ts;
}
