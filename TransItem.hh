#pragma once

#include <type_traits>
#include <string.h>
#include <assert.h>

#include "Tagged64.hh"

enum {
  READER_BIT = 1<<0,
  WRITER_BIT = 1<<1,
  UNDO_BIT = 1<<2,
  AFTERC_BIT = 1<<3,
};

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


class Shared;
class Transaction;
class TransProxy;

struct TransItem {
  TransItem(Shared* s, void* k)
      : shared(s), key_(k) {
  }

  Shared *sharedObj() const {
    return shared.ptr();
  }

    bool has_write() const {
        return shared.flags() & WRITER_BIT;
    }
    bool has_read() const {
        return shared.flags() & READER_BIT;
    }
    bool has_undo() const {
        return shared.flags() & UNDO_BIT;
    }
    bool has_afterC() const {
        return shared.flags() & AFTERC_BIT;
    }
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
        assert(has_write());
        return Packer<T>::unpack(wdata_);
    }
    template <typename T>
    const T& write_value() const {
        assert(has_write());
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
    uint8_t flags() {
        return shared.flags() >> 8;
    }
    TransItem& assign_flags(uint8_t flags) {
        shared.assign_flags(((uint16_t)flags << 8) | (shared.flags() & 0xff));
        return *this;
    }
    TransItem& rm_flags(uint8_t flags) {
        shared.rm_flags((uint16_t)flags << 8);
        return *this;
    }
    TransItem& or_flags(uint8_t flags) {
        shared.or_flags((uint16_t)flags << 8);
        return *this;
    }

  private:
    friend class Transaction;
    friend class TransProxy;
    Tagged64<Shared> shared;
    // this word must be unique (to a particular item) and consistently ordered across transactions
    void* key_;
    void* rdata_;
    void* wdata_;
};


class TransProxy {
  public:
    TransProxy* operator->() { // make OptionalTransProxy work
        return this;
    }
    operator TransItem&() {
        return *i_;
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
    bool has_undo() const {
        return i_->has_undo();
    }
    bool has_afterC() const {
        return i_->has_afterC();
    }

    template <typename T>
    inline TransProxy& add_read(T rdata);
    inline TransProxy& clear_read() {
        i_->shared.rm_flags(READER_BIT);
        return *this;
    }
    template <typename T, typename U>
    inline TransProxy& update_read(T old_rdata, U new_rdata);

    template <typename T>
    inline TransProxy& add_write(T wdata);
    inline TransProxy& clear_write() {
        i_->shared.rm_flags(WRITER_BIT);
        return *this;
    }

    TransProxy& add_undo() {
        i_->shared.or_flags(UNDO_BIT);
        return *this;
    }
    TransProxy& add_afterC() {
        i_->shared.or_flags(AFTERC_BIT);
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
        i_->shared.rm_flags(WRITER_BIT);
        return *this;
    }
    TransProxy& remove_read() { // XXX should also cleanup_read
        i_->shared.rm_flags(READER_BIT);
        return *this;
    }
    TransProxy& remove_undo() {
        i_->shared.rm_flags(UNDO_BIT);
        return *this;
    }
    TransProxy& remove_afterC() {
        i_->shared.rm_flags(AFTERC_BIT);
        return *this;
    }

    // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
    uint8_t flags() {
        return i_->flags();
    }
    TransProxy& assign_flags(uint8_t flags) {
        i_->assign_flags(flags);
        return *this;
    }
    TransProxy& rm_flags(uint8_t flags) {
        i_->rm_flags(flags);
        return *this;
    }
    TransProxy& or_flags(uint8_t flags) {
        i_->or_flags(flags);
        return *this;
    }

  private:
    Transaction* t_;
    TransItem* i_;
    TransProxy(Transaction& t, TransItem& i)
        : t_(&t), i_(&i) {
    }
    friend class Transaction;
    friend struct OptionalTransProxy;
};


struct OptionalTransProxy {
  public:
    typedef TransProxy (OptionalTransProxy::*unspecified_bool_type)() const;
    operator unspecified_bool_type() const {
        return i_ ? &OptionalTransProxy::get : 0;
    }
    TransProxy get() const {
        assert(i_);
        return TransProxy(*t_, *i_);
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
