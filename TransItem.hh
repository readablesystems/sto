#pragma once

#include <type_traits>
#include <string.h>
#include <assert.h>
#include "Interface.hh"
#include "compiler.hh"

// Packer
template <typename T>
struct __attribute__((may_alias)) Aliasable {
    T x __attribute__((may_alias));
};

template <typename T,
          bool simple = (__has_trivial_copy(T) && sizeof(T) <= sizeof(void*))>
  struct Packer {};
template <typename T> struct Packer<T, true> {
    typedef T type;
    typedef int is_simple_type;
    static constexpr bool is_simple = true;
    typedef const T& argument_type;
    typedef T& return_type;
    static void* pack(const T& x) {
        void* v = 0;
        memcpy(&v, &x, sizeof(T));
        return v;
    }
    static T& unpack(void*& x) {
        return *(T*) &x;
    }
    static const T& unpack(void* const& x) {
        return *(const T*) &x;
    }
};
template <typename T> struct Packer<T, false> {
    typedef T type;
    typedef void* is_simple_type;
    static constexpr bool is_simple = false;
    static T& unpack(void* x) {
        return *(T*) x;
    }
};


class TransProxy;

class TransItem {
  public:
#if SIZEOF_VOID_P == 8
    typedef Shared* sharedstore_type;
    typedef uintptr_t flags_type;
#else
    typedef uint64_t sharedstore_type;
    typedef uint64_t flags_type;
#endif

    static constexpr flags_type write_bit = flags_type(1) << 63;
    static constexpr flags_type read_bit = flags_type(1) << 62;
    static constexpr flags_type pred_bit = flags_type(1) << 61;
    static constexpr flags_type pointer_mask = (flags_type(1) << 48) - 1;
    static constexpr flags_type user0_bit = flags_type(1) << 48;
    static constexpr int userf_shift = 48;
    static constexpr flags_type shifted_userf_mask = 0x3FFF;
    static constexpr flags_type special_mask = pointer_mask | read_bit | write_bit | pred_bit;


    TransItem(Shared* s, void* k)
        : s_(reinterpret_cast<sharedstore_type>(s)), key_(k) {
    }

    Shared* sharedObj() const {
        return reinterpret_cast<Shared*>(reinterpret_cast<flags_type>(s_) & pointer_mask);
    }

    bool has_write() const {
        return flags() & write_bit;
    }
    bool has_read() const {
        return flags() & read_bit;
    }
    bool has_predicate() const {
        return flags() & pred_bit;
    }
    bool has_lock(const Transaction& t) const;
    bool same_item(const TransItem& x) const {
        return sharedObj() == x.sharedObj() && key_ == x.key_;
    }

    template <typename T>
    const T& key() const {
        return Packer<T>::unpack(key_);
    }

    template <typename T>
    T& read_value() {
        assert(has_read());
        return Packer<T>::unpack(rdata_);
    }
    template <typename T>
    const T& read_value() const {
        assert(has_read());
        return Packer<T>::unpack(rdata_);
    }
    template <typename T>
    T& write_value() {
        assert(has_write() || has_predicate());
        return Packer<T>::unpack(wdata_);
    }
    template <typename T>
    const T& write_value() const {
        assert(has_write() || has_predicate());
        return Packer<T>::unpack(wdata_);
    }

    inline bool operator==(const TransItem& t2) const {
        return key_ == t2.key_ && sharedObj() == t2.sharedObj();
    }
    inline bool operator<(const TransItem& t2) const {
        // we compare keys and THEN shared objects here so that read and write keys with the same value
        // are next to each other
        return key_ < t2.key_
                      || (key_ == t2.key_ && sharedObj() < t2.sharedObj());
    }

    // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
    flags_type flags() const {
        return reinterpret_cast<flags_type>(s_);
    }
    flags_type shifted_user_flags() const {
        return (flags() >> userf_shift) & shifted_userf_mask;
    }
    // removes any existing flags, too
    TransItem& assign_flags(flags_type flags) {
        assert(!(flags & special_mask));
        s_ = reinterpret_cast<sharedstore_type>((reinterpret_cast<flags_type>(s_) & special_mask) | flags);
        return *this;
    }
    TransItem& clear_flags(flags_type flags) {
        assert(!(flags & special_mask));
        s_ = reinterpret_cast<sharedstore_type>(reinterpret_cast<flags_type>(s_) & ~flags);
        return *this;
    }
    // adds to existing flags
    TransItem& add_flags(flags_type flags) {
        assert(!(flags & special_mask));
        s_ = reinterpret_cast<sharedstore_type>(reinterpret_cast<flags_type>(s_) | flags);
        return *this;
    }
    
    template <typename T>
    inline void add_read_version(T version, Transaction& t);
    
  private:
    friend class Transaction;
    friend class TransProxy;
    Shared* s_;
    // this word must be unique (to a particular item) and consistently ordered across transactions
    void* key_;
    void* rdata_;
    void* wdata_;

    void __rm_flags(flags_type flags) {
        s_ = reinterpret_cast<sharedstore_type>(reinterpret_cast<flags_type>(s_) & ~flags);
    }
    void __or_flags(flags_type flags) {
        s_ = reinterpret_cast<sharedstore_type>(reinterpret_cast<flags_type>(s_) | flags);
    }
};


class TransProxy {
  public:
    TransProxy* operator->() { // make OptionalTransProxy work
        return this;
    }
    operator TransItem&() {
        return *i_;
    }

    bool has_predicate() const {
        return i_->has_predicate();
    }
    template <typename T>
    bool has_predicate(const T& value) const {
        return has_predicate() && this->template read_value<T>() == value;
    }
    bool has_read() const {
        return i_->has_read();
    }
    template <typename T>
    bool has_read(const T& value) const {
        return has_read() && this->template read_value<T>() == value;
    }
    bool has_write() const {
        return i_->has_write();
    }
    bool has_lock() const;

    template <typename T>
    inline TransProxy& add_read(T rdata);
    inline TransProxy& clear_read() {
        i_->__rm_flags(TransItem::read_bit);
        return *this;
    }
    template <typename T, typename U>
    inline TransProxy& update_read(T old_rdata, U new_rdata);

    template <typename T>
    inline TransProxy& add_predicate(T rdata);
    
    template <typename T>
    inline TransProxy& add_write(T wdata);
    inline TransProxy& clear_write() {
        i_->__rm_flags(TransItem::write_bit);
        return *this;
    }

    template <typename T>
    T& read_value() {
        return i_->read_value<T>();
    }
    template <typename T>
    const T& read_value() const {
        return i_->read_value<T>();
    }
    template <typename T>
    T& write_value() {
        return i_->write_value<T>();
    }
    template <typename T>
    const T& write_value() const {
        return i_->write_value<T>();
    }

    TransProxy& remove_write() { // XXX should also cleanup_write
        i_->__rm_flags(TransItem::write_bit);
        return *this;
    }
    TransProxy& remove_read() { // XXX should also cleanup_read
        i_->__rm_flags(TransItem::read_bit);
        return *this;
    }

    // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
    TransItem::flags_type flags() const {
        return i_->flags();
    }
    TransItem::flags_type shifted_user_flags() const {
        return i_->shifted_user_flags();
    }
    TransProxy& assign_flags(TransItem::flags_type flags) {
        i_->assign_flags(flags);
        return *this;
    }
    TransProxy& clear_flags(TransItem::flags_type flags) {
        i_->clear_flags(flags);
        return *this;
    }
    TransProxy& add_flags(TransItem::flags_type flags) {
        i_->add_flags(flags);
        return *this;
    }

  private:
    Transaction* t_;
    TransItem* i_;
    TransProxy(Transaction& t, TransItem& i)
        : t_(&t), i_(&i) {
    }
    friend class Transaction;
    friend class OptionalTransProxy;
};


class OptionalTransProxy {
  public:
    typedef TransProxy (OptionalTransProxy::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const {
        return i_ ? &OptionalTransProxy::get : 0;
    }
    TransProxy get() const {
        assert(i_);
        return TransProxy(*t_, *i_);
    }
    TransProxy operator*() const {
        return get();
    }
    TransProxy operator->() const {
        return get();
    }
  private:
    Transaction* t_;
    TransItem* i_;
    OptionalTransProxy(Transaction& t, TransItem* i)
        : t_(&t), i_(i) {
    }
    friend class Transaction;
};
