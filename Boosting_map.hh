#pragma once

#include "Boosting_tl2.hh"
#include "Boosting_sto.hh"
#include "Boosting_lockkey.hh"

#include "Hashtable.hh"
#include "RBTree.hh"

#define VOIDP(x) ((void*)(uintptr_t)(x))

template <typename K, typename V, unsigned Init_size = 129, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>, 
          typename MapType = Hashtable<K, V, true, Init_size, V, Hash, Pred>>
class TransMap 
#if defined(BOOSTING) && defined(STO)
  : public TransUndoable
#endif
{
public:
  typedef MapType map_type;
private:
  MapType map_;
  LockKey<K, Init_size, Hash, Pred> lockKey_;

public:
  typedef K Key;
  typedef V Value;

  TransMap() : map_(), lockKey_(Init_size, Hash(), Pred()) {}

  TransMap(MapType&& map, unsigned size = Init_size, Hash h = Hash(), Pred p = Pred()) : map_(std::move(map)), lockKey_(size, h, p) {}

  bool transGet(const Key& k, Value& retval) {
    lockKey_.readLock(k);
    return map_.nontrans_find(k, retval);
  }

  bool remove(const Key& k) {
    return map_.nontrans_remove(k);
  }
  bool insert(const Key& k, const Value& val) {
    return map_.nontrans_insert(k, val);
  }
  bool read(const Key& k, Value& val) {
    return map_.nontrans_find(k, val);
  }

  static void _undoDelete(void *self, void *c1, void *c2) {
    static_assert(sizeof(Key) <= 8, "must be trivial and word sized");
    static_assert(sizeof(Value) <= 8, "must be trivial and word sized");
    Key key = (Key)(uintptr_t)c1;
    Value val = (Value)(uintptr_t)c2;
    bool inserted = ((TransMap*)self)->map_.nontrans_insert(key, val);
    assert(inserted);
    // XXX: Assume we're always running in STAMP and thus key and value are always POD
    //    delete key;
    //    delete val;
  }
  
  static void _undoInsert(void *self, void *c1, void *c2) {
    Key key = (Key)(uintptr_t)c1;
    bool success = ((TransMap*)self)->map_.nontrans_remove(key);
    assert(success);
    //    delete key;
  }

  bool transDelete(const Key& k) {
    lockKey_.writeLock(k);
    Value oldval;
    bool success = map_.nontrans_remove(k, oldval);
    if (success) {
      ADD_UNDO(TransMap::_undoDelete, this, VOIDP(k), VOIDP(oldval));
    }

    return success;
  }

  bool transInsert(const Key& k, const Value& val) {
    lockKey_.writeLock(k);
    bool success = map_.nontrans_insert(k, val);
    
    if (success) {
      ADD_UNDO(TransMap::_undoInsert, this, VOIDP(k), NULL);
    }

    return success;
  }

// for concurrent.cc
// probably won't compile with RBTree :(

// barf
#ifdef USE_HASHTABLE

  static void _undoSet(void *self, void *c1, void *c2) {
    Key key = (Key)(uintptr_t)c1;
    Value value = (Value)(uintptr_t)c2;
    bool exists = ((TransMap*)self)->map_.put(key, value);
    assert(exists);
    //    delete key;
  }

  void transPut(const Key& k, const Value& val) {
    lockKey_.writeLock(k);
    Value oldval;
    bool exists = map_.put_getold(k, val, oldval);
    if (exists) {
      ADD_UNDO(TransMap::_undoSet, this, VOIDP(k), VOIDP(oldval));
    } else {
      ADD_UNDO(TransMap::_undoInsert, this, VOIDP(k), NULL);
    }
  }
  bool transUpdate(const Key& k, const Value& val) {
    lockKey_.writeLock(k);
    Value oldval;
    bool there = map_.nontrans_find(k, oldval);
    bool exists = map_.template put<false/*Insert*/, true/*Set*/>(k, val);
    assert(there == exists);
    if (there) {
      ADD_UNDO(TransMap::_undoSet, this, VOIDP(k), VOIDP(oldval));
    }
    return there;
  }
  Value unsafe_get(const Key& k) {
    return map_.unsafe_get(k);
  }
#endif
};
