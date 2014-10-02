#include "Hashtable.hh"
#include "Transaction.hh"

template <typename T>
class GenericSTM : public Shared {
public:
  template <typename T2>
  T2 transRead(Transaction& t, T2* word) {
    static_assert(sizeof(T2)==sizeof(T), "should only use words that are same size as templated type");
    T ret;
    if (!table_.transGet(t, word, ret)) {
      // this should be safe because if someone does do a write of this key,
      // whole transaction will end up aborting
      return *word;
    }
    return *(T2*)&ret;
  }

  template <typename T2>
  void transWrite(Transaction& t, T2* word, const T2& new_val) {
    static_assert(sizeof(T2)==sizeof(T), "should only use words that are same size as templated type");
    table_.transPut(t, word, *(T*)&new_val);
    auto& item = t.add_item(this, word);
    // we also add it ourselves because we want to actually change the
    // memory location (not strictly necessary, but probably a good idea
    // if value is ever used outside of transactional context)
    t.add_write(item, new_val);
  }

  // Hashtable handles all of this
  void lock(TransItem&) {}
  void unlock(TransItem&) {}
  bool check(TransItem&, Transaction&) { assert(0); return false; }
  void install(TransItem& item) {
    T* word = (T*)item.key();
    // Hashtable implementation has already locked this word for us
    *word = item.template write_value<T>();
  }

private:
  Hashtable<void*, T> table_;
};
