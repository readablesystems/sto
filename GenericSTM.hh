#pragma once

#include "Transaction.hh"

#define SIZE (1<<15)
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
    // ensure version doesn't change
    auto version = table_[key];
    Sto::check_opacity(version);
    it.add_read(version);
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
  }

  bool own_lock(TransactionTid::type& lock) {
    return TransactionTid::is_locked(lock) && TransactionTid::user_bits(lock) == Transaction::threadid;
  }

  void lock(TransItem& item) {
    size_t key = bucket(item.key<void*>());
    if (!own_lock(table_[key]))
      TransactionTid::lock(table_[key], Transaction::threadid);
  }
  void unlock(TransItem& item) {
    size_t key = bucket(item.key<void*>());
    if (own_lock(table_[key]))
      TransactionTid::unlock(table_[key]);
  }
  bool check(const TransItem& item, const Transaction&) {
    size_t key = bucket(item.key<void*>());
    auto current = table_[key];
    return TransactionTid::same_version(current & ~TransactionTid::user_mask, item.template read_value<uint64_t>() & ~TransactionTid::user_mask)
      && (!TransactionTid::is_locked(current) || item.has_write() || TransactionTid::user_bits(current) == Transaction::threadid);
  }
  void install(TransItem& item, const Transaction& t) {
    void* word = item.key<void*>();
    // need to know when we've unlocked it so we keep threadid in there for now
    TransactionTid::set_version(table_[bucket(word)], TransactionTid::add_user_bits(t.commit_tid(), Transaction::threadid));
    void *data = item.write_value<void*>();
    memcpy(word, &data, item.shifted_user_flags());
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
  uint64_t table_[SIZE];
};
