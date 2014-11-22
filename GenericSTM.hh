#pragma once

#include "Hashtable.hh"
#include "Transaction.hh"

class GenericSTM : public Shared {
public:

  template <typename T>
  T transRead(Transaction& t, T* word) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    int unused;
    auto& item = t.item(this, word);
    if (item.has_write()) {
      t.check_reads();
      return item.template write_value<T>();
    }
    // ensures version doesn't change
    table_.transGet(t, word, unused);
    t.check_reads();
    return *word;
  }

  template <typename T>
  void transWrite(Transaction& t, T* word, const T& new_val) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    // just makes the version number change, i.e., makes conflicting reads abort
    // (and locks this word for us)
    table_.transPut(t, word, 0);
    auto& item = t.item(this, word);
    t.add_write(item, new_val);
    item.data.rdata = (void*)sizeof(T);
    t.check_reads();
  }

  // Hashtable handles all of this
  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(TransItem&, Transaction&) { assert(0); return false; }
  void install(TransItem& item, uint64_t tid) {
    void* word = item.key();

    // Hashtable implementation has already locked this word for us
    void *data = item.write_value<void*>();
    memcpy(word, &data, (size_t)item.data.rdata);
  }

private:
  // value is actually unused!
  Hashtable<void*, int, 1000000> table_;
};
