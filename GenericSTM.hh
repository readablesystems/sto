#pragma once

#include "Array.hh"
#include "Transaction.hh"

#define SIZE (1<<10)
class GenericSTM : public Shared {
public:
  GenericSTM() : table_() {}

  template <typename T>
  T transRead(T* word) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");

    auto it = Sto::item(this, word);
    if (it.has_write()) {
        return it.template write_value<T>();
    }
    size_t key = bucket(word);
    // ensures version doesn't change
    auto version = table_[key];
    t.check_opacity(version);
    t.item(this, key).add_read(version).assign_flags(uint64_t(3) << TransItem::userf_shift);
    fence();
    T ret = *word;
    return ret;
  }
  
  template <typename T>
  void transWrite(T* word, const T& new_val) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    // just makes the version number change, i.e., makes conflicting reads abort
    // (and locks this word for us)
    Sto::item(this, word).add_write(new_val).assign_flags(sizeof(T) << TransItem::userf_shift);
    Sto::item(this, bucket(word)).add_write(0).assign_flags(uint64_t(3) << TransItem::userf_shift);
  }

  void lock(TransItem& item) {
    if (item.shifted_user_flags() != 3)
      return;
    size_t key = (size_t)item.key<void*>();
    TransactionTid::lock(table_[key]);
  }
  void unlock(TransItem& item) {
    if (item.shifted_user_flags() != 3)
      return;
    size_t key = (size_t)item.key<void*>();
    TransactionTid::unlock(table_[key]);
  }
  bool check(const TransItem& item, const Transaction&) {
    size_t key = (size_t)item.key<void*>();
    return TransactionTid::same_version(table_[key], item.template read_value<uint64_t>())
      && (!TransactionTid::is_locked(table_[key]) || item.has_write());
  }
  void install(TransItem& item, const Transaction& t) {
    if (item.shifted_user_flags() == 3)
      TransactionTid::set_version(table_[(size_t)item.key<void*>()], t.commit_tid());
    else {
      void* word = item.key<void*>();
      // Hashtable implementation has already locked this word for us
      void *data = item.write_value<void*>();
      memcpy(word, &data, item.shifted_user_flags());
    }
  }

private:
  inline size_t hash(void* k) {
    //        std::hash<void*> hash_fn;
        //    return hash_fn(k);
    return ((size_t)k) >> 3;
  }
  
  inline size_t nbuckets() {
    return SIZE;
  }
  inline size_t bucket(void* k) {
    return hash(k) % nbuckets();
  }
  // value is actually unused!
  //Array<int, SIZE/*, SingleElem<int, true>*/> table_;
  uint64_t table_[SIZE];
};
