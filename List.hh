#pragma once

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

template <typename T, bool Duplicates = false, typename Compare = DefaultCompare<T>, bool Sorted = true>
class List : public Shared {
public:
  List(Compare comp = Compare()) : head_(NULL), listsize_(0), listversion_(0), comp_(comp) {
  }

  typedef uint32_t Version;
  typedef VersionFunctions<Version, 0> ListVersioning;

  static constexpr Version invalid_bit = 1<<0;
  // we need this to protect deletes (could just do a CAS on invalid_bit, but it's unclear
  // how to make that work with the lock, check, install, unlock protocol
  static constexpr Version node_lock_bit = 1<<1;

  static constexpr Version delete_bit = 1<<0;

  static constexpr void* size_key = (void*)0;

  struct list_node {
    list_node(const T& val, list_node *next, bool valid) 
      : val(val), next(next, valid) {}

    void mark_invalid() {
      next.or_flags(invalid_bit);
    }

    void mark_valid() {
      next.set_flags(next.flags() & ~invalid_bit);
    }

    bool is_valid() {
      return !next.has_flags(invalid_bit);
    }

    void lock() {
      next.atomic_add_flags(node_lock_bit);
    }

    void unlock() {
      next.set_flags(next.flags() & ~node_lock_bit);
    }
    
    bool is_locked() {
      return next.has_flags(node_lock_bit);
    }

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

  T* transFind(Transaction& t, const T& elem) {
    auto listv = listversion_;
    fence();
    auto *n = _find(elem);
    if (n) {
      auto item = t_item(t, n);
      if (!validityCheck(n, item)) {
        t.abort();
        return NULL;
      }
      if (has_delete(item)) {
        return NULL;
      }
      item.add_read(0);
    } else {
      // log list v#
      ensureNotFound(t, listv);
      return NULL;
    }
    return &n->val;
  }

  list_node* _find(const T& elem) {
    list_node *cur = head_;
    while (cur != NULL) {
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
      return head_;
    }

    list_node *prev = NULL;
    list_node *cur = head_;
    while (cur != NULL) {
      int c = comp_(cur->val, elem);
      if (!Duplicates && c == 0) {
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
      prev->next = ret;
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

  bool transInsert(Transaction& t, const T& elem) {
    bool inserted;
    auto *node = _insert<true>(elem, &inserted);
    auto item = t_item(t, node);
    if (!inserted) {
      if (!validityCheck(node, item)) {
        t.abort();
        return false;
      }
      // intertransactional insert-then-insert = failed insert
      if (has_insert(item)) {
        return false;
      }
      // delete-then-insert... (should really become an update...)
      if (has_delete(item)) {
        item.set_flags(0);
        // TODO: should really become an update, but for now it's better
        // we just don't do anything
        item.remove_write();
        add_trans_size_offs(t, 1);
        return true;
      }
      // "normal" insert-then-insert = failed insert
      // need to make sure it's still there at commit time
      item.add_read(0);
      return false;
    }
    add_trans_size_offs(t, 1);
    // we lock the list for inserts
    add_lock_list_item(t);
    item.add_write(0);
    item.add_undo();
    return true;
  }

  bool transDelete(Transaction& t, const T& elem) {
    auto listv = listversion_;
    fence();
    auto *n = _find(elem);
    if (n) {
      auto item = t_item(t, n);
      if (!validityCheck(n, item)) {
        t.abort();
        return false;
      }
      if (has_delete(item)) {
        // we're deleting our delete
        return false;
      }
      // insert-then-delete becomes nothing
      if (has_insert(item)) {
        remove(n);
        item.remove_read().remove_write().remove_undo().remove_afterC();
        add_trans_size_offs(t, -1);
        // TODO: should have a count on add_lock_list_item so we can cancel that here
        // still need to make sure no one else inserts something
        ensureNotFound(t, listv);
        return true;
      }
      item.set_flags(delete_bit);
      // mark as a write
      item.add_write(0);
      // we also need to check that it's still valid at commit time (not 
      // bothering with valid_check_only_bit optimization right now)
      item.add_read(0);
      add_lock_list_item(t);
      add_trans_size_offs(t, -1);
      return true;
    } else {
      ensureNotFound(t, listv);
      return false;
    }
  }

  bool remove(const T& elem, bool locked = false) {
    return _remove([&] (list_node *n2) { return comp_(n2->val, elem) == 0; }, locked);
  }

  bool remove(list_node *n, bool locked = false) {
    // TODO: doing this remove means we don't have to value compare, but we also
    // have to go through the whole list (possibly). Unclear which is better.
    return _remove([n] (list_node *n2) { return n == n2; }, locked);
  }

  template <typename FoundFunc>
  bool _remove(FoundFunc found_f, bool locked = false) {
    if (!locked)
      lock(listversion_);
    list_node *prev = NULL;
    list_node *cur = head_;
    while (cur != NULL) {
      if (found_f(cur)) {
        cur->mark_invalid();
        if (prev) {
          prev->next = (list_node*)cur->next;
        } else {
          head_ = cur->next;
        }
        // TODO: rcu free
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

    bool transHasNext(Transaction&) const {
      return !!cur;
    }

    void transReset(Transaction& t) {
      cur = us->head_;
      ensureValid(t);
    }

    T* transNext(Transaction& t) {
      auto ret = cur ? &cur->val : NULL;
      if (cur)
        cur = cur->next;
      ensureValid(t);
      return ret;
    }

    T* transNthNext(Transaction& t, int n) {
      T* ret = NULL;
      while (n > 0 && transHasNext(t)) {
        ret = transNext(t);
        n--;
      }
      if (n == 0)
        return ret;
      return NULL;
    }

private:
    ListIter(List *us, list_node *cur, Transaction& t) : us(us), cur(cur) {
      ensureValid(t);
    }

    ListIter(List *us, list_node *cur) : us(us), cur(cur) {}

    void ensureValid(Transaction& t) {
      while (cur) {
        auto item = us->t_item(t, cur);
        if (!cur->is_valid()) {
          if (!us->has_insert(item)) {
            t.abort();
            // TODO: do we continue in this situation or abort?
            cur = cur->next;
            continue;
          }
        }
        if (us->has_delete(item)) {
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
    return ListIter(this, head_);
  }

  ListIter transIter(Transaction& t) {
    ensureNotFound(t, listversion_);//TODO: rename
    return ListIter(this, head_, t);
  }

  size_t transSize(Transaction& t) {
    ensureNotFound(t, listversion_);
    return listsize_ + trans_size_offs(t);
  }

  size_t size() const {
      return listsize_;
  }

  void clear() {
      while (auto item = head_)
          remove(head_, true);
  }


  void ensureNotFound(Transaction& t, Version readv) {
    auto item = t_item(t, this);
    if (!item.has_read())
      item.add_read(readv);
  }

  void lock(Version& v) {
    ListVersioning::lock(v);
  }

  void unlock(Version& v) {
    ListVersioning::unlock(v);
  }

  bool is_locked(Version& v) {
    return ListVersioning::is_locked(v);
  }

  void lock(TransItem& item) {
    // only lock we need to maybe do is for deletes
    if (has_delete(item))
        item.key<list_node*>()->lock();
    else if (item.key<List*>() == this)
        lock(listversion_);
  }

  void unlock(TransItem& item) {
    if (has_delete(item))
      item.key<list_node*>()->unlock();
    else if (item.key<List*>() == (void*)this)
      unlock(listversion_);
  }

  bool check(TransItem& item, Transaction& t) {
    if (item.key<void*>() == size_key) {
      return true;
    }
    if (item.key<List*>() == this) {
      auto lv = listversion_;
      return 
        ListVersioning::versionCheck(lv, item.template read_value<Version>())
        && (!is_locked(lv) || t.check_for_write(item));
    }
    auto n = item.key<list_node*>();
    return (n->is_valid() || has_insert(item)) && (has_delete(item) || !n->is_locked());
  }

  void install(TransItem& item) {
    if (item.key<List*>() == this)
      return;
    list_node *n = item.key<list_node*>();
    if (has_delete(item)) {
      remove(n, true);
      listsize_--;
      // not super ideal that we have to change version
      // but we need to invalidate transSize() calls
      ListVersioning::inc_version(listversion_);
    } else {
      n->mark_valid();
      listsize_++;
      ListVersioning::inc_version(listversion_);
    }
  }

  void cleanup(TransItem& item, bool committed) {
      if (!committed && item.has_undo()) {
          list_node *n = item.key<list_node*>();
          remove(n);
      }
  }

  bool validityCheck(list_node *n, TransItem& item) {
    return n->is_valid() || has_insert(item);
  }

  template <typename PTR>
  TransProxy t_item(Transaction& t, PTR *node) {
    // can switch this to fresh_item to not read our writes
    return t.item(this, node);
  }
  
  bool has_insert(TransItem& item) {
    return item.has_write() && !has_delete(item);
  }
  
  bool has_delete(TransItem& item) {
    return item.has_flags(delete_bit);
  }

  void add_lock_list_item(Transaction& t) {
    auto item = t_item(t, (void*)this);
    item.add_write(0);
  }

  void add_trans_size_offs(Transaction& t, int size_offs) {
    // TODO: it would be more efficient to store this directly in Transaction,
    // since the "key" is fixed (rather than having to search the transset each time)
    auto item = t_item(t, size_key);
    int cur_offs = 0;
    // XXX: this is sorta ugly
    if (item.has_read()) {
      cur_offs = item.template read_value<int>();
      item.update_read(cur_offs, cur_offs + size_offs);
    } else
      item.add_read(cur_offs + size_offs);
  }

  int trans_size_offs(Transaction& t) {
    auto item = t_item(t, size_key);
    if (item.has_read())
      return item.template read_value<int>();
    return 0;
  }

  list_node *head_;
  long listsize_;
  Version listversion_;
  Compare comp_;
};
