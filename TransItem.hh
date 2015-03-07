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

template <bool B> struct packer {};

template <> struct packer<true> {
  template <typename T>
  void* pack(T value) {
    void* x = NULL;
    // yuck. we need to force T's copy constructor to be called at some point so we do this...
    new (&x) T(value);

    return x;
  }

  template <typename T>
  T& unpack(void*& packed) {
    // TODO: this won't work on big endian
    // could use memcpy but then can't return a reference
    return reinterpret_cast<T&>(packed);
  }

  template <typename T>
  void free_packed(void*& packed) { if (!__has_trivial_copy(T)) { this->unpack<T>(packed).T::~T(); } }
};

template <> struct packer<false> {
  template <typename T>
  void* pack(T value) {
    return new T(std::move(value));
  }

  template <typename T>
  T& unpack(void*& packed) {
    return *reinterpret_cast<T*>(packed);
  }

  template <typename T>
  void free_packed(void*& packed) {
    delete reinterpret_cast<T*>(packed);
  }
};


template <typename T>
inline void *pack(T v) {
  // TODO: probably better to use std::is_trivially_copyable or copy_constructible but gcc sucks :(
  return packer<sizeof(T) <= sizeof(void*)>().pack(std::move(v));
}

template <typename T>
inline T& unpack(void *&vp) {
  return packer<sizeof(T) <= sizeof(void*)>().template unpack<T>(vp);
}

template <typename T>
inline void free_packed(void *&vp) {
  packer<sizeof(T) <= sizeof(void*)>().template free_packed<T>(vp);
}

class Shared;
class Transaction;
class TransProxy;

struct TransItem {
  template <typename K>
  TransItem(Shared *s, K k)
      : shared(s), key_(pack(std::move(k))) {
  }

  Shared *sharedObj() const {
    return shared.ptr();
  }

  bool has_write() const {
    return shared.has_flags(WRITER_BIT);
  }
  bool has_read() const {
    return shared.has_flags(READER_BIT);
  }
  bool has_undo() const {
    return shared.has_flags(UNDO_BIT);
  }
  bool has_afterC() const {
    return shared.has_flags(AFTERC_BIT);
  }
  bool same_item(const TransItem& x) const {
    return sharedObj() == x.sharedObj() && key_ == x.key_;
  }

  template <typename T>
  T& write_value() {
    assert(has_write());
    return unpack<T>(wdata_);
  }
  template <typename T>
  T& read_value() {
    assert(has_read());
    return unpack<T>(rdata_);
  }

  template <typename T>
  const T& key() {
    return unpack<T>(key_);
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

  template <typename T>
  TransItem& cleanup_key() {
      free_packed<T>(key_);
      return *this;
  }
  template <typename T>
  TransItem& cleanup_read() {
      if (has_read())
          free_packed<T>(rdata_);
      return *this;
  }
  template <typename T>
  TransItem& cleanup_write() {
      if (has_write())
          free_packed<T>(wdata_);
      return *this;
  }

  // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
  uint8_t flags() {
    return shared.flags() >> 8;
  }
  TransItem& set_flags(uint8_t flags) {
    shared.set_flags(((uint16_t)flags << 8) | (shared.flags() & 0xff));
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
  bool has_flags(uint8_t flags) {
    return shared.has_flags((uint16_t)flags << 8);
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
        return i_;
    }

    bool has_write() const {
        return i_.shared.has_flags(WRITER_BIT);
    }
    bool has_read() const {
        return i_.shared.has_flags(READER_BIT);
    }
    bool has_undo() const {
        return i_.shared.has_flags(UNDO_BIT);
    }
    bool has_afterC() const {
        return i_.shared.has_flags(AFTERC_BIT);
    }

    template <typename T>
    TransProxy& add_read(T rdata) {
        if (!i_.shared.has_flags(READER_BIT)) {
            i_.shared.or_flags(READER_BIT);
            i_.rdata_ = pack(std::move(rdata));
        }
        return *this;
    }
    template <typename T>
    TransProxy& clear_read() {
        if (i_.shared.has_flags(READER_BIT)) {
            free_packed<T>(i_.rdata_);
            i_.shared.rm_flags(READER_BIT);
        }
        return *this;
    }
    template <typename T>
    TransProxy& clear_read(T rdata) {
        if (i_.shared.has_flags(READER_BIT)
            && this->read_value<T>() == rdata) {
            free_packed<T>(i_.rdata_);
            i_.shared.rm_flags(READER_BIT);
        }
        return *this;
    }
    template <typename T, typename U>
    TransProxy& update_read(T old_rdata, U new_rdata) {
        if (i_.shared.has_flags(READER_BIT)
            && this->read_value<T>() == old_rdata) {
            free_packed<T>(i_.rdata_);
            i_.rdata_ = pack(std::move(new_rdata));
        }
        return *this;
    }

    template <typename T>
    inline TransProxy& add_write(T wdata);
    template <typename T>
    TransProxy& clear_write() {
        if (i_.shared.has_flags(WRITER_BIT)) {
            free_packed<T>(i_.wdata_);
            i_.shared.rm_flags(WRITER_BIT);
        }
        return *this;
    }

    TransProxy& add_undo() {
        i_.shared.or_flags(UNDO_BIT);
        return *this;
    }
    TransProxy& add_afterC() {
        i_.shared.or_flags(AFTERC_BIT);
        return *this;
    }

    template <typename T>
    T& read_value() {
        assert(has_read());
        return unpack<T>(i_.rdata_);
    }
    template <typename T>
    T& write_value() {
        assert(has_write());
        return unpack<T>(i_.wdata_);
    }

    TransProxy& remove_write() { // XXX should also cleanup_write
        i_.shared.rm_flags(WRITER_BIT);
        return *this;
    }
    TransProxy& remove_read() { // XXX should also cleanup_read
        i_.shared.rm_flags(READER_BIT);
        return *this;
    }
    TransProxy& remove_undo() {
        i_.shared.rm_flags(UNDO_BIT);
        return *this;
    }
    TransProxy& remove_afterC() {
        i_.shared.rm_flags(AFTERC_BIT);
        return *this;
    }

    // these methods are all for user flags (currently we give them 8 bits, the high 8 of the 16 total flag bits we have)
    uint8_t flags() {
        return i_.shared.flags() >> 8;
    }
    TransProxy& set_flags(uint8_t flags) {
        i_.shared.set_flags(((uint16_t)flags << 8) | (i_.shared.flags() & 0xff));
        return *this;
    }
    TransProxy& rm_flags(uint8_t flags) {
        i_.shared.rm_flags((uint16_t)flags << 8);
        return *this;
    }
    TransProxy& or_flags(uint8_t flags) {
        i_.shared.or_flags((uint16_t)flags << 8);
        return *this;
    }
    bool has_flags(uint8_t flags) {
        return i_.shared.has_flags((uint16_t)flags << 8);
    }

  private:
    Transaction& t_;
    TransItem& i_;
    TransProxy(Transaction& t, TransItem& i)
        : t_(t), i_(i) {
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
        return TransProxy(t_, *i_);
    }
    TransProxy operator->() const {
        return get();
    }
  private:
    Transaction& t_;
    TransItem* i_;
    OptionalTransProxy(Transaction& t, TransItem* i)
        : t_(t), i_(i) {
    }
    friend class Transaction;
};
