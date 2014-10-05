#pragma once

#include "Hashtable.hh"
#include "Transaction.hh"

class GenericSTM : public Shared {
public:
  template <typename T>
  T transRead(Transaction& t, T* word) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    void* ret;
    if (!table_.transGet(t, word, ret)) {
      // this should be safe because if someone does do a write of this key,
      // whole transaction will end up aborting
      return *word;
    }
    return *(T*)&ret;
  }

  template <typename T>
  void transWrite(Transaction& t, T* word, const T& new_val) {
    static_assert(sizeof(T) <= sizeof(void*), "don't support words larger than pointer size");
    table_.transPut(t, word, pack(new_val));
    auto& item = t.add_item(this, word);
    // we also add it ourselves because we want to actually change the
    // memory location (not strictly necessary, but probably a good idea
    // if value is ever used outside of transactional context)
    t.add_write(item, new_val);
    item.data.rdata = (void*)sizeof(T);
  }

  // Hashtable handles all of this
  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(TransItem&, Transaction&) { assert(0); return false; }
  void install(TransItem& item) {
    void* word = item.key();

    // Hashtable implementation has already locked this word for us
    void *data = item.template write_value<void*>();
    memcpy(word, &data, (size_t)item.data.rdata);
  }

private:
  Hashtable<void*, void*, 1000000> table_;
};
