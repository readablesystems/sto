#pragma once

#include "compiler.hh"

#define HASHTABLE_DELETE 1

template <typename K, typename V, unsigned INIT_SIZE = 129>
class Hashtable : public Shared {
public:
  typedef unsigned Version;
  typedef K Key;
  typedef V Value;

private:
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
    Version version;
    bucket_entry() : head(NULL), version(0) {}
  };

  typedef std::vector<bucket_entry> MapType;
  MapType map_;

public:
  static const Version lock_bit = 1U<<(sizeof(Version)*8 - 1);
  // if set we check only node validity, not the node's version number
  static const Version valid_check_only_bit = 1U<<(sizeof(Version)*8 - 2);
  static const Version version_mask = ~(lock_bit|valid_check_only_bit);
  static const uintptr_t bucket_bit = 1U<<0;

  Hashtable(unsigned size = INIT_SIZE) : map_() {
    map_.resize(size);
  }
  
  inline size_t hash(Key k) {
    std::hash<Key> hash_fn;
    return hash_fn(k);
  }

  inline size_t nbuckets() {
    return map_.size();
  }

  inline size_t bucket(Key k) {
    return hash(k) % nbuckets();
  }

  // invariants:
  // if bucket list is different, version number is different
  // if bucket is locked, can still do everything but insert/delete
  // one way to rehash would be to lock every bucket...
  // could also potentially just check every bucket's version number after rehash (transaction)

  // transaction time!
  // key is either a pointer to entry in hashtable (if actually in hashtable)
  // or a bucket index where the key would be, if not in hashtable
  // TODO: could be relatively common that insert/delete happens in same bucket as a key but the key itself
  // was not inserted or deleted. would need to store key in the transaction as well to check this case

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
      insert_locked(buck, k, val);
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

#if 0
  template <typename T>
  void *pack_valid_check(T* ptr) {
    return pack((uintptr_t)ptr | validity_check_bit);
  }
#endif

  void inc_version(Version& v) {
    assert(is_locked(v));
    // TODO: work with lock bit
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

  void insert_locked(bucket_entry& buck, Key k, Value val) {
    assert(is_locked(buck.version));
    auto new_head = new internal_elem(k, val);
    internal_elem *cur_head = buck.head;
    new_head->next = cur_head;
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

  // returns true if found false if not
  bool transGet(Transaction& t, Key k, Value& retval) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto& item = t.item(this, e);
      // deleted
      if (item.has_afterC()) {
        return false;
      }
      if (item.has_write()) {
        retval = item.template write_value<Value>();
        return true;
      }
      if (!e->valid()) {
        t.abort();
        return false;
      }
      Version elem_vers;
      // "atomic" read of both the current value and the version #
      atomicRead(e, elem_vers, retval);
      // abort both on node change and node delete
      if (!item.has_read() || item.template read_value<Version>() & valid_check_only_bit) {
        t.add_read(item, elem_vers);
      }
      return true;
    } else {
      auto& item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read()) {
        t.add_read(item, buck_version);
      }
      return false;
    }
  }

#if 0
  unsigned transCount(Transaction& t, Key k) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto& item = t.item(this, e);
      if (!item.has_write() && !e->valid()) {
        t.abort();
        return false;
      }
      if (!item.has_read()) {
        t.add_item(pack_presence(e)).add_read(0);
      }
    } else {
      auto& item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read()) {
        t.add_read(item, buck_version);
      }
    }
  }
#endif

#if HASHTABLE_DELETE
  // returns true if successful
  bool transDelete(Transaction& t, Key k) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto& item = t.item(this, e);
      // already deleted!
      if (item.has_afterC()) {
        return false;
      }
      if (!item.has_write() && !e->valid()) {
        t.abort();
        return false;
      } else if (item.has_undo() && !e->valid()) {
        // we're deleting our own insert. special case this to just remove element and just check for no insert at commit
        remove(e);
        // no way to remove an item (would be pretty inefficient)
        // so we just unmark all attributes so the item is ignored
        item.remove_read();
        item.remove_write();
        item.remove_undo();
        item.remove_afterC();
        // insert-then-delete still can only succeed if no one else inserts this node so we add a check for that
        auto& itemb = t.item(this, pack_bucket(bucket(k)));
        if (!itemb.has_read()) {
          t.add_read(itemb, buck_version);
        }
        return true;
      }
      assert(e->valid());
      // we need to make sure this bucket didn't change (e.g. what was once there got removed)
      if (!item.has_read())
        // we only need to check validity, not presence
        t.add_read(item, valid_check_only_bit);
      // we use has_afterC() to detect deletes so we don't need any other data 
      // for deletes, just to mark it as a write
      if (!item.has_write())
        t.add_write(item, 0);
      t.add_afterC(item);
      return true;
    } else {
      // add a read that yes this element doesn't exist
      auto& item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read())
        t.add_read(item, buck_version);
      return false;
    }
  }
#endif

  // returns true if item already existed, false if it did not
  template <bool INSERT = true, bool SET = true>
  bool transPut(Transaction& t, Key k, Value v) {
    // TODO: technically puts don't need to look into the table at all until lock time
    bucket_entry& buck = buck_entry(k);
    // TODO: update doesn't need to lock the table
    lock(&buck.version);
    internal_elem *e = find(buck, k);
    if (e) {
      unlock(&buck.version);
      auto& item = t.item(this, e);
      if (item.has_afterC()) {
        // delete-then-insert == update (technically v# would get set to 0, but this doesn't matter
        // if user can't read v#)
        if (INSERT) {
          item.remove_afterC();
          t.add_write(item, v);
        } else {
          // delete-then-update == not found
          // delete will check for other deletes so we don't need to re-log that check
        }
        return false;
      }

      if (!item.has_write() && !e->valid()) {
        t.abort();
        // unreachable (t.abort() raises an exception)
        return false;
      }
#if HASHTABLE_DELETE
      // we need to make sure this item stays here
      if (!item.has_read())
        // we only need to check validity, not presence
        t.add_read(item, valid_check_only_bit);
#endif
      if (SET) {
        t.add_write(item, v);
      }
      return true;
    } else {
      if (!INSERT) {
        auto buck_version = buck.version;
        unlock(&buck.version);
        auto& item = t.item(this, pack_bucket(bucket(k)));
        if (!item.has_read()) {
          t.add_read(item, buck_version);
        }
        return false;
      }

      // not there so need to insert
      insert_locked(buck, k, v); // marked as invalid
      auto new_version = buck.version;
      auto new_head = buck.head;
      unlock(&buck.version);
      // see if this item was previously read
      auto bucket_item = t.has_item(this, pack_bucket(bucket(k)));
      if (bucket_item) {
        if (bucket_item->has_read() && 
            versionCheck(bucket_item->template read_value<Version>(), new_version - 1)) {
          // looks like we're the only ones to have updated the version number, so update read's version number
          // to still be valid
          t.add_read(*bucket_item, new_version);
        }
        //} else { could abort transaction now
      }
      // use add_item because we know there are no collisions
      auto& item = t.add_item<false>(this, new_head);
      // don't actually need to store anything for the write, just mark as valid on install
      // (for now insert and set will just do the same thing on install, set a value and then mark valid)
      t.add_write(item, v);
      // need to remove this item if we abort
      t.add_undo(item);
      return false;
    }
  }

  // returns true if successful
  bool transInsert(Transaction& t, Key k, Value v) {
    return !transPut</*insert*/true, /*set*/false>(t, k, v);
  }

  // aka putIfAbsent (returns true if successful)
  bool transUpdate(Transaction& t, Key k, Value v) {
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

  bool check(TransItem item, bool isReadWrite) {
    if (is_bucket(item.key())) {
      bucket_entry& buck = map_[bucket_value(item.key())];
      return versionCheck(item.template read_value<Version>(), buck.version) && !is_locked(buck.version);
    }
    auto el = unpack<internal_elem*>(item.key());
    auto read_version = item.template read_value<Version>();
    // if item has undo then its an insert so no validity check needed.
    // otherwise we check that it is both valid and not locked
    bool validity_check = item.has_undo() || (el->valid() && (isReadWrite || !is_locked(el->version)));
    return validity_check && ((read_version & valid_check_only_bit) ||
                              versionCheck(read_version, el->version));
  }

  void lock(TransItem item) {
    assert(!is_bucket(item.key()));
    auto el = unpack<internal_elem*>(item.key());
    lock(el);
  }
  void unlock(TransItem item) {
    assert(!is_bucket(item.key()));
    auto el = unpack<internal_elem*>(item.key());
    unlock(el);
  }
  void install(TransItem item) {
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
    el->value = item.template write_value<Value>();
    inc_version(el->version);
    el->valid() = true;
  }

  void undo(TransItem item) {
    auto el = unpack<internal_elem*>(item.key());
    assert(!el->valid());
    remove(el);
  }

  void afterC(TransItem item) {
#if HASHTABLE_DELETE
    auto el = unpack<internal_elem*>(item.key());
    assert(!el->valid());
    remove(el);
#endif
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
    Transaction::cleanup([=] () { free(cur); });
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

  void transWrite(Transaction& t, Key k, Value v) {
    transPut(t, k, v);
  }
  Value transRead(Transaction& t, Key k) {
    Value v;
    if (!transGet(t, k, v)) {
      return 0;
    }
    return v;
  }
  Value transRead_nocheck(Transaction& t, Key k) { return 0; }
  void transWrite_nocheck(Transaction& t, Key k, Value v) {}
  Value read(Key k) {
    Transaction t;
    return transRead(t, k);
  }

  void insert(Key k, Value v) {
    Transaction t;
    transInsert(t, k, v);
    t.commit();
  }

  void erase(Key k) {}

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
  void rehash(unsigned n) {}
  void reserve(unsigned n) {}
  
  private:
  bucket_entry& buck_entry(Key k) {
    return map_[bucket(k)];
  }

  internal_elem* find(bucket_entry& buck, Key k) {
    internal_elem *list = buck.head;
    while (list && !(list->key == k)) {
      list = list->next;
    }
    return list;
  }

  internal_elem* elem(Key k) {
    return find(buck_entry(k), k);
  }

};
