#pragma once

#include "Boosting_tl2.hh"
#include "Boosting_sto.hh"
#include "Boosting_lockkey.hh"

#include "List.hh"

template <typename T, bool Duplicates = false, typename Compare = DefaultCompare<T>, bool Sorted = true>
class TransList 
#if defined(BOOSTING) && defined(STO)
  : public TransUndoable
#endif
{
public:
  typedef List<T, Duplicates, Compare, Sorted> inner_list_t;

  TransList(Compare comp = Compare()) : list_(comp), listlock_() {}

  static void _undoInsert(void *self, void *c1, void *c2) {
    
    ((TransList*)self)->list_.template remove<false>((T){ c1, c2 });
    //delete elem;
  }

  static void _undoDelete(void *self, void *c1, void *c2) {
    T* elem = (T*)c1;
    ((TransList*)self)->list_.insert(*elem);
    delete elem;
  }

  bool transInsert(const T& elem) {
    // TODO: assuming T is a pair_t (always is in STAMP)
    static_assert(sizeof(T) == 16, "we assume T is pair_t");
    TRANS_WRITE_LOCK(&listlock_);
    bool inserted = list_.insert(elem);
    if (inserted) {
      void *words[2];
      new (words) T(elem);
      ADD_UNDO(TransList::_undoInsert, this, words[0], words[1]);
    }
    return inserted;
  }

  T* transFind(const T& elem) {
    printf("never happens\n");
    TRANS_READ_LOCK(&listlock_);
    T* ret = list_.find(elem);
    return ret;
  }

  bool transDelete(const T& elem) {
    printf("never happens either\n");
    TRANS_WRITE_LOCK(&listlock_);
    bool removed = list_.template remove<false>(elem);
    if (removed) {
      ADD_UNDO(TransList::_undoDelete, this, new T(elem), NULL);
    }
    return removed;
  }

  typename inner_list_t::ListIter transIter() {
    TRANS_READ_LOCK(&listlock_);
    return list_.iter();
  }

  typename inner_list_t::ListIter iter() {
    return list_.iter();
  }

  size_t transSize() {
    TRANS_READ_LOCK(&listlock_);
    return list_.size();
  }
  
private:
  inner_list_t list_;
  RWLock listlock_;
};
