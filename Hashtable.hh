#pragma once

#include "config.h"
#include "compiler.hh"
// XXX: honestly hashtable should probably use local_vector too
#include <vector>
#include "Transaction.hh"
#include "simple_str.hh"

#define HASHTABLE_DELETE 1

#ifndef READ_MY_WRITES
#define READ_MY_WRITES 1
#endif 

template <typename K, typename V, bool Opacity = false, unsigned Init_size = 129, typename W = V, typename Hash = std::hash<K>, typename Pred = std::equal_to<K>>
#ifdef STO_NO_STM
class Hashtable {
#else
class Hashtable : public Shared {
#endif
public:
    typedef uint64_t Version;
    typedef K Key;
    typedef K key_type;
    typedef V Value;
    typedef W Value_type;
private:
  // our hashtable is an array of linked lists. 
  // an internal_elem is the node type for these linked lists
  struct internal_elem {
    // nate: I wonder if this would perform better if these had their own
    // cache line.
    Key key;
    internal_elem *next;
    Version version;
#ifndef STO_NO_STM
    // TODO(nate): we should just stuff this in the version so we don't have to
    // deal with this.
    bool valid_;
#endif
    Value_type value;
#ifndef STO_NO_STM
    internal_elem(Key k, Value val) : key(k), next(NULL), version(0), valid_(false), value(val) {}
    bool& valid() {
      return valid_;
    }
#else
    internal_elem(Key k, Value val) : key(k), next(NULL), version(0), value(val) {}
#endif
  };

  struct bucket_entry {
    // nate: we could inline the first element of a bucket. Would probably
    // make resize harder though.
    internal_elem *head;
    // this is the bucket version number, which is incremented on insert
    // we use it to make sure that an unsuccessful key lookup will still be
    // unsuccessful at commit time (because this will always be true if no
    // new inserts have occurred in this bucket)
    Version version;
    bucket_entry() : head(NULL), version(0) {}
  };

  typedef std::vector<bucket_entry> MapType;
  // this is the hashtable itself, an array of bucket_entry's
  MapType map_;
  Hash hasher_;
  Pred pred_;

  // used to mark whether a key is a bucket (for bucket version checks)
  // or a pointer (which will always have the lower 3 bits as 0)
  static constexpr uintptr_t bucket_bit = 1U<<0;

  static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
  static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;
  static constexpr TransItem::flags_type copyvals_bit = TransItem::user0_bit<<2;

public:
  Hashtable(unsigned size = Init_size, Hash h = Hash(), Pred p = Pred()) : map_(), hasher_(h), pred_(p) {
    map_.resize(size);
  }
  
  inline size_t hash(const Key& k) {
    return hasher_(k);
  }

  inline size_t nbuckets() {
    return map_.size();
  }

  inline size_t bucket(const Key& k) {
    return hash(k) % nbuckets();
  }

#ifndef STO_NO_STM
  // returns true if found false if not
  bool transGet(const Key& k, Value& retval) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto item = t_read_only_item(e);
      if (!validity_check(item, e)) {
        Sto::abort();
        return false;
      }
#if READ_MY_WRITES
      // deleted
      if (has_delete(item)) {
        return false;
      }
      if (item.has_write()) {
        if (item.flags() & copyvals_bit)
          retval = item.template write_value<Value>();
        else
          retval = (Value&)item.template write_value<void*>();
        return true;
      }
#endif
      Version elem_vers;
      // "atomic" read of both the current value and the version #
      atomicRead(e, elem_vers, retval);
      // check both node changes and node deletes
      item.add_read(elem_vers);
      if (Opacity)
        check_opacity(e->version);
      return true;
    } else {
      Sto::item(this, pack_bucket(bucket(k))).add_read(TransactionTid::unlocked(buck_version));
      if (Opacity)
        check_opacity(buck.version);
      return false;
    }
  }

#if HASHTABLE_DELETE
  // returns true if successful
  bool transDelete(const Key& k) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      Version elemvers = e->version;
      fence();
      auto item = t_item(e);
      bool valid = e->valid();
#if READ_MY_WRITES
      if (!valid && has_insert(item)) {
        // we're deleting our own insert. special case this to just remove element and just check for no insert at commit
        _remove(e);
        // no way to remove an item (would be pretty inefficient)
        // so we just unmark all attributes so the item is ignored
        item.remove_read().remove_write().clear_flags(insert_bit | delete_bit);
        // insert-then-delete still can only succeed if no one else inserts this node so we add a check for that
        Sto::item(this, pack_bucket(bucket(k))).add_read(TransactionTid::unlocked(buck_version));
        return true;
      } else 
#endif
      if (!valid) {
        Sto::abort();
        return false;
      }
      assert(valid);
#if READ_MY_WRITES      
      // we already deleted!
      if (has_delete(item)) {
        return false;
      }
#endif
      // we need to make sure this bucket didn't change (e.g. what was once there got removed)
      item.add_read(elemvers);
      if (Opacity)
        check_opacity(e->version);
      // we use delete_bit to detect deletes so we don't need any other data
      // for deletes, just to mark it as a write
      if (!item.has_write())
          item.add_write(0).add_flags(copyvals_bit); // XXX is this the right type?
      item.add_flags(delete_bit);
      return true;
    } else {
      // add a read that yes this element doesn't exist
      Sto::item(this, pack_bucket(bucket(k))).add_read(TransactionTid::unlocked(buck_version));
      if (Opacity)
        check_opacity(buck.version);
      return false;
    }
  }
#endif

  // returns true if item already existed, false if it did not
  template <bool CopyVals = true, bool INSERT = true, bool SET = true>
  bool transPut(const Key& k, const Value& v) {
    // TODO: technically puts don't need to look into the table at all until lock time
    bucket_entry& buck = buck_entry(k);
    // TODO: update doesn't need to lock the table
    // also we should lock the head pointer instead so we don't
    // mess with tids
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (e) {
      unlock(&buck.version);
      Version elemvers = e->version;
      fence();
      auto item = t_item(e);
      if (!validity_check(item, e)) {
        Sto::abort();
        // unreachable (t.abort() raises an exception)
        return false;
      }
#if READ_MY_WRITES
      if (has_delete(item)) {
        // delete-then-insert == update (technically v# would get set to 0, but this doesn't matter
        // if user can't read v#)
        if (INSERT) {
          item.clear_flags(delete_bit);
          if (CopyVals)
            item.add_write(v).add_flags(copyvals_bit);
          else
            item.add_write(pack(v));
        } else {
          // delete-then-update == not found
          // delete will check for other deletes so we don't need to re-log that check
        }
        return false;
      }
#endif

#if HASHTABLE_DELETE
      // make sure the item doesn't get deleted before us
      item.add_read(elemvers);
      if (Opacity)
        check_opacity(e->version);
#endif
      if (SET) {
        if (CopyVals)
            item.add_write(v).add_flags(copyvals_bit);
        else
            item.add_write(pack(v));
#if READ_MY_WRITES
        if (has_insert(item)) {
	  // Updating the value here, as we won't update it during install
          e->value = v;
        }
#endif
      }
      return true;
    } else {
      if (!INSERT) {
        auto buck_version = unlock(&buck.version);
        Sto::item(this, pack_bucket(bucket(k))).add_read(TransactionTid::unlocked(buck_version));
        if (Opacity)
            check_opacity(buck.version);
        return false;
      }

      // not there so need to insert
      insert_locked(buck, k, v); // marked as invalid
      auto new_head = buck.head;
      auto new_version = unlock(&buck.version);
      // see if this item was previously read
      auto bucket_item = Sto::check_item(this, pack_bucket(bucket(k)));
      if (bucket_item) {
        bucket_item->update_read(new_version - TransactionTid::increment_value, new_version);
        //} else { could abort transaction now
      }
      // use new_item because we know there are no collisions
      auto item = Sto::new_item(this, new_head);
      // don't actually need to Store anything for the write, just mark as valid on install
      // (for now insert and set will just do the same thing on install, set a value and then mark valid)
      if (CopyVals)
            item.add_write(v).add_flags(copyvals_bit);
      else
            item.add_write(pack(v));
      // need to remove this item if we abort
      item.add_flags(insert_bit);
      return false;
    }
  }

  // returns true if successful
  bool transInsert(const Key& k, const Value& v) {
    return !transPut</*copyvals*/true, /*insert*/true, /*set*/false>(k, v);
  }

  bool transUpdate(const Key& k, const Value& v) {
    return transPut</*copyvals*/true, /*insert*/false, /*set*/true>(k, v);
  }


  bool versionCheck(Version v1, Version v2) {
    return TransactionTid::same_version(v1, v2);
  }

  bool check(const TransItem& item, const Transaction& t) {
    if (is_bucket(item)) {
      bucket_entry& buck = map_[bucket_key(item)];
      return versionCheck(item.template read_value<Version>(), buck.version) && !is_locked(buck.version);
    }
    auto el = item.key<internal_elem*>();
    auto read_version = item.template read_value<Version>();
    // if item has insert_bit then its an insert so no validity check needed.
    // otherwise we check that it is both valid and not locked
    bool validity_check = has_insert(item) || (el->valid() && (!is_locked(el->version) || item.has_lock(t)));
    return validity_check && versionCheck(read_version, el->version);
  }

  bool lock(TransItem& item) {
    assert(!is_bucket(item));
    auto el = item.key<internal_elem*>();
    lock(el);
    return true;
  }
  void install(TransItem& item, const Transaction& t) {
    assert(!is_bucket(item));
    auto el = item.key<internal_elem*>();
    assert(is_locked(el));
    // delete
    if (item.flags() & delete_bit) {
      assert(el->valid());
      el->valid() = false;
      // we wait to remove the node til afterC() (unclear that this is actually necessary)
      return;
    }
    // else must be insert/update
    if (!(item.flags() & insert_bit)) {
      // Update
      Value& new_v = item.flags() & copyvals_bit ? item.template write_value<Value>() : (Value&)item.template write_value<void*>();
      el->value = new_v;
    }
    //if (!__has_trivial_copy(Value)) {
      //Transaction::rcu_cleanup([new_v] () { delete new_v; });
    //}

    if (Opacity)
      TransactionTid::set_version(el->version, t.commit_tid());
    else
      TransactionTid::inc_invalid_version(el->version);
#if 0
    if (has_insert(item)) {
      // need to update bucket version
      bucket_entry& buck = buck_entry(el->key);
      lock(&buck.version);
      if (Opacity)
	TransactionTid::set_version(buck.version, t.commit_tid());
      else
	TransactionTid::inc_invalid_version(buck.version);
      unlock(&buck.version);
    }
#endif
    el->valid() = true;
  }

  void unlock(TransItem& item) {
      assert(!is_bucket(item));
      auto el = item.key<internal_elem*>();
      unlock(el);
  }

  void cleanup(TransItem& item, bool committed) {
      if (committed ? has_delete(item) : has_insert(item)) {
          auto el = item.key<internal_elem*>();
          assert(!el->valid());
          _remove(el);
      }
  }

  // these are wrappers for concurrent.cc and other
  // frameworks we use the hashtable in
  void transWrite(Key k, Value v) {
    transPut(k, v);
  }
  Value transRead(Key k) {
    Value v;
    if (!transGet(k, v)) {
      return Value();
    }
    return v;
  }
  Value read(Key k) {
    TransactionGuard t;
    return transRead(k);
  }
  void insert(Key k, Value v) {
    TransactionGuard t;
    transInsert(k, v);
  }
  void erase(Key ) {}
  void update(Key k, Value v) {
    TransactionGuard t;
    transUpdate(k, v);
  }
  template <typename F>
  void update_fn(Key k, F f) {
    TransactionGuard t;
    transUpdate(k, f(transRead(t, k)));
  }
  template <typename F>
  void upsert(Key k, F f, Value v) {
    TransactionGuard t;
    Value cur;
    if (transGet(k, cur)) {
      transUpdate(k, f(cur));
    } else {
      transInsert(k, v);
    }
  }
  void find(Key k, Value& v) {
    TransactionGuard t;
    transGet(k, v);
  }
  Value find(Key k) {
    return read(k);
  }
  void rehash(unsigned ) {}
  void reserve(unsigned ) {}

#endif /* STO_NO_STM */

  void lock(internal_elem *el) {
    lock(&el->version);
  }

  void lock(const Key& k) {
    lock(&elem(k));
  }
  void unlock(internal_elem *el) {
    unlock(&el->version);
  }

  void unlock(const Key& k) {
    unlock(&elem(k));
  }

  bool is_locked(internal_elem *el) {
    return is_locked(el->version);
  }

  void print_stats() {
    int tot_count = 0;
    int max_chaining = 0;
    int num_empty = 0;

    for (unsigned i = 0; i < map_.size(); ++i) {
      bucket_entry& buck = map_[i];
      if (!buck.head) {
        num_empty++;
        continue;
      }
      int ct = 0;
      internal_elem * list = buck.head;
      while (list) {
        ct++;
        tot_count++;
        list = list->next;
      }

      if (ct > max_chaining) max_chaining = ct;
    }

    printf("Total count: %d, Empty buckets: %d, Avg chaining: %f, Max chaining: %d\n", tot_count, num_empty, ((double)(tot_count))/(map_.size() - num_empty), max_chaining);
  }

  void print() {
    printf("Hashtable:\n");
    for (unsigned i = 0; i < map_.size(); ++i) {
      bucket_entry& buck = map_[i];
      if (!buck.head)
        continue;
      printf("bucket %d (version %d): ", i, buck.version);
      internal_elem *list = buck.head;
      while (list) {
        printf("key: %d, val: %d, version: %d, valid: %d ; ", list->key, list->value, list->version, list->valid());
        list = list->next;
      }
      printf("\n");
    }
  }

  // non-transactional const iteration
  // (we don't have current support for transactional iteration)
  class const_iterator {
  public:
    std::pair<Key, Value> operator*() const {
      return std::make_pair(node->key, node->value);
    }

    const_iterator& operator++() {
      if (node) {
        node = node->next;
      }
      while (!node && bucket != table->map_.size()) {
        node = table->map_[bucket].head;
        bucket++;
      }
      return *this;
    }
    
    bool operator!=(const const_iterator& it) const {
      return node != it.node || bucket != it.bucket;
    }
  private:
    const Hashtable *table;
    int bucket;
    internal_elem *node;
    friend class Hashtable;
  };

  const_iterator begin() const {
    const_iterator begin;
    begin.table = this;
    begin.bucket = -1;
    begin.node = NULL;
    return ++begin; //eh
  }
  const_iterator end() const {
    const_iterator end;
    end.bucket = map_.size();
    end.node = NULL;
    return end;
  }

  // remove given the internal element node. used by transaction system
  void _remove(internal_elem *el) {
    bucket_entry& buck = buck_entry(el->key);
    lock(&buck.version);
    internal_elem *prev = NULL;
    internal_elem *cur = buck.head;
    while (cur != NULL && cur != el) {
      prev = cur;
      cur = cur->next;
    }
    assert(cur);
    if (prev) {
      prev->next = cur->next;
    } else {
      buck.head = cur->next;
    }
    unlock(&buck.version);
    // TODO(nate): we probably need to do a delete of cur->value too to actually free
    // all memory (for non-trivial types)
    Transaction::rcu_free(cur);
  }

  // non-txnal remove given a key
  bool remove(const Key& k) {
    bucket_entry& buck = buck_entry(k);
    lock(&buck.version);
    internal_elem *prev = NULL;
    internal_elem *cur = buck.head;
    while (cur != NULL && !pred_(cur->key, k)) {
      prev = cur;
      cur = cur->next;
    }
    if (!cur) {
      unlock(&buck.version);
      return false;
    }
    if (prev) {
      prev->next = cur->next;
    } else {
      buck.head = cur->next;
    }
    unlock(&buck.version);    
    // TODO(nate): this would probably work fine as-is
    // Transaction::rcu_free(cur);
    return true;
  }

  bool read(const Key& k, Value& retval) {
    auto e = find(buck_entry(k), k);
    if (e) {
      // TODO(nate): this isn't safe for non-trivial types (need an atomic read)
      assign_val(retval, e->value);
    }
    return !!e;
  }

  template <typename ValType>
  static void assign_val(ValType& val, const ValType& val_to_assign) {
    val = val_to_assign;
  }
  static void assign_val(std::string& val, simple_str& val_to_assign) {
    val.assign(val_to_assign.data(), val_to_assign.length());
  }

  Value* readPtr(const Key& k) {
    auto e = find(buck_entry(k), k);
    if (e) {
      return &e->value;
    }
    return NULL;
  }

  // returns pointer to the value in the hashtable 
  // (no current way to distinguish if insert or set)
  Value* putIfAbsentPtr(const Key& k, const Value& val) {
    bucket_entry& buck = buck_entry(k);
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (!e) {
      insert_locked<true>(buck, k, val);
      e = buck.head;
    }
    Value *ret = &e->value;
    unlock(&buck.version);
    return ret;
  }

  // returns true if inserted. otherwise return false and val is set to current value.
  bool putIfAbsent(const Key& k, Value& val) {
    bool exists = false;
    bucket_entry& buck = buck_entry(k);
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (e) {
      assign_val(val, e->value);
      exists = true;
    } else {
      insert_locked<true>(buck, k, val);
      exists = false;
    }
    unlock(&buck.version);
    return exists;
  }

  // returns true if item already existed
  template <bool InsertOnly = false>
  bool put(const Key& k, const Value& val) {
    bool exists = false;
    bucket_entry& buck = buck_entry(k);
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (e) {
      if (!InsertOnly)
        set(e, val);
      exists = true;
    } else {
      insert_locked<true>(buck, k, val);
      exists = false;
    }
    unlock(&buck.version);
    return exists;
  }
  // returns true if successfully inserted
  bool insert(const Key& k, const Value& val) {
    return !put<true>(k, val);
  }

  void set(internal_elem *e, const Value& val) {
    assert(e);
    lock(&e->version);
    e->value = val;
#ifndef STO_NO_STM
    TransactionTid::inc_invalid_version(e->version);
#endif
    unlock(&e->version);
  }
  
private:
  bucket_entry& buck_entry(const Key& k) {
    return map_[bucket(k)];
  }

  // looks up a key's internal_elem, given its bucket
  internal_elem* find(bucket_entry& buck, const Key& k) {
    internal_elem *list = buck.head;
    while (list && ! pred_(list->key, k)) {
      list = list->next;
    }
    return list;
  }

  // looks up a key's internal_elem
  internal_elem* elem(const Key& k) {
    return find(buck_entry(k), k);
  }

  bool has_delete(const TransItem& item) {
      return item.flags() & delete_bit;
  }

  bool has_insert(const TransItem& item) {
      return item.flags() & insert_bit;
  }

  bool validity_check(const TransItem& item, internal_elem *e) {
    return has_insert(item) || e->valid();
  }

  void check_opacity(Version& v) {
    assert(Opacity);
    Version v2 = v;
    fence();
    Sto::check_opacity(v2);
  }

  static bool is_bucket(const TransItem& item) {
      return is_bucket(item.key<void*>());
  }
  static bool is_bucket(void* key) {
      return (uintptr_t)key & bucket_bit;
  }
  static unsigned bucket_key(const TransItem& item) {
      assert(is_bucket(item));
      return (uintptr_t) item.key<void*>() >> 1;
  }
  void* pack_bucket(unsigned bucket) {
      return (void*) ((bucket << 1) | bucket_bit);
  }

  static bool is_locked(Version v) {
    return TransactionTid::is_locked(v);
  }
  static void lock(Version *v) {
    TransactionTid::lock(*v);
  }
  static Version unlock(Version *v) {
    auto ret = TransactionTid::unlocked(*v);
    TransactionTid::unlock(*v);
    return ret;
  }

  template <bool markValid = false>
  void insert_locked(bucket_entry& buck, const Key& k, const Value& val) {
    assert(is_locked(buck.version));
    auto new_head = new internal_elem(k, val);
    internal_elem *cur_head = buck.head;
    new_head->next = cur_head;
    if (markValid) {
#ifndef STO_NO_STM
      new_head->valid() = true;
#endif
    }
    buck.head = new_head;
    // TODO(nate): this means we'll always have to do a hard opacity check on 
    // the bucket version (but I don't think we can get a commit tid yet).
    TransactionTid::inc_invalid_version(buck.version);
  }

  void atomicRead(internal_elem *e, Version& vers, Value& val) {
    Version v2;
    do {
      v2 = e->version;
      if (is_locked(v2))
        Sto::abort();
      fence();
      assign_val(val, e->value);
      fence();
      vers = e->version;
    } while (vers != v2);
  }
  
  template <typename ValType>
  static inline void *pack(const ValType& value) {
    assert(sizeof(ValType) <= sizeof(void*));
    void *placed_val = *(void**)&value;
    return placed_val;
  }
  
  template <typename T>
  TransProxy t_item(T e) {
    return Sto::item(this, e);
  }

  template <typename T>
  TransProxy t_read_only_item(T e) {
#if READ_MY_WRITES
    return Sto::read_item(this, e);
#else
    return Sto::fresh_item(this, e);
#endif
  }
};
