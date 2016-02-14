#pragma once

#include "Hashtable.hh"
#include "Transaction.hh"

class GenericSTM : public Shared {
public:

  template <typename T>
  T transRead(Transaction& t, T* word) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    int unused;
    auto item = t.item(this, word);
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
    t.item(this, word).add_write(new_val).set_flags((int) sizeof(T));
    t.check_reads();
  }

  // Hashtable handles all of this
  bool lock(TransItem&) { return true; }
  bool check(const TransItem&, const Transaction&) { assert(0); return false; }
  void install(TransItem& item, const Transaction&) {
    void* word = item.key();

    // Hashtable implementation has already locked this word for us
    void *data = item.write_value<void*>();
    memcpy(word, &data, item.flags());
  }

private:
  // value is actually unused!
  Hashtable<void*, int, 1000000> table_;
};
