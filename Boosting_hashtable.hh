#pragma once

#include "Boosting.hh"
#include "Boosting_lockkey.hh"

#define STO_NO_STM
#include "Hashtable.hh"

template <typename K, typename V, unsigned Init_size = 129, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
  class TransHashtable {
  Hashtable<K, V, true, Init_size, V, Hash, Pred> hashtable;
  LockKey<K, Init_size, Hash, Pred> lockKey;

public:
  typedef K Key;
  typedef V Value;

  TransHashtable(unsigned size = Init_size, Hash h = Hash(), Pred p = Pred()) : hashtable(size, h, p), lockKey(size, h, p) {}

  bool transGet(const Key& k, Value& retval) {
    lockKey.readLock(k);
    return hashtable.read(k, retval);
  }

  bool remove(const Key& k) {
    return hashtable.remove(k);
  }
  bool insert(const Key& k, const Value& val) {
    return hashtable.insert(k, val);
  }
  bool read(const Key& k, Value& val) {
    return hashtable.read(k, val);
  }

  static void _undoDelete(void *self, void *c1, void *c2) {
    Key key = (Key)c1;
    Value val = (Value)c2;
    bool inserted = ((TransHashtable*)self)->hashtable.insert(key, val);
    assert(inserted);
    // Assume we're always running in STAMP and thus key and value are always POD
    //    delete key;
    //    delete val;
  }
  
  static void _undoInsert(void *self, void *c1, void *c2) {
    Key key = (Key)c1;
    bool success = ((TransHashtable*)self)->hashtable.remove(key);
    assert(success);
    //    delete key;
  }

  bool transDelete(const Key& k) {
    lockKey.writeLock(k);
    Value oldval;
    if (!hashtable.read(k, oldval)) {
      return false;
    }
    bool success = hashtable.remove(k);
    assert(success);

    ON_ABORT(TransHashtable::_undoDelete, this, k, oldval);

    return success;
  }

  bool transInsert(const Key& k, const Value& val) {
    lockKey.writeLock(k);
    bool success = hashtable.insert(k, val);
    
    if (success) {
      ON_ABORT(TransHashtable::_undoInsert, this, k, NULL);
    }

    return success;
  }
};
