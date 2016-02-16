#pragma once

#include "config.h"
#include "compiler.hh"
#include "Transaction.hh"

template <unsigned ByteSize = 1000000>
class BitTable {
public:
  BitTable() : table_() {}

  template <bool Atomic = false>
  void set(unsigned index) {
    assert(index < size());
    auto byte = index / 8;
    auto bit = 1 << (index % 8);
    auto& byte_ref = table_[byte];
    if (Atomic) {
      while (1) {
        auto cur = byte_ref;
        if (!(cur & bit) && bool_cmpxchg(&byte_ref, cur, cur|bit)) {
          break;
        }
      }
    } else {
      byte_ref |= bit;
    }
  }

  template <bool Atomic = false>
  void unset(unsigned index) {
    assert(index < size());
    auto byte = index / 8;
    auto bit = 1 << (index % 8);
    auto& byte_ref = table_[byte];

    if (Atomic) {
      while (1) {
        auto cur = byte_ref;
        if (bool_cmpxchg(&byte_ref, cur, cur & ~bit)) {
          break;
        }
      }
    } else {
      byte_ref &= ~bit;
    }
  }

  bool get(unsigned index) {
    assert(index < size());
    auto byte = index / 8;
    auto bit = 1 << (index % 8);
    auto& byte_ref = table_[byte];
    return byte_ref & bit;
  }

  size_t size() const {
    return ByteSize*8;
  }
private:

  uint8_t table_[ByteSize];
};


class GenericSTM : public Shared {
public:

  GenericSTM() : table_() {}

  template <typename T>
  T transRead(T* word) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    auto item = Sto::item(this, word);
    if (item.has_write()) {
      Sto::check_reads();
      return item.template write_value<T>();
    }
    T ret = *word;
    if (!item.has_read()) {
      // TODO: this could be a pointer so can we get ABA problems?
      item.add_read(ret);
      item.assign_flags(sizeof(T) << TransItem::userf_shift);
    } else if (item.template read_value<T>() != ret) {
      // this prevents problems where we read a value twice, get different values each time, then end up back at the original val
      Sto::abort();
      return ret;
    }
    t.check_reads();
    return ret;
  }

  template <typename T>
  void transWrite(T* word, const T& new_val) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    auto item = Sto::item(this, word);
    item.add_write(new_val);
    // we need to store sizeof the data type somewhere. 
    // this is 1, 2, 4, or 8, so we can fit it in our 8 flag bits
    item.assign_flags(sizeof(T) << TransItem::userf_shift);
    Sto::check_reads();
  }

  unsigned hash(void *word) const {
    uintptr_t w = (uintptr_t)word;
    return (w >> 4) ^ (w & 0xf);
  }

  // Hashtable handles all of this
  bool lock(TransItem& item) {
      table_.set(hash(item.key<void*>()) % table_.size());
      return true;
  }
  bool check(const TransItem& item, const Transaction& t) {
    size_t sz = item.shifted_user_flags();
    uintptr_t cur = 0, old = 0;
    memcpy(&cur, &item.read_value<void*>(), sz);
    memcpy(&old, item.key<void*>(), sz);
    // TODO: will eventually need to check for false conflicts, too...
    return cur == old
        && (!table_.get(hash(item.key<void*>()) % table_.size()) || item.has_lock(t));
  }
  void install(TransItem& item, const Transaction&) {
      void* word = item.key<void*>();
      void* data = item.write_value<void*>();
      memcpy(word, &data, item.shifted_user_flags());
  }
  void unlock(TransItem& item) {
      table_.unset(hash(item.key<void*>()) % table_.size());
  }

private:
  BitTable<> table_;
};
