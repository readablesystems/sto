#pragma once

#include "config.h"
#include "compiler.hh"
#include "Transaction.hh"

#define HASHTABLE_DELETE 1

template <typename K, typename V, unsigned Init_size = 129, typename Hash = std::hash<K>>
class Hashtable : public Shared {
public:
    typedef unsigned Version;
    typedef K Key;
    typedef K key_type;
    typedef V Value;

private:
  // our hashtable is an array of linked lists. 
  // an internal_elem is the node type for these linked lists
  struct internal_elem {
    Key key;
    internal_elem *next;
    Version version;
    bool valid_;
    Value value;
    internal_elem(Key k, Value val) : key(k), next(NULL), version(0), valid_(false), value(val) {}
    bool& valid() {
      return valid_;
    }
  };

  struct bucket_entry {
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

public:
  static constexpr Version lock_bit = 1U<<(sizeof(Version)*8 - 1);
  // if set we check only node validity, not the node's version number
  // (for deletes we need to make sure the node is still there/valid, but 
  // not that its value is the same)
  static constexpr Version valid_check_only_bit = 1U<<(sizeof(Version)*8 - 2);
  static constexpr Version version_mask = ~(lock_bit|valid_check_only_bit);
  // used to mark whether a key is a bucket (for bucket version checks)
  // or a pointer (which will always have the lower 3 bits as 0)
  static constexpr uintptr_t bucket_bit = 1U<<0;

  Hashtable(unsigned size = Init_size, Hash h = Hash()) : map_(), hasher_(h) {
    map_.resize(size);
  }
  
  inline size_t hash(Key k) {
    return hasher_(k);
  }

  inline size_t nbuckets() {
    return map_.size();
  }

  inline size_t bucket(Key k) {
    return hash(k) % nbuckets();
  }

  // returns true if found false if not
  bool transGet(Transaction& t, Key k, Value& retval) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto item = t.item(this, e);
      if (!validity_check(item, e)) {
        t.abort();
        return false;
      }
      // deleted
      if (has_delete(item)) {
        return false;
      }
      if (item.has_write()) {
        retval = item.template write_value<Value>();
        return true;
      }
      Version elem_vers;
      // "atomic" read of both the current value and the version #
      atomicRead(e, elem_vers, retval);
      // check both node changes and node deletes
      if (!item.has_read() || item.template read_value<Version>() & valid_check_only_bit) {
        item.add_read(elem_vers);
      }
      return true;
    } else {
      auto item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read()) {
        item.add_read(buck_version);
      }
      return false;
    }
  }

#if HASHTABLE_DELETE
  // returns true if successful
  bool transDelete(Transaction& t, Key k) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto item = t.item(this, e);
      bool valid = e->valid();
      if (!valid && has_insert(item)) {
        // we're deleting our own insert. special case this to just remove element and just check for no insert at commit
        remove(e);
        // no way to remove an item (would be pretty inefficient)
        // so we just unmark all attributes so the item is ignored
        item.remove_read().remove_write().remove_undo().remove_afterC();
        // insert-then-delete still can only succeed if no one else inserts this node so we add a check for that
        t.item(this, pack_bucket(bucket(k))).add_read(buck_version);
        return true;
      } else if (!valid) {
        t.abort();
        return false;
      }
      assert(valid);
      // we already deleted!
      if (has_delete(item)) {
        return false;
      }
      // we need to make sure this bucket didn't change (e.g. what was once there got removed)
      if (!item.has_read()) {
        // we only need to check validity, not presence
        item.add_read(valid_check_only_bit);
      }
      // we use has_afterC() to detect deletes so we don't need any other data
      // for deletes, just to mark it as a write
      if (!item.has_write())
        item.add_write(0);
      item.add_afterC();
      return true;
    } else {
      // add a read that yes this element doesn't exist
      auto item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read())
        item.add_read(buck_version);
      return false;
    }
  }
#endif

  // returns true if item already existed, false if it did not
  template <bool INSERT = true, bool SET = true>
  bool transPut(Transaction& t, Key k, const Value& v) {
    // TODO: technically puts don't need to look into the table at all until lock time
    bucket_entry& buck = buck_entry(k);
    // TODO: update doesn't need to lock the table
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (e) {
      unlock(&buck.version);
      auto item = t.item(this, e);
      if (!validity_check(item, e)) {
        t.abort();
        // unreachable (t.abort() raises an exception)
        return false;
      }

      if (has_delete(item)) {
        // delete-then-insert == update (technically v# would get set to 0, but this doesn't matter
        // if user can't read v#)
        if (INSERT) {
          item.remove_afterC();
          item.add_write(v);
        } else {
          // delete-then-update == not found
          // delete will check for other deletes so we don't need to re-log that check
        }
        return false;
      }

#if HASHTABLE_DELETE
      // we need to make sure this item stays here
      if (!item.has_read())
        // we only need to check validity, not presence
        item.add_read(valid_check_only_bit);
#endif
      if (SET) {
        item.add_write(v);
      }
      return true;
    } else {
      if (!INSERT) {
        auto buck_version = buck.version;
        unlock(&buck.version);
        auto item = t.item(this, pack_bucket(bucket(k)));
        if (!item.has_read()) {
          item.add_read(buck_version);
        }
        return false;
      }

      // not there so need to insert
      insert_locked(buck, k, v); // marked as invalid
      auto new_version = buck.version;
      auto new_head = buck.head;
      unlock(&buck.version);
      // see if this item was previously read
      auto bucket_item = t.check_item(this, pack_bucket(bucket(k)));
      if (bucket_item) {
        if (bucket_item->has_read() && 
            versionCheck(bucket_item->template read_value<Version>(), new_version - 1)) {
          // looks like we're the only ones to have updated the version number, so update read's version number
          // to still be valid
          bucket_item->add_read(new_version);
        }
        //} else { could abort transaction now
      }
      // use new_item because we know there are no collisions
      auto item = t.new_item(this, new_head);
      // don't actually need to store anything for the write, just mark as valid on install
      // (for now insert and set will just do the same thing on install, set a value and then mark valid)
      item.add_write(v);
      // need to remove this item if we abort
      item.add_undo();
      return false;
    }
  }

  // returns true if successful
  bool transInsert(Transaction& t, Key k, const Value& v) {
    return !transPut</*insert*/true, /*set*/false>(t, k, v);
  }

  // aka putIfAbsent (returns true if successful)
  bool transUpdate(Transaction& t, Key k, const Value& v) {
    return transPut</*insert*/false, /*set*/true>(t, k, v);
  }

  void lock(internal_elem *el) {
    lock(&el->version);
  }

  void lock(Key k) {
    lock(&elem(k));
  }
  
  void unlock(internal_elem *el) {
    Version cur = el->version;
    assert(cur & lock_bit);
    cur &= ~lock_bit;
    el->version = cur;
  }

  void unlock(Key k) {
    unlock(&elem(k));
  }

  bool is_locked(internal_elem *el) {
    return is_locked(el->version);
  }

  bool versionCheck(Version v1, Version v2) {
    return ((v1 ^ v2) & version_mask) == 0;
  }

  bool check(TransItem& item, Transaction& t) {
    if (is_bucket(item.key())) {
      bucket_entry& buck = map_[bucket_value(item.key())];
      return versionCheck(item.template read_value<Version>(), buck.version) && !is_locked(buck.version);
    }
    auto el = unpack<internal_elem*>(item.key());
    auto read_version = item.template read_value<Version>();
    // if item has undo then its an insert so no validity check needed.
    // otherwise we check that it is both valid and not locked
    bool validity_check = item.has_undo() || (el->valid() && (!is_locked(el->version) || t.check_for_write(item)));
    return validity_check && ((read_version & valid_check_only_bit) ||
                              versionCheck(read_version, el->version));
  }

  void lock(TransItem& item) {
    assert(!is_bucket(item.key()));
    auto el = unpack<internal_elem*>(item.key());
    lock(el);
  }
  void unlock(TransItem& item) {
    assert(!is_bucket(item.key()));
    auto el = unpack<internal_elem*>(item.key());
    unlock(el);
  }
  void install(TransItem& item) {
    assert(!is_bucket(item.key()));
    auto el = unpack<internal_elem*>(item.key());
    assert(is_locked(el));
    // delete
    if (item.has_afterC()) {
      assert(el->valid());
      el->valid() = false;
      // we wait to remove the node til afterC() (unclear that this is actually necessary)
      return;
    }
    // else must be insert/update
    el->value = std::move(item.template write_value<Value>());
    inc_version(el->version);
    el->valid() = true;
  }

  void cleanup(TransItem& item, bool committed) {
    if (committed ? item.has_afterC() : item.has_undo()) {
        auto el = unpack<internal_elem*>(item.key());
        assert(!el->valid());
        remove(el);
    }

    free_packed<internal_elem*>(item.key());
    item.cleanup_read<Version>().cleanup_write<Value>();
  }

  void remove(internal_elem *el) {
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
    Transaction::cleanup([cur] () { free(cur); });
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

  // non-transactional get/put/etc.
  // not very interesting
  bool read(Key k, Value& retval) {
    auto e = elem(k);
    if (e)
      retval = e->value;
    return !!e;
  }

  void put(Key k, Value val) {
    bucket_entry& buck = buck_entry(k);
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (e) {
      set(e, val);
    } else {
      insert_locked<true>(buck, k, val);
    }
    unlock(&buck.version);
  }

  void set(internal_elem *e, Value val) {
    assert(e);
    lock(&e->version);
    e->value = val;
    inc_version(e->version);
    unlock(&e->version);
  }

  // these are wrappers for concurrent.cc
  void transWrite(Transaction& t, Key k, Value v) {
    transPut(t, k, v);
  }
  Value transRead(Transaction& t, Key k) {
    Value v;
    if (!transGet(t, k, v)) {
      return Value();
    }
    return v;
  }
  Value transRead_nocheck(Transaction& , Key ) { return Value(); }
  void transWrite_nocheck(Transaction& , Key , Value ) {}
  Value read(Key k) {
    Transaction t;
    return transRead(t, k);
  }
  void insert(Key k, Value v) {
    Transaction t;
    transInsert(t, k, v);
    t.commit();
  }
  void erase(Key ) {}
  void update(Key k, Value v) {
    Transaction t;
    transUpdate(t, k, v);
    t.commit();
  }
  template <typename F>
  void update_fn(Key k, F f) {
    Transaction t;
    transUpdate(t, k, f(transRead(t, k)));
    t.commit();
  }
  template <typename F>
  void upsert(Key k, F f, Value v) {
    Transaction t;
    Value cur;
    if (transGet(t, k, cur)) {
      transUpdate(t, k, f(cur));
    } else {
      transInsert(t, k, v);
    }
    t.commit();
  }
  void find(Key k, Value& v) {
    Transaction t;
    transGet(t, k, v);
  }
  Value find(Key k) {
    return read(k);
  }
  void rehash(unsigned ) {}
  void reserve(unsigned ) {}
  
private:
  bucket_entry& buck_entry(Key k) {
    return map_[bucket(k)];
  }

  // looks up a key's internal_elem, given its bucket
  internal_elem* find(bucket_entry& buck, Key k) {
    internal_elem *list = buck.head;
    while (list && !(list->key == k)) {
      list = list->next;
    }
    return list;
  }

  // looks up a key's internal_elem
  internal_elem* elem(Key k) {
    return find(buck_entry(k), k);
  }

  bool has_delete(TransItem& item) {
    return item.has_afterC();
  }
  
  bool has_insert(TransItem& item) {
    return item.has_undo();
  }

  bool validity_check(TransItem& item, internal_elem *e) {
    return has_insert(item) || e->valid();
  }
  
  bool is_bucket(void *key) {
    return (uintptr_t)key & bucket_bit;
  }
  unsigned bucket_value(void *key) {
    assert(is_bucket(key));
    return (uintptr_t)key >> 1;
  }
  void *pack_bucket(unsigned bucket) {
    return pack((bucket << 1) | bucket_bit);
  }

  void inc_version(Version& v) {
    assert(is_locked(v));
    Version cur = v & version_mask;
    cur = (cur+1) & version_mask;
    v = cur | (v & ~version_mask);
  }

  bool is_locked(Version v) {
    return v & lock_bit;
  }
  void lock(Version *v) {
    while (1) {
      Version cur = *v;
      if (!(cur&lock_bit) && bool_cmpxchg(v, cur, cur|lock_bit)) {
        break;
      }
      relax_fence();
    }
  }
  void unlock(Version *v) {
    assert(is_locked(*v));
    Version cur = *v;
    cur &= ~lock_bit;
    *v = cur;
  }

  template <bool markValid = false>
  void insert_locked(bucket_entry& buck, Key k, const Value& val) {
    assert(is_locked(buck.version));
    auto new_head = new internal_elem(k, val);
    internal_elem *cur_head = buck.head;
    new_head->next = cur_head;
    if (markValid) {
      new_head->valid() = true;
    }
    buck.head = new_head;
    inc_version(buck.version);
  }

  void atomicRead(internal_elem *e, Version& vers, Value& val) {
    Version v2;
    do {
      vers = e->version;
      fence();
      val = e->value;
      fence();
      v2 = e->version;
    } while (vers != v2);
  }


};
