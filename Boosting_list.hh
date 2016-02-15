#pragma once

#include "Boosting.hh"
#include "Boosting_lockkey.hh"

#define STO_NO_STM
#include "List.hh"

template <typename T, bool Duplicates = false, typename Compare = DefaultCompare<T>, bool Sorted = true>
class TransList {
public:
  typedef List<T, Duplicates, Compare, Sorted> inner_list_t;

  TransList(Compare comp = Compare()) : list_(comp), listlock_() {}

  static void _undoInsert(void *self, void *c1, void *c2) {
    T* elem = (T*)c1;
    ((TransList*)self)->list_.template remove<false>(*elem);
    delete elem;
  }

  static void _undoDelete(void *self, void *c1, void *c2) {
    T* elem = (T*)c1;
    ((TransList*)self)->list_.insert(*elem);
    delete elem;
  }

  bool transInsert(const T& elem) {
    transWriteLock(&listlock_);
    bool inserted = list_.insert(elem);
    if (inserted)
      // TODO(nate): we can avoid the alloc here if we use the fact that T is a pair :\
      ON_ABORT(TransList::_undoInsert, this, new T(elem), NULL);
    return inserted;
  }

  T* transFind(const T& elem) {
    transReadLock(&listlock_);
    T* ret = list_.find(elem);
    return ret;
  }

  bool transDelete(const pair_t& elem) {
    transWriteLock(&listlock_);
    bool removed = list_.template remove<false>(elem);
    if (removed)
      ON_ABORT(TransList::_undoDelete, this, new T(elem), NULL);
    return removed;
  }

  typename inner_list_t::ListIter transIter() {
    transReadLock(&listlock_);
    return list_.iter();
  }

  typename inner_list_t::ListIter iter() {
    return list_.iter();
  }

  size_t transSize() {
    transReadLock(&listlock_);
    return list_.size();
  }
  
private:
  inner_list_t list_;
  RWLock listlock_;
};
