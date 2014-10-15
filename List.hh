#pragma once

#include "TaggedLow.hh"
#include "Transaction.hh"

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
  static constexpr Version invalid_bit = 1<<0;
  // we need this to protect deletes (could just do a CAS on invalid_bit, but it's unclear
  // how to make that work with the lock, check, install, unlock protocol
  static constexpr Version node_lock_bit = 1<<1;

  static constexpr Version delete_bit = 1<<0;

  static constexpr Version list_lock_bit = 1<<(sizeof(Version) - 1);

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
      auto& item = t_item(t, n);
      if (!validityCheck(n, item)) {
        t.abort();
        return NULL;
      }
      if (has_delete(item)) {
        return NULL;
      }
      t.add_read(item, 0);
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
    // TODO: incorrect...
    listsize_++;
    unlock(listversion_);
    return ret;
  }

  bool transInsert(Transaction& t, const T& elem) {
    bool inserted;
    auto *node = _insert<true>(elem, &inserted);
    auto& item = t_item(t, node);
    if (!inserted) {
      if (!validityCheck(node, item)) {
        t.abort();
        return false;
      }
      // intertransactional insert-then-insert = failed insert
      if (has_insert(item)) {
        return false;
      }
      // delete-then-insert...
      if (has_delete(item)) {
        item.set_flags(0);
        return true;
      }
      // "normal" insert-then-insert = failed insert
      return false;
    }
    t.add_write(item, 0);
    t.add_undo(item);
    return true;
  }

  bool transDelete(Transaction& t, const T& elem) {
    auto listv = listversion_;
    fence();
    // read list version
    auto *n = _find(elem);
    if (n) {
      auto& item = t_item(t, n);
      item.set_flags(delete_bit);
      t.add_write(item, 0);
      return true;
    } else {
      ensureNotFound(t, listv);
      return false;
    }
  }

  bool remove(const T& elem) {
    return _remove([&] (list_node *n2) { return comp_(n2->val, elem) == 0; });
  }

  bool remove(list_node *n) {
    return _remove([n] (list_node *n2) { return n == n2; });
  }

  template <typename FoundFunc>
  bool _remove(FoundFunc found_f) {
    list_node *prev = NULL;
    list_node *cur = head_;
    while (cur != NULL) {
      if (found_f(cur)) {
        cur->mark_invalid();
        if (prev) {
          prev->next = cur->next;
        } else {
          head_ = cur->next;
        }
        // TODO: rcu free
        return true;
      }
      prev = cur;
      cur = cur->next;
    }
    return false;
  }

  struct ListIter {
    ListIter() : us(NULL), cur(NULL) {}

    bool valid() const {
      return us != NULL;
    }

    bool transHasNext(Transaction& t) const {
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

  private:
    ListIter(List *us, list_node *cur, Transaction& t) : us(us), cur(cur) {
      ensureValid(t);
    }

    void ensureValid(Transaction& t) {
      while (cur) {
        if (!cur->is_valid()) {
          auto& item = us->t_item(t, cur);
          if (!us->has_insert(item))
            // TODO: do we continue in this situation or abort?
            continue;
        }
        break;
      }
    }

    friend class List;
    List *us;
    list_node *cur;
  };

  ListIter transIter(Transaction& t) {
    ensureNotFound(t, listversion_);//TODO: rename
    return ListIter(this, head_, t);
  }

  size_t transSize(Transaction& t) {
    ensureNotFound(t, listversion_);
    return listsize_;
  }


  void ensureNotFound(Transaction& t, Version readv) {
    auto& item = t_item(t, this);
    if (!item.has_read())
      t.add_read(item, readv);
  }

  void lock(Version& v) {
    while (1) {
      auto cur = v;
      if (!(cur&list_lock_bit) && bool_cmpxchg(&v, cur, cur|list_lock_bit)) {
        break;
      }
      relax_fence();
    }
  }

  void unlock(Version& v) {
    assert(is_locked(v));
    auto cur = v;
    cur &= ~list_lock_bit;
    v = cur;
  }

  bool is_locked(Version& v) {
    return v & list_lock_bit;
  }

  void lock(TransItem& item) {
    // only lock we need to maybe do is for deletes
    if (has_delete(item))
      unpack<list_node*>(item.key())->lock();
  }

  void unlock(TransItem& item) {
    if (has_delete(item))
      unpack<list_node*>(item.key())->lock();
  }

  bool check(TransItem& item, Transaction&) {
    if (item.key() == (void*)this) {
      return listversion_ == item.template read_value<Version>();
    }
    auto n = unpack<list_node*>(item.key());
    return (n->is_valid() || has_insert(item)) && (has_delete(item) || !n->is_locked());
  }

  void install(TransItem& item) {
    list_node *n = unpack<list_node*>(item.key());
    if (has_delete(item)) {
      n->mark_invalid();
      remove(n);
    } else {
      n->mark_valid();
    }
  }

  void undo(TransItem& item) {
    list_node *n = unpack<list_node*>(item.key());
    remove(n);
  }
  
  bool validityCheck(list_node *n, TransItem& item) {
    return n->is_valid() || has_insert(item);
  }

  template <typename PTR>
  TransItem& t_item(Transaction& t, PTR *node) {
    // can switch this to add_item to not read our writes
    return t.item(this, node);
  }
  
  bool has_insert(TransItem& item) {
    return item.has_write() && !has_delete(item);
  }
  
  bool has_delete(TransItem& item) {
    return item.has_flags(delete_bit);
  }

  list_node *head_;
  long listsize_;
  Version listversion_;
  Compare comp_;
};
