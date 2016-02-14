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

  bool transInsert(const T& elem) {
    transWriteLock(&listlock_);
    bool inserted = list_.insert(elem);
    return inserted;
  }

  T* transFind(const T& elem) {
    transReadLock(&listlock_);
    T* ret = list_.find(elem);
    return ret;
  }

  bool transDelete(const T& elem) {
    transWriteLock(&listlock_);
    bool removed = list_.template remove<false>(elem);
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
