#pragma once

#include "masstree-beta/masstree.hh"
#include "masstree-beta/kvthread.hh"
#include "masstree-beta/masstree_tcursor.hh"
#include "masstree-beta/masstree_insert.hh"
#include "masstree-beta/masstree_print.hh"
#include "masstree-beta/masstree_remove.hh"
#include "masstree-beta/string.hh"
#include "Transaction.hh"

template <typename V>
class MassTrans : public Shared {
public:
  struct ti_wrapper {
    threadinfo *ti;
  };

  typedef V value_type;
  typedef ti_wrapper threadinfo_type;
  typedef Masstree::Str Str;

  static __thread threadinfo_type mythreadinfo;

private:
  typedef uint32_t Version;
  struct versioned_value : public threadinfo::rcu_callback {
    Version version;
    value_type value;

    void print(FILE* f, const char* prefix,
               int indent, Str key, kvtimestamp_t initial_timestamp,
               char* suffix) {
      fprintf(f, "%s%*s%.*s = %d%s (version %d)\n", prefix, indent, "", key.len, key.s, value, suffix, version);
    }
    
    // rcu_callback method to self-destruct ourself
    void operator()(threadinfo& ti) override {
      // this will call value's destructor
      this->versioned_value::~versioned_value();
      // and free our memory too
      ti.deallocate(this, sizeof(versioned_value), memtag_value);
    }
  };

public:
  MassTrans() {
    if (!mythreadinfo.ti) {
      auto* ti = threadinfo::make(threadinfo::TI_MAIN, -1);
      mythreadinfo.ti = ti;
    }
    table_.initialize(*mythreadinfo.ti);
    // TODO: technically we could probably free this threadinfo at this point since we won't use it again,
    // but doesn't seem to be possible
  }

  void thread_init() {
    if (!mythreadinfo.ti) {
      auto* ti = threadinfo::make(threadinfo::TI_PROCESS, Transaction::threadid);
      mythreadinfo.ti = ti;
    }
    Transaction::tinfo[Transaction::threadid].trans_start_callback = [] () {
      mythreadinfo.ti->rcu_start();
    };
    Transaction::tinfo[Transaction::threadid].trans_end_callback = [] () {
      mythreadinfo.ti->rcu_stop();
    };
  }

  bool put(Str key, const value_type& value, threadinfo_type& ti = mythreadinfo) {
    cursor_type lp(table_, key);
    bool found = lp.find_insert(*ti.ti);
    if (found) {
      lock(&lp.value()->version);
    } else {
      auto p = ti.ti->allocate(sizeof(versioned_value), memtag_value);
      lp.value() = new(p) versioned_value;
    }
    // this will uses value's copy constructor (TODO: just doing this may be unsafe and we should be using rcu for dealloc)
    lp.value()->value = value;
    if (found) {
      inc_version(lp.value()->version);
      unlock(&lp.value()->version);
    } else {
      lp.value()->version = 0;
    }
    lp.finish(1, *ti.ti);
    return found;
  }

  bool get(Str key, value_type& value, threadinfo_type& ti = mythreadinfo) {
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found)
      value = lp.value()->value;
    return found;
  }

#if 1
  bool transGet(Transaction& t, Str key, value_type& retval, threadinfo_type& ti = mythreadinfo) {
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      auto& item = t.item(this, e);
      if (!validityCheck(item, e)) {
        t.abort();
        return false;
      }
      if (has_delete(item)) {
        return false;
      }
      if (item.has_write()) {
        // read directly from the element if we're inserting it
        retval = has_insert(item) ? e->value : item.template write_value<value_type>();
        return true;
      }
      Version elem_vers;
      atomicRead(e, elem_vers, retval);
      if (!item.has_read() || item.template read_value<Version>() & valid_check_only_bit) {
        t.add_read(item, elem_vers);
      }
    } else {
      ensureNotFound(t, lp.node(), lp.full_version_value());
    }
    return found;
  }

  bool transDelete(Transaction& t, Str key, threadinfo_type& ti = mythreadinfo) {
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      auto& item = t.item(this, e);
      bool valid = !(e->version & invalid_bit);
      if (!valid && has_insert(item)) {
        // we're deleting our own insert. special case this to just remove now
        bool success = remove(key, ti);
        assert(success); // because node is marked invalid this should never fail

        // insert-then-delete still can only succeed if no one else inserts this node so we add a check for that
        // TODO: usually/maybe always we can get this information while we're removing the key instead of doing a relookup,
        // but insert-then-delete should be relatively rare anyway so for now we don't optimize this case
        unlocked_cursor_type lp2(table_, key);
        bool found = lp2.find_unlocked(*ti.ti);
        if (found) {
          // someone reinserted between when we removed it and now, so abort!
          t.abort();
          return false;
        }
        ensureNotFound(t, lp2.node(), lp2.full_version_value());

        if (lp.node() == lp2.node()) {
          updateNodeVersion(t, lp.node(), lp.full_version_value(), lp2.full_version_value());
        }

        // no way to remove an item (would be pretty inefficient)
        // so we just unmark all attributes so the item is ignored
        item.remove_read();
        item.remove_write();
        item.remove_undo();
        item.remove_afterC();
        item.set_flags(0);
        return true;
      } else if (!valid) {
        t.abort();
        return false;
      }
      assert(valid);
      // already deleted!
      if (has_delete(item)) {
        return false;
      }
      if (!item.has_read()) {
        // we only need to check validity, not if the item has changed
        t.add_read(item, valid_check_only_bit);
      }
      // same as inserts we need to store (copy) key so we can lookup to remove later
      t.add_write(item, std::string(key));
      item.set_flags(delete_bit);
      return found;
    } else {
      ensureNotFound(t, lp.node(), lp.full_version_value());
      return found;
    }
  }

  template <typename NODE, typename VERSION>
  bool updateNodeVersion(Transaction& t, NODE *node, VERSION prev_version, VERSION new_version) {
    auto node_item = t.has_item(this, tag_inter(node));
    if (node_item) {
      if (node_item->has_read() &&
          prev_version == node_item->template read_value<VERSION>()) {
        t.add_read(*node_item, new_version);
        return true;
      }
    }
    return false;
  }

  template <bool INSERT = true, bool SET = true>
  bool transPut(Transaction& t, Str key, const value_type& value, threadinfo_type& ti = mythreadinfo) {
    cursor_type lp(table_, key);
    bool found = lp.find_insert(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      // TODO: should really just do an unlocked lookup for updates
      lp.finish(0, *ti.ti);
      auto& item = t.item(this, e);
      if (!validityCheck(item, e)) {
        t.abort();
        return false;
      }
      if (has_delete(item)) {
        // delete-then-insert == update (technically v# would get set to 0, but this doesn't matter
        // if user can't read v#)
        if (INSERT) {
          item.set_flags(0);
          t.add_write(item, value);
        } else {
          // delete-then-update == not found
          // delete will check for other deletes so we don't need to re-log that check 
        }
        return false;
      }
      // make sure this item doesn't get deleted (we don't care about other updates to it though)
      if (!item.has_read()) {
        t.add_read(item, valid_check_only_bit);
      }
      if (SET) {
        // if we're inserting this element already we can just update the value we're inserting
        if (has_insert(item))
          e->value = value;
        else
          t.add_write(item, value);
      }
      return found;
    } else {
      if (!INSERT) {
        // TODO: for !INSERT we should really just be doing an unlocked lookup
        lp.finish(0, *ti.ti);
        return found;
      }

      auto p = ti.ti->allocate(sizeof(versioned_value), memtag_value);
      versioned_value* val = new(p) versioned_value;
      val->value = value;
      val->version = invalid_bit;
      fence();
      lp.value() = val;
      lp.finish(1, *ti.ti);
      fence();

      auto orig_node = lp.original_node();
      auto orig_version = lp.original_version_value();
      auto upd_version = lp.updated_version_value();

      if (updateNodeVersion(t, orig_node, orig_version, upd_version)) {
        // add any new nodes as a result of splits, etc. to the read/absent set
        for (auto&& pair : lp.new_nodes()) {
          t.add_read(t.add_item<false>(this, tag_inter(pair.first)), pair.second);
        }
      }
      auto& item = t.add_item<false>(this, val);
      // TODO: this isn't great because it's going to require an extra alloc (because Str/std::string is 2 words)...
      // we convert to std::string because Str objects are not copied!!
      t.add_write(item, std::string(key));
      t.add_undo(item);
      return found;
    }
  }

  bool transUpdate(Transaction& t, Str k, const value_type& v, threadinfo_type& ti = mythreadinfo) {
    return transPut</*insert*/false, /*set*/true>(t, k, v, ti);
  }

  bool transInsert(Transaction& t, Str k, const value_type& v, threadinfo_type&ti = mythreadinfo) {
    return !transPut</*insert*/true, /*set*/false>(t, k, v, ti);
  }

  void lock(versioned_value *e) {
    lock(&e->version);
  }
  void unlock(versioned_value *e) {
    unlock(&e->version);
  }

  void lock(TransItem& item) {
    lock(unpack<versioned_value*>(item.key()));
  }
  void unlock(TransItem& item) {
    unlock(unpack<versioned_value*>(item.key()));
  }
  bool check(TransItem& item, bool isReadWrite) {
    if (is_inter(item.key())) {
      auto n = untag_inter(unpack<leaf_type*>(item.key()));
      auto cur_version = n->full_version_value();
      auto read_version = item.template read_value<typename unlocked_cursor_type::nodeversion_value_type>();
      //      if (cur_version != read_version)
      //printf("node versions disagree: %d vs %d\n", cur_version, read_version);
      return cur_version == read_version;
        //&& !(cur_version & (unlocked_cursor_type::nodeversion_type::traits_type::lock_bit));
      
    }
    auto e = unpack<versioned_value*>(item.key());
    auto read_version = item.template read_value<Version>();
    //    if (read_version != e->version)
    //printf("leaf versions disagree: %d vs %d\n", e->version, read_version);
    bool valid = validityCheck(item, e);
    bool lockedCheck = isReadWrite || !is_locked(e->version);
    bool valid_check_only = (read_version & valid_check_only_bit);
    bool vCheck = versionCheck(read_version, e->version);
    bool ret = valid && lockedCheck && (valid_check_only || vCheck);
    return ret;
  }
  void install(TransItem& item) {
    assert(!is_inter(item.key()));
    auto e = unpack<versioned_value*>(item.key());
    assert(is_locked(e->version));
    if (has_delete(item)) {
      assert(!(e->version & invalid_bit));
      e->version |= invalid_bit;
      fence();
      // TODO: hashtable did this in afterC, we're doing this now, unclear really which is better
      // (if we do it now, we take more time while holding other locks, if we wait, we make other transactions abort more
      // from looking up an invalid node)
      auto &s = item.template write_value<std::string>();
      bool success = remove(Str(s));
      // no one should be able to remove since we hold the lock
      assert(success);
      return;
    }
    if (!has_insert(item))
      e->value = item.template write_value<value_type>();
    // also marks valid if needed
    inc_version(e->version);
  }

  void undo(TransItem& item) {
    // remove node
    auto& stdstr = item.template write_value<std::string>();
    // does not copy
    Str s(stdstr);
    bool success = remove(s);
    assert(success);
  }

  void cleanup(TransItem& item) {
    free_packed<versioned_value*>(item.key());
    if (item.has_read())
      free_packed<Version>(item.data.rdata);
    if (has_insert(item) || has_delete(item))
      free_packed<std::string>(item.data.wdata);
    else if (item.has_write())
      free_packed<value_type>(item.data.wdata);
  }

  bool remove(const Str& key, threadinfo_type& ti = mythreadinfo) {
    cursor_type lp(table_, key);
    bool found = lp.find_locked(*ti.ti);
    ti.ti->rcu_register(lp.value());
    lp.finish(found ? -1 : 0, *ti.ti);
    // rcu the value
    return found;
  }

#endif

  void print() {
    table_.print();
  }
  

  void transWrite(Transaction& t, int k, value_type v) {
    char s[16];
    sprintf(s, "%d", k);
    transPut(t, s, v);
  }
  value_type transRead(Transaction& t, int k) {
    char s[16];
    sprintf(s, "%d", k);
    value_type v;
    if (!transGet(t, s, v)) {
      return 0;
    }
    return v;
  }
  bool transGet(Transaction& t, int k, value_type& v) {
    char s[16];
    sprintf(s, "%d", k);
    return transGet(t, s, v);
  }
  bool transPut(Transaction& t, int k, value_type v) {
    char s[16];
    sprintf(s, "%d", k);
    return transPut(t, s, v);
  }
  bool transUpdate(Transaction& t, int k, value_type v) {
    char s[16];
    sprintf(s, "%d", k);
    return transUpdate(t, s, v);
  }
  bool transInsert(Transaction& t, int k, value_type v) {
    char s[16];
    sprintf(s, "%d", k);
    return transInsert(t, s, v);
  }
  bool transDelete(Transaction& t, int k) {
    char s[16];
    sprintf(s, "%d", k);
    return transDelete(t, s);
  }
  value_type transRead_nocheck(Transaction& t, int k) { return value_type(); }
  void transWrite_nocheck(Transaction& t, int k, value_type v) {}
  value_type read(int k) {
    Transaction t;
    return transRead(t, k);
  }

  void put(int k, value_type v) {
    char s[16];
    sprintf(s, "%d", k);
    put(s, v);
  }


private:
  template <typename NODE, typename VERSION>
  void ensureNotFound(Transaction& t, NODE n, VERSION v) {
    // TODO: could be more efficient to use add_item here, but that will also require more work for read-then-insert
    auto& item = t.item(this, tag_inter(n));
    if (!item.has_read()) {
      t.add_read(item, v);
    }
  }

  bool has_insert(TransItem& item) {
    return item.has_undo();
  }
  bool has_delete(TransItem& item) {
    return item.has_flags(delete_bit);
  }

  bool validityCheck(TransItem& item, versioned_value *e) {
    return has_insert(item) || !(e->version & invalid_bit);
  }

  static constexpr Version lock_bit = 1U<<(sizeof(Version)*8 - 1);
  static constexpr Version invalid_bit = 1U<<(sizeof(Version)*8 - 2);
  static constexpr Version valid_check_only_bit = 1U<<(sizeof(Version)*8 - 3);
  static constexpr Version version_mask = ~(lock_bit|invalid_bit|valid_check_only_bit);

  static constexpr uintptr_t internode_bit = 1<<0;

  static constexpr uint8_t delete_bit = 1<<0;

  template <typename T>
  T* tag_inter(T* p) {
    return (T*)((uintptr_t)p | internode_bit);
  }

  template <typename T>
  T* untag_inter(T* p) {
    return (T*)((uintptr_t)p & ~internode_bit);
  }
  template <typename T>
  bool is_inter(T* p) {
    return (uintptr_t)p & internode_bit;
  }

  bool versionCheck(Version v1, Version v2) {
    return ((v1 ^ v2) & version_mask) == 0;
  }
  void inc_version(Version& v) {
    assert(is_locked(v));
    Version cur = v & version_mask;
    cur = (cur+1) & version_mask;
    // set new version and ensure invalid bit is off
    v = (cur | (v & ~version_mask)) & ~invalid_bit;
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

  void atomicRead(versioned_value *e, Version& vers, value_type& val) {
    Version v2;
    do {
      vers = e->version;
      fence();
      val = e->value;
      fence();
      v2 = e->version;
    } while (vers != v2);
  }

  struct table_params : public Masstree::nodeparams<15,15> {
    typedef versioned_value* value_type;
    typedef Masstree::value_print<value_type> value_print_type;
    typedef threadinfo threadinfo_type;
  };
  typedef Masstree::basic_table<table_params> table_type;
  typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
  typedef Masstree::tcursor<table_params> cursor_type;
  typedef Masstree::leaf<table_params> leaf_type;
  table_type table_;
};

template <typename V>
__thread typename MassTrans<V>::threadinfo_type MassTrans<V>::mythreadinfo;
