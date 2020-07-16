#pragma once

#include <type_traits>
#include <cstring>
#include <cassert>
#include "compiler.hh"
#include "Packer.hh"
#include "TThread.hh"
#include "TicTocStructs.hh"

class TWrappedAccess;

class MvAccess;

class TransProxy;

class CicadaHashtable;

template <typename VersImpl>
class VersionBase;

template <typename VersImpl>
class TicTocBase;

enum class CCMode : int {none = 0, opt, lock, tictoc};

class TransItem {
  public:
#if SIZEOF_VOID_P == 8
    typedef uintptr_t ownerstore_type;
    typedef uintptr_t flags_type;
#else
    typedef uint64_t ownerstore_type;
    typedef uint64_t flags_type;
#endif

    static constexpr flags_type write_bit = flags_type(1) << 63;
    static constexpr flags_type read_bit = flags_type(1) << 62;
    static constexpr flags_type lock_bit = flags_type(1) << 61;
    static constexpr flags_type predicate_bit = flags_type(1) << 60;
    static constexpr flags_type stash_bit = flags_type(1) << 59;
    static constexpr flags_type cl_bit = flags_type(1) << 58;
    static constexpr flags_type commute_bit = flags_type(1) << 57;
    static constexpr flags_type mvhistory_bit = flags_type(1) << 56;
    static constexpr flags_type pointer_mask = (flags_type(1) << 48) - 1;
    static constexpr flags_type owner_mask = pointer_mask;
    static constexpr flags_type user0_bit = flags_type(1) << 48;
    static constexpr int userf_shift = 48;
    static constexpr flags_type shifted_userf_mask = 0x7FF;
    static constexpr flags_type special_mask = owner_mask | cl_bit | read_bit | write_bit | lock_bit | predicate_bit | stash_bit | commute_bit | mvhistory_bit;


    TransItem() : s_(), key_(), rdata_(), wdata_(), mode_(CCMode::none) {};
    TransItem(TObject* owner, void* k)
        : s_(reinterpret_cast<ownerstore_type>(owner)), key_(k), rdata_(), wdata_(), mode_(CCMode::none) {
    }

    TObject* owner() const {
        return reinterpret_cast<TObject*>(s_ & pointer_mask);
    }

    bool has_write() const {
        return flags() & write_bit;
    }
    bool has_read() const {
        return flags() & read_bit;
    }
    bool has_predicate() const {
        return flags() & predicate_bit;
    }
    bool has_stash() const {
        return flags() & stash_bit;
    }
    bool has_commute() const {
        return flags() & commute_bit;
    }
    bool has_mvhistory() const {
        return flags() & mvhistory_bit;
    }
    bool needs_unlock() const {
        return flags() & lock_bit;
    }
    bool locked_at_commit() const {
        return flags() & cl_bit;
    }
    bool same_item(const TransItem& x) const {
        return !((s_ ^ x.s_) & owner_mask) && key_ == x.key_;
    }
    bool has_flag(flags_type f) const {
        return flags() & f;
    }

    template <typename T>
    const T& key() const {
        return Packer<T>::unpack(key_);
    }

    template <typename T>
    T& read_value() {
        assert(has_read());
        return Packer<T>::unpack(rdata_.v);
    }
    template <typename T>
    const T& read_value() const {
        assert(has_read());
        return Packer<T>::unpack(rdata_.v);
    }

    // Reserved for accessing TicToc versions in the read set
    const WideTid& wide_read_value() const {
        assert(has_read());
        return rdata_.w;
    }

    WideTid& wide_read_value() {
        return rdata_.w;
    }

    template <typename T>
    T& predicate_value() {
        assert(has_predicate() && !has_read());
        return Packer<T>::unpack(rdata_.v);
    }
    template <typename T>
    const T& predicate_value() const {
        assert(has_predicate() && !has_read());
        return Packer<T>::unpack(rdata_.v);
    }

    template <typename T>
    T& raw_write_value() {
        return Packer<T>::unpack(wdata_);
    }

    template <typename T>
    const T& raw_write_value() const {
        return Packer<T>::unpack(wdata_);
    }

    template <typename T>
    T& write_value() {
        assert(has_write());
        return Packer<T>::unpack(wdata_);
    }
    template <typename T>
    const T& write_value() const {
        assert(has_write());
        return Packer<T>::unpack(wdata_);
    }
    template <typename T>
    T write_value(T default_value) const {
        return has_write() ? Packer<T>::unpack(wdata_) : default_value;
    }

    template <typename T>
    T& xwrite_value() {
        static_assert(Packer<T>::is_simple, "xwrite_value only works on simple types");
        return Packer<T>::unpack(wdata_);
    }
    template <typename T>
    const T& xwrite_value() const {
        static_assert(Packer<T>::is_simple, "xwrite_value only works on simple types");
        return Packer<T>::unpack(wdata_);
    }

    template <typename T>
    T& stash_value() {
        assert(has_stash());
        return Packer<T>::unpack(rdata_.v);
    }
    template <typename T>
    const T& stash_value() const {
        assert(has_stash());
        return Packer<T>::unpack(rdata_.v);
    }
    template <typename T>
    T stash_value(T default_value) const {
        assert(!has_read());
        if (has_stash())
            return Packer<T>::unpack(rdata_.v);
        else
            return std::move(default_value);
    }

    inline bool operator==(const TransItem& x) const {
        return same_item(x);
    }
    inline bool operator<(const TransItem& x) const {
        // we compare keys and THEN shared objects here so that read and write keys with the same value
        // are next to each other
        return key_ < x.key_
            || (key_ == x.key_ && (s_ & owner_mask) < (x.s_ & owner_mask));
    }

    flags_type flags() const {
        return s_;
    }
    flags_type shifted_user_flags() const {
        return (s_ >> userf_shift) & shifted_userf_mask;
    }
    // removes any existing flags, too
    TransItem& assign_flags(flags_type flags) {
        assert(!(flags & special_mask));
        s_ = (s_ & special_mask) | flags;
        return *this;
    }
    TransItem& clear_flags(flags_type flags) {
        assert(!(flags & special_mask));
        s_ = s_ & ~flags;
        return *this;
    }
    // adds to existing flags
    TransItem& add_flags(flags_type flags) {
        assert(!(flags & special_mask));
        s_ = s_ | flags;
        return *this;
    }

    TransItem& clear_needs_unlock() {
        __rm_flags(lock_bit);
        return *this;
    }

    CCMode cc_mode() const {
        return mode_;
    }

    CCMode cc_mode(CCMode observe_mode) {
        if (mode_ == CCMode::none) {
            mode_ = observe_mode;
        }
        return mode_;
    }

    void cc_mode_check_tictoc(void *ts_origin, bool compressed) {
        if (mode_ == CCMode::none) {
            mode_ = CCMode::tictoc;
            if (compressed) {
                ts_origin_ =
                        reinterpret_cast<uintptr_t>(ts_origin) | tictoc_compressed_type_bit;
            } else {
                ts_origin_ = reinterpret_cast<uintptr_t>(ts_origin);
            }
        }
        assert(mode_ == CCMode::tictoc);
        assert(ts_origin_ != 0);
    }

    bool cc_mode_is_optimistic(bool version_optimistic) {
        if (mode_ == CCMode::none)
            mode_ = version_optimistic ? CCMode::opt : CCMode::lock;
        return (mode_ == CCMode::opt);
    }

    template <typename VersImpl>
    VersImpl& tictoc_fetch_ts_origin() {
        return *reinterpret_cast<VersImpl *>(ts_origin_ & ~tictoc_compressed_type_bit);
    }

    template <typename VersImpl>
    VersImpl tictoc_extract_read_ts() const {
        return TicTocBase<VersImpl>::rdata_extract(rdata_);
    }

    bool is_tictoc_compressed() {
        assert(cc_mode() == CCMode::tictoc);
        return (ts_origin_ & tictoc_compressed_type_bit) != 0;
    }

private:
    static constexpr uintptr_t tictoc_compressed_type_bit = 0x1;

    ownerstore_type s_;
    // this word must be unique (to a particular item) and consistently ordered across transactions
    void* key_;
    rdata_t rdata_;
    void* wdata_;
    uintptr_t ts_origin_; // only used by TicToc

    CCMode mode_;

    void __rm_flags(flags_type flags) {
        s_ = s_ & ~flags;
    }
    void __or_flags(flags_type flags) {
        s_ = s_ | flags;
    }

    friend class Transaction;
    friend class TransProxy;
    friend class TWrappedAccess;
    friend class MvAccess;
    friend class VersionDelegate;
    friend class CicadaHashtable;
};

class TransProxy {
  public:
    TransProxy(Transaction& t, TransItem& item)
        : t_(&t), item_(&item) {
        assert(&t == TThread::txn);
    }

    TransProxy* operator->() { // make OptionalTransProxy work
        return this;
    }
    operator TransItem&() {
        return item();
    }

    bool has_read() const {
        return item().has_read();
    }
    template <typename T>
    bool has_read(const T& value) const {
        return has_read() && this->template read_value<T>() == value;
    }
    bool has_predicate() const {
        return item().has_predicate();
    }
    bool has_write() const {
        return item().has_write();
    }
    bool has_stash() const {
        return item().has_stash();
    }
    bool has_commute() const {
        return item().has_commute();
    }
    bool has_mvhistory() const {
        return item().has_mvhistory();
    }
    bool has_flag(TransItem::flags_type f) const {
        return item().flags() & f;
    }
    CCMode cc_mode(CCMode obs_mode) {
        return item().cc_mode(obs_mode);
    }
    bool cc_mode_is_optimistic(bool v_opt) {
        return item().cc_mode_is_optimistic(v_opt);
    }

    template <typename T>
    inline bool add_read(T rdata);
    template <typename T>
    inline bool add_read_opaque(T rdata);

    inline TransProxy& clear_read() {
        item().__rm_flags(TransItem::read_bit);
        return *this;
    }
    template <typename T>
    inline TransProxy& update_read(T old_rdata, T new_rdata);

    inline TransProxy& set_predicate();
    template <typename T>
    inline TransProxy& set_predicate(T pdata);
    inline TransProxy& clear_predicate() {
        item().__rm_flags(TransItem::predicate_bit);
        return *this;
    }

    template <typename T>
    inline TransProxy& add_commute(const T*);
    template <typename T>
    inline TransProxy& add_commute(const T&);
    inline TransProxy& clear_commute() {
        item().__rm_flags(TransItem::commute_bit);
        return *this;
    }

    template <typename T>
    inline TransProxy& add_mvhistory(const T&);
    inline TransProxy& clear_mvhistory() {
        item().__rm_flags(TransItem::mvhistory_bit);
        return *this;
    }

    inline TransProxy& add_write();
    template <typename T>
    inline TransProxy& add_write(const T& wdata);
    template <typename T>
    inline TransProxy& add_write(T&& wdata);
    template <typename T, typename... Args>
    inline TransProxy& add_write(Args&&... wdata);
    inline TransProxy& clear_write() {
        item().__rm_flags(TransItem::write_bit);
        return *this;
    }

    // New interface
    template <typename VersImpl>
    bool observe(VersionBase<VersImpl>& version) {
        return version.observe_read(item(), true);
    }
    template <typename VersImpl>
    bool observe(VersionBase<VersImpl>& version, bool add_read) {
        return version.observe_read(item(), add_read);
    }
    template <typename VersImpl>
    bool observe(VersionBase<VersImpl>& version, VersImpl& snapshot) {
        return version.observe_read(item(), snapshot);
    }
    template <typename VersImpl>
    bool observe_opacity(VersionBase<VersImpl>& version) {
        return version.observe_read(item(), false);
    }

    // like "add_writes"s but also takes a version as argument
    template <typename VersImpl>
    bool acquire_write(VersionBase<VersImpl>& vers) {
        return vers.acquire_write(item());
    }
    template <typename VersImpl, typename T>
    bool acquire_write(VersionBase<VersImpl>& vers, const T& wdata) {
        return vers.acquire_write(item(), wdata);
    };
    template <typename VersImpl, typename T>
    bool acquire_write(VersionBase<VersImpl>& vers, T&& wdata) {
        return vers.acquire_write(item(), wdata);
    };
    template <typename T, typename VersImpl, typename... Args>
    bool acquire_write(VersionBase<VersImpl>& vers, Args&&... args) {
        return vers.template acquire_write<T>(item(), std::forward<Args>(args)...);
    };

    template <typename T>
    inline TransProxy& set_stash(T sdata);
    inline TransProxy& clear_stash() {
        item().__rm_flags(TransItem::stash_bit);
        return *this;
    }

    template <typename T>
    T& read_value() {
        return item().read_value<T>();
    }
    template <typename T>
    const T& read_value() const {
        return item().read_value<T>();
    }

    template <typename T>
    T& predicate_value() {
        return item().predicate_value<T>();
    }
    template <typename T>
    inline T& predicate_value(T default_value);
    template <typename T>
    const T& predicate_value() const {
        return item().predicate_value<T>();
    }

    template <typename T>
    T& raw_write_value() {
        return item().raw_write_value<T>();
    }

    template <typename T>
    const T& raw_write_value() const {
        return item().raw_write_value<T>();
    }

    template <typename T>
    T& write_value() {
        return item().write_value<T>();
    }
    template <typename T>
    const T& write_value(const T& default_value) {
        if (item().has_write())
            return item().write_value<T>();
        else
            return default_value;
    }
    template <typename T>
    const T& write_value() const {
        return item().write_value<T>();
    }

    template <typename T>
    T& xwrite_value() {
        return item().xwrite_value<T>();
    }

    template <typename T>
    T& stash_value() {
        return item().stash_value<T>();
    }
    template <typename T>
    const T& stash_value() const {
        return item().stash_value<T>();
    }
    template <typename T>
    T stash_value(T default_value) const {
        return item().stash_value<T>(std::move(default_value));
    }

    TransProxy& remove_read() { // XXX should also cleanup_read
        item().__rm_flags(TransItem::read_bit);
        return *this;
    }
    TransProxy& remove_write() { // XXX should also cleanup_write
        item().__rm_flags(TransItem::write_bit);
        return *this;
    }

    // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
    TransItem::flags_type flags() const {
        return item().flags();
    }
    TransItem::flags_type shifted_user_flags() const {
        return item().shifted_user_flags();
    }
    TransProxy& assign_flags(TransItem::flags_type flags) {
        item().assign_flags(flags);
        return *this;
    }
    TransProxy& clear_flags(TransItem::flags_type flags) {
        item().clear_flags(flags);
        return *this;
    }
    TransProxy& add_flags(TransItem::flags_type flags) {
        item().add_flags(flags);
        return *this;
    }

    inline Transaction& transaction() const {
        return *t_;
    }
    inline TransItem& item() const {
        return *item_;
    }
    

private:
    Transaction* t_;
    TransItem* item_;
    inline Transaction* t() const {
        return t_;
    }

    template <typename VersImpl>
    friend class VersionBase;

    friend class Transaction;
    friend class OptionalTransProxy;
};


class OptionalTransProxy {
  public:
    typedef TransProxy (OptionalTransProxy::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const {
        return item_ ? &OptionalTransProxy::get : 0;
    }
    TransProxy get() const {
        assert(item_);
        return TransProxy(*t(), *item_);
    }
    TransProxy operator*() const {
        return get();
    }
    TransProxy operator->() const {
        return get();
    }
  private:
    TransItem* item_;
    OptionalTransProxy(Transaction& t, TransItem* item)
        : item_(item) {
        (void) t;
        assert(&t == TThread::txn);
    }
    inline Transaction* t() const {
        return TThread::txn;
    }
    friend class Transaction;
};


inline std::ostream& operator<<(std::ostream& w, const TransItem& item) {
    item.owner()->print(w, item);
    return w;
}
