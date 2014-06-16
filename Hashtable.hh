#pragma once

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
  const Version lock_bit = 1U<<(sizeof(Version)*8 - 1);
  const intptr_t bucket_bit = 1U<<0;

  Hashtable() : map_() {
    map_.resize(INIT_SIZE);
  }
  
  inline size_t hash(Key k) {
    std::hash<Key> hash_fn;
    return hash_fn(k);
  }

  inline size_t nbuckets() {
    return INIT_SIZE;
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

  void inc_version(Version& v) {
    // TODO: work with lock bit
    v++;
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

  bool transGet(Transaction& t, Key k, Value& retval) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto& item = t.item(this, e);
      if (item.has_write()) {
        // TODO: what if element is marked invalid because of a delete, not
        // us doing an insert
        retval = item.template write_value<Value>();
        return true;
      }
      // we do this before checking validity because a validity change will change the version number
      Version elem_vers;
      atomicRead(e, elem_vers, retval);
      fence();
      if (!e->valid()) {
        t.abort();
        return false;
      }
      // elem_vers changes on delete so we'll also abort this transaction if we're deleted
      t.add_read(item, elem_vers);
      return true;
    } else {
      auto& item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read()) {
        t.add_read(item, buck_version);
      }
      return false;
    }
  }

#if DELETE
  // returns true if successful
  bool transDelete(Transaction& t, Key k) {
    bucket_entry& buck = buck_entry(k);
    Version buck_version = buck.version;
    fence();
    internal_elem *e = find(buck, k);
    if (e) {
      auto& item = t.item(this, e);
      lock(&e->version);
      if (!item.has_write() && !e->valid()) {
        unlock(&e->version);
        t.abort();
        return false;
      }
      e->valid = false;
      inc_version(e->version);
      Version newv = e->version;
      unlock(&e->version);
      // delete shouldn't invalidate any of our own prior reads
      if (item.has_read() && versionCheck(item.template read_value<Version>(), newv-1)) {
        item.add_read(newv);
      }
      // probably need to mark deletes in a separate manner from insert/set
      // TODO: currently just mark afterC. how do we handle delete-then-insert in the same transaction then??
      item.add_afterC();
      return true;
    } else {
      // add a read that yes this element doesn't exist
      auto& item = t.item(this, pack_bucket(bucket(k)));
      if (!item.has_read())
        t.add_read(item, buck_version);
      return false;
  }
#endif

  // returns true if item already existed, false if it did not
  template <bool INSERT = true, bool SET = true>
  bool transPut(Transaction& t, Key k, Value v) {
    // TODO: technically puts don't need to look into the table at all until lock time
    bucket_entry& buck = buck_entry(k);
    // TODO: update doesn't need to lock the table
    lock(&buck.version);
    bool ret;
    internal_elem *e = find(buck, k);
    if (e) {
      auto& item = t.item(this, e);
      if (!item.has_write() && !e->valid()) {
        unlock(&buck.version);
        t.abort();
        // unreachable (t.abort() raises an exception)
        return false;
      } else {
#if DELETE
        // we need to make sure this bucket didn't change (e.g. what was once there got removed)
        auto& itemr = t.item(this, pack_bucket(bucket(k)));
        if (!item.has_read())
          t.add_read(itemr, buck.version);
#endif
        if (SET) {
          t.add_write(item, v);
        }
        ret = true;
      }
    } else {
      if (!INSERT) {
        // add a read that yes this element doesn't exist
        auto& item = t.item(this, pack_bucket(bucket(k)));
        if (!item.has_read())
          t.add_read(item, buck.version);
        unlock(&buck.version);
        return false;
      }

      // not there so need to insert
      insert_locked(buck, k, v); // marked as invalid
      // see if this item was previously read
      auto bucket_item = t.has_item(this, pack_bucket(bucket(k)));
      if (bucket_item) {
        auto new_version = buck.version;
        if (bucket_item->has_read() && 
            versionCheck(bucket_item->template read_value<Version>(), new_version - 1)) {
          // looks like we're the only ones to have updated the version number, so update read's version number
          // to still be valid
          t.add_read(*bucket_item, new_version);
        }
        //} else { could abort transaction now
      }
      // use add_item because we know there are no collisions
      auto& item = t.add_item<false>(this, buck.head);
      // don't actually need to store anything for the write, just mark as valid on install
      // (for now insert and set will just do the same thing on install, set a value and then mark valid)
      t.add_write(item, v);
      // need to remove this item if we abort
      t.add_undo(item);
      ret = false;
    }
    unlock(&buck.version);
    return ret;
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
    return ((v1 ^ v2) & ~lock_bit) == 0;
  }

  bool check(TransData data, bool isReadWrite) {
    if (is_bucket(data.key)) {
      bucket_entry& buck = map_[bucket_value(data.key)];
      return versionCheck(unpack<Version>(data.rdata), buck.version) && !is_locked(buck.version);
    }
    auto el = unpack<internal_elem*>(data.key);
    return versionCheck(unpack<Version>(data.rdata), el->version) && (isReadWrite || !is_locked(el->version));
  }

  void lock(TransData data) {
    assert(!is_bucket(data.key));
    auto el = unpack<internal_elem*>(data.key);
    lock(el);
  }
  void unlock(TransData data) {
    assert(!is_bucket(data.key));
    auto el = unpack<internal_elem*>(data.key);
    unlock(el);
  }
  void install(TransData data) {
    assert(!is_bucket(data.key));
    auto el = unpack<internal_elem*>(data.key);
    assert(is_locked(el));
    el->value = unpack<Value>(data.wdata);
    inc_version(el->version);
    el->valid() = true;
  }

  void undo(TransData data) {
    auto el = unpack<internal_elem*>(data.key);
    assert(!el->valid());
    remove(el);
  }

  void afterC(TransData data) {
#if DELETE
    auto el = unpack<internal_elem*>(data.key);
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
