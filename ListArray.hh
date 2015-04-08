#pragma once
#include "List.hh"

// implememnts abstraction of a transactional array using our linked list
// not particularly fast or useful, except for testing purposes
template <typename T>
class ListArray {
public:
    typedef int key_type;

  T transRead(Transaction& t, int k) {
    KV* kv = list_.transFind(t, KV(k, T()));
    if (kv) {
      return kv->value;
    }
    return T();
  }

  void transWrite(Transaction& t, int k, const T& value) {
    bool inserted = list_.transInsert(t, KV(k, value));
    // will only be useful for very large arrays (in the random insertion case)
    // we can generalize this later by removing an existing key
    assert(inserted);
  }

  bool transDelete(Transaction& t, int k) {
    return list_.transDelete(t, KV(k, T()));
  }

  bool transInsert(Transaction& t, int k, const T& value) {
    return list_.transInsert(t, KV(k, value));
  }

  T read(int k) {
    Transaction t;
    auto ret = transRead(t, k);
    assert(t.try_commit());
    return ret;
  }

private:
  struct KV {
    KV(int key, const T& value) : key(key), value(value) {}
    int key;
    T value;

    bool operator<(const KV& k2) const {
      return key < k2.key;
    }
  };
  List<KV> list_;
};
