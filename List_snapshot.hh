#pragma once

// yihe: experimenting with a snapshot-enabled list (to deal with persistence)

#include "TaggedLow.hh"
#include "Transaction.hh"
#include "VersionFunctions.hh"

template<typename T>
class DefaultCompare {
public:
  int operator()(const T& t1, const T& t2) {
    if (t1 < t2)
      return -1;
    return t2 < t1 ? 1 : 0;
  }
};

template <typename T, bool Duplicates = false, typename Compare = DefaultCompare<T>, bool Sorted = true, bool Opacity = false> class ListIterator;

template <typename T, bool Duplicates = false, typename Compare = DefaultCompare<T>, bool Sorted = true, bool Opacity = false>
class List : public Shared {
  friend class ListIterator<T, Duplicates, Compare, Sorted, Opacity>;
  typedef ListIterator<T, Duplicates, Compare, Sorted, Opacity> iterator;
public:
  List(Compare comp = Compare()) : head_(NULL), listsize_(0), listversion_(0), comp_(comp) {
  }

private:
  typedef TransactionTid::type version_type;

public:
  static constexpr uint8_t invalid_bit = 1<<0;

  static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
  static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;
  static constexpr TransItem::flags_type doupdate_bit = TransItem::user0_bit<<2;

  static constexpr void* size_key = (void*)0;
  static constexpr void* snapshot_key = (void*)1;
  static constexpr uint64_t snapshot_inc_value = 1 << 1;

  struct list_node {
    list_node(const T& val, list_node *next, bool invalid)
        : val(val), next(next, invalid ? invalid_bit : 0), snapshot_state(0) {}

    void mark_invalid() {
      next.or_flags(invalid_bit);
    }

    void mark_valid() {
      next.assign_flags(next.flags() & ~invalid_bit);
    }

    bool is_valid() {
        return !(next.flags() & invalid_bit);
    }
    uint64_t snapshot_state; //LSB: DELETED, special value 0 means pending
    T val;
    TaggedLow<list_node> next;
  };

  bool find(const T& elem, T& val) {
    auto *ret = _find(elem);
    if (ret) {
      val = ret->val;
    }
    return !!ret;
  }

  T* find(const T& elem) {
    auto *ret = _find(elem);
    if (ret) {
      return &ret->val;
    }
    return NULL;
  }

  T* transFind(const T& elem) {
    auto listv = listversion_;
    fence();
    auto *n = _find(elem);
    if (n) {
      auto item = t_item(n);
      if (!validityCheck(n, item)) {
        Sto::abort();
        return NULL;
      }
      if (has_delete(item)) {
        return NULL;
      }
      item.add_read(0);
    } else {
      // log list v#
      verify_list(listv);
      return NULL;
    }
    return &n->val;
  }

  list_node* _find(const T& elem) {
    list_node *cur = head_;
    uint64_t sid = Sto::get_sid();
    while (cur != NULL) {
      if (sid != cur->snapshot_state) {
        // if deleted or snapshot not found, advance to the next node
        if (cur->snapshot_state & 1) {
          cur = cur->next;
          continue;
        }
        // otherwise, retrieve the snapshot and proceed with the traversal
        try {
          list_node& n = Sto::snapshot_item<list_node>(cur, sid);
          cur = &n;
        } catch (SnapshotKeyNotFoundException& e) {
          cur = cur->next;
          continue;
        }
      }
      int c = comp_(cur->val, elem);
      if (c == 0) {
        return cur;
      }
      if (Sorted && c > 0) {
        return NULL;
      }
      cur = cur->next;
    }
    return NULL;
  }

  template <bool Txnal = false>
  list_node* _insert(const T& elem, bool *inserted = NULL) {
    if (inserted)
      *inserted = true;
    lock(listversion_);
    if (!Sorted && !Duplicates) {
      list_node *new_head = new list_node(elem, head_, Txnal);
      head_ = new_head;
      unlock(listversion_);
      return new_head;
    }

    // Does '!Duplicates' imply 'Sorted'?
    list_node *prev = NULL;
    list_node *cur = head_;
    while (cur != NULL) {
      int c = comp_(cur->val, elem);
      if (!Duplicates && c == 0) {
        if (snapshot_is_deleted(cur->snapshot_state)) {
          // save the old snapshot and reuse the deleted slot
          Sto::new_snapshot<list_node>(*cur, cur, cur->snapshot_state & ~(uint64_t)1);
          cur->mark_invalid();
          cur->val = elem;
          cur->snapshot_state = 0;
          // TODO: listsize_++
          unlock(listversion_);
          return cur;
        }
        unlock(listversion_);
        if (inserted)
          *inserted = false;
        return cur;
      } else if (Sorted && c >= 0) {
        break;
      }
      prev = cur;
      cur = cur->next;
    }
    auto ret = new list_node(elem, cur, Txnal);
    if (prev) {
        prev->next.assign_ptr(ret);
    } else {
        head_ = ret;
    }
    if (!Txnal)
      listsize_++;
    unlock(listversion_);
    return ret;
  }

  bool insert(const T& elem) {
    bool inserted;
    _insert<false>(elem, &inserted);
    return inserted;
  }

  bool transInsert(const T& elem) {
    bool inserted;
    auto *node = _insert<true>(elem, &inserted);
    auto item = t_item(node);
    if (!inserted) {
      if (!validityCheck(node, item)) {
        Sto::abort();
        return false;
      }
      // intertransactional insert-then-insert = failed insert
      if (has_insert(item)) {
        return false;
      }
      // delete-then-insert, then insert -- failed insert
      if (has_doupdate(item)) {
        return false;
      }
      // delete-then-insert... (should really become an update...)
      if (has_delete(item)) {
        item.clear_write().add_write(elem);
        item.assign_flags(doupdate_bit);
        add_trans_size_offs(1);
        return true;
      }
      // "normal" insert-then-insert = failed insert
      // need to make sure it's still there at commit time
      item.add_read(0);
      return false;
    }
    add_trans_size_offs(1);
    // we lock the list for inserts
    add_lock_list_item();
    item.add_write(0);
    item.add_flags(insert_bit);
    t_item(snapshot_key).add_write(0);
    return true;
  }

  bool transDelete(const T& elem) {
    auto listv = listversion_;
    fence();
    auto *n = _find(elem);
    if (n) {
      auto item = t_item(n);
      if (!validityCheck(n, item)) {
        Sto::abort();
        return false;
      }
      if (has_delete(item)) {
        // we're deleting our delete
        return false;
      }
      // delete-then-insert, then delete
      if (has_doupdate(item)) {
        // back to deleting
        item.assign_flags(delete_bit);
        add_trans_size_offs(-1);
        return true;
      }
      // insert-then-delete becomes nothing
      if (has_insert(item)) {
        remove<true>(n);
        item.remove_read().remove_write().clear_flags(insert_bit);
        add_trans_size_offs(-1);
        // TODO: should have a count on add_lock_list_item so we can cancel that here
        // still need to make sure no one else inserts something
        verify_list(listv);
        return true;
      }
      item.assign_flags(delete_bit);
      // mark as a write
      item.add_write(0);
      // we also need to check that it's still valid at commit time (not
      // bothering with valid_check_only_bit optimization right now)
      item.add_read(0);
      t_item(snapshot_key).add_write(0);
      add_lock_list_item();
      add_trans_size_offs(-1);
      return true;
    } else {
      verify_list(listv);
      return false;
    }
  }

  template <bool Txnal>
  bool remove(const T& elem, bool locked = false) {
    return _remove<Txnal>([&] (list_node *n2) { return comp_(n2->val, elem) == 0; }, locked);
  }

  template <bool Txnal>
  bool remove(list_node *n, bool locked = false) {
    // TODO: doing this remove means we don't have to value compare, but we also
    // have to go through the whole list (possibly). Unclear which is better.
    return _remove<Txnal>([n] (list_node *n2) { return n == n2; }, locked);
  }

  template <bool Txnal, typename FoundFunc>
  bool _remove(FoundFunc found_f, bool locked = false) {
    if (!locked)
      lock(listversion_);
    list_node *prev = NULL;
    list_node *cur = head_;
    while (cur != NULL) {
      if (found_f(cur)) {
        cur->mark_invalid();
        if (prev) {
            prev->next.assign_ptr(cur->next);
        } else {
            head_ = cur->next;
        }
        if (Txnal) {
          Transaction::rcu_free(cur);
        } else {
          free(cur);
        }
        if (!Txnal)
          listsize_--;
        if (!locked)
          unlock(listversion_);
        return true;
      }
      prev = cur;
      cur = cur->next;
    }
    if (!locked)
      unlock(listversion_);
    return false;
  }

  inline void opacity_check() {
    // When we check for opacity, we need to compare the latest listversion and not
    // the one at the beginning of the operation.
    assert(Opacity);
    auto listv = listversion_;
    fence();
    Sto::check_opacity(listv);
  }
    
  iterator begin() { return iterator(this, head_); }
  iterator end() { return iterator(this, NULL); }

  struct ListIter {
    ListIter() : us(NULL), cur(NULL) {}

    bool valid() const {
      return us != NULL;
    }

    bool hasNext() const {
      return !!cur;
    }

    void reset() {
      cur = us->head_;
    }

    T* next() {
      auto ret = cur ? &cur->val : NULL;
      if (cur)
        cur = cur->next;
      return ret;
    }

    bool transHasNext() const {
      return !!cur;
    }

    void transReset() {
      cur = us->head_;
      ensureValid();
    }

    T* transNext() {
      auto ret = cur ? &cur->val : NULL;
      if (cur)
        cur = cur->next;
      ensureValid();
      return ret;
    }

    T* transNthNext(int n) {
      T* ret = NULL;
      while (n > 0 && transHasNext()) {
        ret = transNext();
        n--;
      }
      if (n == 0)
        return ret;
      return NULL;
    }

private:
    ListIter(List *us, list_node *cur, bool trans) : us(us), cur(cur) {
      if (trans)
        ensureValid();
    }

    void ensureValid() {
      while (cur) {
	// need to check if this item already exists
        auto item = Sto::check_item(us, cur);
        if (!cur->is_valid()) {
          if (!item || !us->has_insert(*item)) {
            Sto::abort();
            // TODO: do we continue in this situation or abort?
            cur = cur->next;
            continue;
          }
        }
        if (item && us->has_delete(*item)) {
          cur = cur->next;
          continue;
        }
        break;
      }
    }

    friend class List;
    List *us;
    list_node *cur;
  };

  ListIter iter() {
    return ListIter(this, head_, false);
  }

  ListIter transIter() {
    verify_list(listversion_);//TODO: rename
    return ListIter(this, head_, true);
  }

  size_t transSize() {
    verify_list(listversion_);
    return listsize_ + trans_size_offs();
  }

  size_t size() const {
      return listsize_;
  }

  void clear() {
      while (auto item = head_)
        remove<false>(head_, true);
  }

  void verify_list(version_type readv) {
      t_item(this).add_read(readv);
      acquire_fence();
  }


  void lock(version_type& v) {
    TransactionTid::lock(v);
  }

  void unlock(version_type& v) {
    TransactionTid::unlock(v);
  }

  bool is_locked(version_type& v) {
    return TransactionTid::is_locked(v);
  }

  void lock(TransItem& item) {
    // this lock is useless given that we also lock the listversion_
    // currently
    // XXX: this isn't great, but I think we need it to update the size...
    if (item.key<List*>() == this)
        lock(listversion_);
    else if (item.key<void*>() == snapshot_key)
        Sto::lock_read_next_sid();
  }

  void unlock(TransItem& item) {
    if (item.key<List*>() == this)
      unlock(listversion_);
    else if (item.key<void*>() == snapshot_key)
      Sto::unlock_read_next_sid();
  }

  bool check(const TransItem& item, const Transaction& t) {
    if (item.key<void*>() == size_key) {
      return true;
    }
    if (item.key<List*>() == this) {
      auto lv = listversion_;
      return
        TransactionTid::same_version(lv, item.template read_value<version_type>())
          && (!is_locked(lv) || item.has_lock(t));
    }
    auto n = item.key<list_node*>();
    return n->is_valid() || has_insert(item);
  }

  bool is_snapshot(list_node* n) {
    assert(!snapshot_is_deleted(n->snapshot_state));
    if (n->snapshot_state < Sto::next_sid())
      return true;
    else
      return false;
  }

  uint64_t get_snapshot_number(uint64_t snapshot_state) {
    return (snapshot_state >> 1);
  }

  void snapshot_mark_deleted(uint64_t& snapshot_state) {
    assert(snapshot_state % 2 == 0);
    snapshot_state |= (uint64_t)1;
    fence();
  }

  bool snapshot_is_deleted(uint64_t snapshot_state) {
    return (snapshot_state & (uint64_t)1);
  }

  void install(TransItem& item, const Transaction& t) {
    if (item.key<List*>() == this)
      return;
    list_node *n = item.key<list_node*>();
    if (has_delete(item)) {
      if (is_snapshot(n)) {
        // Mark node as deleted, but don't actually delete the node
        snapshot_mark_deleted(n->snapshot_state);
      } else {
        remove<true>(n, true);
      }

      // TODO copy-on-write for listsize_
      listsize_--;
      // not super ideal that we have to change version
      // but we need to invalidate transSize() calls
      if (Opacity) {
        TransactionTid::set_version(listversion_, t.commit_tid());
      } else {
        TransactionTid::inc_invalid_version(listversion_);
      }
    } else if (has_doupdate(item)) {
        // XXX BUG
      if (is_snapshot(n)) {
        // create a snapshot copy of previous version of the node (copy-on-write)
        // set snapshot state to next_sid()
        Sto::new_snapshot<list_node>(*n, n, n->snapshot_state);
        n->snapshot_state = Sto::next_sid;
      }
      n->val = item.template write_value<T>();
    } else {
      // insert (install time): set snapshot state to next_sid
      assert(n->snapshot_state == 0);
      n->snapshot_state = Sto::next_sid();
      n->mark_valid();
      listsize_++;
      if (Opacity) {
        TransactionTid::set_version(listversion_, t.commit_tid());
      } else {
        TransactionTid::inc_invalid_version(listversion_);
      }
    }
  }

  void cleanup(TransItem& item, bool committed) {
      if (!committed && (item.flags() & insert_bit)) {
          list_node *n = item.key<list_node*>();
          remove<true>(n);
      }
  }

  bool validityCheck(list_node *n, TransItem& item) {
      return n->is_valid() || (item.flags() & insert_bit);
  }

  template <typename PTR>
  TransProxy t_item(PTR *node) {
    // can switch this to fresh_item to not read our writes
    return Sto::item(this, node);
  }

  bool has_insert(const TransItem& item) {
      return item.has_write() && !has_delete(item) && !has_doupdate(item);
  }

  bool has_delete(const TransItem& item) {
      return item.flags() & delete_bit;
  }

  bool has_doupdate(const TransItem& item) {
      return item.flags() & doupdate_bit;
  }

  void add_lock_list_item() {
    auto item = t_item((void*)this);
    item.add_write(0);
    t_item(snapshot_key).add_write(0);
  }

  void add_trans_size_offs(int size_offs) {
    // TODO: it would be more efficient to store this directly in Transaction,
    // since the "key" is fixed (rather than having to search the transset each time)
    auto item = t_item(size_key);
    int cur_offs = 0;
    // XXX: this is sorta ugly
    if (item.has_read()) {
      cur_offs = item.template read_value<int>();
      item.update_read(cur_offs, cur_offs + size_offs);
    } else
      item.add_read(cur_offs + size_offs);
  }

  int trans_size_offs() {
    auto item = t_item(size_key);
    if (item.has_read())
      return item.template read_value<int>();
    return 0;
  }

  list_node *head_;
  long listsize_;
  version_type listversion_;
  Compare comp_;
};
    
    

    
template <typename T, bool Duplicates, typename Compare, bool Sorted, bool Opacity>
class ListIterator : public std::iterator<std::forward_iterator_tag, T> {
    typedef ListIterator<T, Duplicates, Compare, Sorted, Opacity> iterator;
    typedef List<T, Duplicates, Compare, Sorted, Opacity> list_type;
    typedef typename list_type::list_node list_node;
public:
    ListIterator(list_type * list, list_node* ptr) : myList(list), myPtr(ptr) {
        myList->list_verify(myList->listversion_);
    }
    ListIterator(const ListIterator& itr) : myList(itr.myList), myPtr(itr.myPtr) {}
    
    ListIterator& operator= (const ListIterator& v) {
        myList = v.myList;
        myPtr = v.myPtr;
        return *this;
    }
    
    bool operator==(iterator other) const {
        return (myList == other.myList) && (myPtr == other.myPtr);
    }
    
    bool operator!=(iterator other) const {
        return !(operator==(other));
    }
    
    T& operator*() {
        return myPtr->val; // Just returing the pointer to the value because this
                           // list does not transactionally track updates to list values.
    }
    
    void increment_ptr() {
        if (myPtr)
            myPtr = myPtr->next;
        while (myPtr) {
            // need to check if this item already exists
            auto item = Sto::check_item(myList, myPtr);
            if (!myPtr->is_valid()) {
                if (!item || !myList->has_insert(*item)) {
                    Sto::abort();
                    // TODO: do we continue in this situation or abort?
                    myPtr = myPtr->next;
                    continue;
                }
            }
            if (item && myList->has_delete(*item)) {
                myPtr = myPtr->next;
                continue;
            }
            break;
        }
    }
    
    /* This is the prefix case */
    iterator& operator++() {
        increment_ptr();
        return *this;
    }
    
    /* This is the postfix case */
    iterator operator++(int) {
        iterator clone(*this);
        increment_ptr();
        return clone;
    }
    
private:
    list_type * myList;
    list_node * myPtr;
};


