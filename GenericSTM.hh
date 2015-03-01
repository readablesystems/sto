#pragma once

#include "Array1.hh"
#include "Transaction.hh"

#define SIZE 10000
class GenericSTM : public Shared {
public:
  
  template <typename T>
  T transRead(Transaction& t, T* word) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    auto it = t.has_item(this, word);
    if (it) {
      auto& item = *it;
      if (item.has_write()) {
        t.check_reads();
        return item.template write_value<T>();
      }
    }
    
    size_t key = bucket(word);
    // ensures version doesn't change
    table_.transRead(t, key);
    t.check_reads();
    return *word;
  }
  
  template <typename T>
  void transWrite(Transaction& t, T* word, const T& new_val) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    // just makes the version number change, i.e., makes conflicting reads abort
    // (and locks this word for us)
    size_t key = bucket(word);
    table_.transWrite(t, key, 0);
    auto& item = t.item(this, word);
    t.add_write(item, new_val);
    item.data.rdata = (void*)sizeof(T);
    t.check_reads();
  }
  
  // Hashtable handles all of this
  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(TransItem&, Transaction&) { assert(0); return false; }
  void install(TransItem& item) {
    void* word = item.key();
    
    // Hashtable implementation has already locked this word for us
    void *data = item.write_value<void*>();
    memcpy(word, &data, (size_t)item.data.rdata);
  }
  
private:
  inline size_t hash(void* k) {
    std::hash<void*> hash_fn;
    return hash_fn(k);
  }
  
  inline size_t nbuckets() {
    return SIZE;
  }
  inline size_t bucket(void* k) {
    return hash(k) % nbuckets();
  }
  // value is actually unused!
  Array1<int, SIZE> table_;
};
