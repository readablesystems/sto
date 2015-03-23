#pragma once

#include "masstree-beta/masstree.hh"
#include "masstree-beta/kvthread.hh"
#include "masstree-beta/masstree_tcursor.hh"
#include "masstree-beta/masstree_insert.hh"
#include "masstree-beta/masstree_print.hh"
#include "masstree-beta/masstree_remove.hh"
#include "masstree-beta/masstree_scan.hh"
#include "masstree-beta/string.hh"
#include "Transaction.hh"

#include "versioned_value.hh"
#include "stuffed_str.hh"

#define RCU 0
#define ABORT_ON_WRITE_READ_CONFLICT 0

#define READ_MY_WRITES 0

#include "Debug_rcu.hh"

#if PERF_LOGGING
uint64_t node_aborts;
#endif

typedef stuffed_str<uint32_t> versioned_str;

/*
// TODO: ughhh
void print(FILE* f, const char* prefix,
           int indent, lcdf::Str key, kvtimestamp_t,
           char* suffix) {
  //      int fprintf(FILE * , const char * , ...);                                                                                                
  fprintf(f, "%s%*s%.*s = %d%s (version %d)\n", prefix, indent, "", key.len, key.s, value_, suffix, version_);
}

*/

struct versioned_str_struct : public versioned_str {
  typedef Masstree::Str value_type;
  typedef versioned_str::stuff_type version_type;
  
  bool needsResize(const value_type& v) {
    return needs_resize(v.length());
  }
  
  versioned_str_struct* resizeIfNeeded(const value_type& potential_new_value) {
    // TODO: this cast is only safe because we have no ivars or virtual methods
    return (versioned_str_struct*)this->reserve(versioned_str::size_for(potential_new_value.length()));
  }
  
  template <typename StringType>
  inline void set_value(const StringType& v) {
    auto *ret = this->replace(v.data(), v.length());
    // we should already be the proper size at this point
    (void)ret;
    assert(ret == this);
  }
  
  // responsibility is on the caller of this method to make sure this read is atomic
  value_type read_value() {
    return Masstree::Str(this->data(), this->length());
  }
  
  inline version_type& version() {
    return stuff();
  }
};

template <typename V, typename Box = versioned_value_struct<V>>
class MassTrans : public Shared {
public:
#if !RCU
  typedef debug_threadinfo threadinfo;
#endif

  struct ti_wrapper {
    threadinfo *ti;
  };

  typedef V value_type;
  typedef ti_wrapper threadinfo_type;
  typedef Masstree::Str Str;

  typedef typename Box::version_type Version;

  static __thread threadinfo_type mythreadinfo;

private:

  typedef Box versioned_value;
  
public:

  MassTrans() {
#if RCU
    if (!mythreadinfo.ti) {
      auto* ti = threadinfo::make(threadinfo::TI_MAIN, -1);
      mythreadinfo.ti = ti;
    }
#else
    mythreadinfo.ti = new threadinfo;
#endif
    table_.initialize(*mythreadinfo.ti);
    // TODO: technically we could probably free this threadinfo at this point since we won't use it again,
    // but doesn't seem to be possible
  }

  void thread_init() {
#if !RCU
    mythreadinfo.ti = new threadinfo;
    return;
#else

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
#endif
  }

  template <typename ValType>
  bool transGet(Transaction& t, Str key, ValType& retval, threadinfo_type& ti = mythreadinfo) {
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      //      __builtin_prefetch(&e->version);
      auto item = t_read_only_item(t, e);
      if (!validityCheck(item, e)) {
        t.abort();
        return false;
      }
      //      __builtin_prefetch();
      //__builtin_prefetch(e->value.data() - sizeof(std::string::size_type)*3);
#if READ_MY_WRITES
      if (has_delete(item)) {
        return false;
      }
      if (item.has_write()) {
        // read directly from the element if we're inserting it
        if (has_insert(item)) {
	  get_val(retval, e);
        } else {
	  if (item.flags() & copyvals_bit)
	    retval = item.template write_value<value_type>();
	  else
	    // TODO: should we refcount, copy, or...?
	    retval = (value_type&)item.template write_value<void*>();
        }
        return true;
      }
#endif
      Version elem_vers;
      atomicRead(t, e, elem_vers, retval);
      if (item.has_read((Version) valid_check_only_bit))
          item.clear_read();
      item.add_read(elem_vers);
    } else {
      ensureNotFound(t, lp.node(), lp.full_version_value());
    }
    return found;
  }

  template <bool CopyVals = true, typename StringType>
  bool transDelete(Transaction& t, StringType& key, threadinfo_type& ti = mythreadinfo) {
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      auto item = t_item(t, e);
      bool valid = !(e->version() & invalid_bit);
#if READ_MY_WRITES
      if (!valid && has_insert(item)) {
        if (has_delete(item)) {
          // insert-then-delete then delete, so second delete should return false
          return false;
        }
        // otherwise this is an insert-then-delete
	item.clear_flags(insert_bit);
        item.add_flags(delete_bit);
        // key is already in write data since this used to be an insert
        return true;
      } else 
#endif
     if (!valid) {
        t.abort();
        return false;
      }
      assert(valid);
#if READ_MY_WRITES
      // already deleted!
      if (has_delete(item)) {
        return false;
      }
#endif
      // XXX deleting something we put?
      // we only need to check validity, not if the item has changed
      item.add_read(valid_check_only_bit);
      // same as inserts we need to store (copy) key so we can lookup to remove later
      item.clear_write();
      if (std::is_same<const std::string, const StringType>::value) {
	if (CopyVals)
	  item.add_write(key).add_flags(copyvals_bit);
	else
	  item.add_write(pack(key));
      } else
	item.add_write(std::string(key)).add_flags(copyvals_bit);
      item.add_flags(delete_bit);
      return found;
    } else {
      ensureNotFound(t, lp.node(), lp.full_version_value());
      return found;
    }
  }

  template <bool CopyVals = true, bool INSERT = true, bool SET = true, typename StringType>
  bool transPut(Transaction& t, StringType& key, const value_type& value, threadinfo_type& ti = mythreadinfo) {
    // optimization to do an unlocked lookup first
    if (SET) {
      unlocked_cursor_type lp(table_, key);
      bool found = lp.find_unlocked(*ti.ti);
      if (found) {
        return handlePutFound<CopyVals, INSERT, SET>(t, lp.value(), key, value);
      } else {
        if (!INSERT) {
          ensureNotFound(t, lp.node(), lp.full_version_value());
          return false;
        }
      }
    }

    cursor_type lp(table_, key);
    bool found = lp.find_insert(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      lp.finish(0, *ti.ti);
      return handlePutFound<CopyVals, INSERT, SET>(t, e, key, value);
    } else {
      //      auto p = ti.ti->allocate(sizeof(versioned_value), memtag_value);
      versioned_value* val = (versioned_value*)versioned_value::make(value, invalid_bit);
      lp.value() = val;
#if ABORT_ON_WRITE_READ_CONFLICT
      auto orig_node = lp.node();
      auto orig_version = lp.previous_full_version_value();
      auto upd_version = lp.next_full_version_value(1);
#endif
      lp.finish(1, *ti.ti);
      fence();

#if !ABORT_ON_WRITE_READ_CONFLICT
      auto orig_node = lp.original_node();
      auto orig_version = lp.original_version_value();
      auto upd_version = lp.updated_version_value();
#endif

      if (updateNodeVersion(t, orig_node, orig_version, upd_version)) {
        // add any new nodes as a result of splits, etc. to the read/absent set
#if !ABORT_ON_WRITE_READ_CONFLICT
        for (auto&& pair : lp.new_nodes()) {
          t.new_item(this, tag_inter(pair.first)).add_read(pair.second);
        }
#endif
      }
      auto item = t.new_item(this, val);
      if (std::is_same<const std::string, const StringType>::value) {
	if (CopyVals)
	  item.add_write(key).add_flags(copyvals_bit);
	else
	  item.add_write(pack(key));
      } else
        // force a copy
	// technically a user could request CopyVals=false and pass non-std::string keys
	// but we don't support that
        item.add_write(std::string(key)).add_flags(copyvals_bit);

      item.add_flags(insert_bit);
      return found;
    }
  }

  template <bool CopyVals = true, typename StringType>
  bool transUpdate(Transaction& t, StringType& k, const value_type& v, threadinfo_type& ti = mythreadinfo) {
    return transPut<CopyVals, /*insert*/false, /*set*/true>(t, k, v, ti);
  }

  template <bool CopyVals = true, typename StringType>
  bool transInsert(Transaction& t, StringType& k, const value_type& v, threadinfo_type& ti = mythreadinfo) {
    return !transPut<CopyVals, /*insert*/true, /*set*/false>(t, k, v, ti);
  }


  size_t approx_size() const {
    // looks like if we want to implement this we have to tree walkers and all sorts of annoying things like that. could also possibly
    // do a range query and just count keys
    return 0;
  }

  // goddammit templates/hax
  template <typename Callback, typename V2>
  static bool query_callback_overload(Str key, versioned_value_struct<V2> *val, Callback c) {
    return c(key, val->read_value());
  }

  template <typename Callback>
  static bool query_callback_overload(Str key, versioned_str_struct *val, Callback c) {
    return c(key, val);
  }

  // range queries
  template <typename Callback>
  void transQuery(Transaction& t, Str begin, Str end, Callback callback, threadinfo_type& ti = mythreadinfo) {
    auto node_callback = [&] (leaf_type* node, typename unlocked_cursor_type::nodeversion_value_type version) {
      this->ensureNotFound(t, node, version);
    };
    auto value_callback = [&] (Str key, versioned_value* value) {
      // TODO: this needs to read my writes
      this->t_read_only_item(t, value).add_read(value->version());
      return query_callback_overload(key, value, callback);
    };

    range_scanner<decltype(node_callback), decltype(value_callback)> scanner(end, node_callback, value_callback);
    table_.scan(begin, true, scanner, *ti.ti);
  }

  template <typename Callback>
  void transRQuery(Transaction& t, Str begin, Str end, Callback callback, threadinfo_type& ti = mythreadinfo) {
    auto node_callback = [&] (leaf_type* node, typename unlocked_cursor_type::nodeversion_value_type version) {
      this->ensureNotFound(t, node, version);
    };
    auto value_callback = [&] (Str key, versioned_value* value) {
      this->t_read_only_item(t, value).add_read(value->version());
      return query_callback_overload(key, value, callback);
    };

    range_scanner<decltype(node_callback), decltype(value_callback), true> scanner(end, node_callback, value_callback);
    table_.rscan(begin, true, scanner, *ti.ti);
  }

private:
  // range query class thang
  template <typename Nodecallback, typename Valuecallback, bool Reverse = false>
  class range_scanner {
  public:
    range_scanner(Str upper, Nodecallback nodecallback, Valuecallback valuecallback) : boundary_(upper), boundary_compar_(false),
                                                                                       nodecallback_(nodecallback), valuecallback_(valuecallback) {}

    template <typename ITER, typename KEY>
    void check(const ITER& iter,
               const KEY& key) {
      int min = std::min(boundary_.length(), key.prefix_length());
      int cmp = memcmp(boundary_.data(), key.full_string().data(), min);
      if (!Reverse) {
        if (cmp < 0 || (cmp == 0 && boundary_.length() <= key.prefix_length()))
          boundary_compar_ = true;
        else if (cmp == 0) {
          uint64_t last_ikey = iter.node()->ikey0_[iter.permutation()[iter.permutation().size() - 1]];
          uint64_t slice = string_slice<uint64_t>::make_comparable(boundary_.data() + key.prefix_length(), std::min(boundary_.length() - key.prefix_length(), 8));
          boundary_compar_ = slice <= last_ikey;
        }
      } else {
        if (cmp >= 0)
          boundary_compar_ = true;
      }
    }

    template <typename ITER>
    void visit_leaf(const ITER& iter, const Masstree::key<uint64_t>& key, threadinfo& ) {
      nodecallback_(iter.node(), iter.full_version_value());
      if (this->boundary_) {
        check(iter, key);
      }
    }
    bool visit_value(const Masstree::key<uint64_t>& key, versioned_value *value, threadinfo&) {
      if (this->boundary_compar_) {
        if ((!Reverse && boundary_ <= key.full_string()) ||
            ( Reverse && boundary_ >= key.full_string()))
          return false;
      }
      
      return valuecallback_(key.full_string(), value);
    }

    Str boundary_;
    bool boundary_compar_;
    Nodecallback nodecallback_;
    Valuecallback valuecallback_;
  };

public:

  // non-transaction put/get. These just wrap a transaction get/put
  bool put(Str key, const value_type& value, threadinfo_type& ti = mythreadinfo) {
    Transaction t;
    auto ret = transPut(t, key, value, ti);
    t.commit();
    return ret;
  }

  bool get(Str key, value_type& value, threadinfo_type& ti = mythreadinfo) {
    Transaction t;
    auto ret = transGet(t, key, value, ti);
    t.commit();
    return ret;
  }

  // implementation of Shared object methods

  void lock(versioned_value *e) {
#if NOSORT
    if (!is_locked(e->version()))
#endif
    lock(&e->version());
  }
  void unlock(versioned_value *e) {
    unlock(&e->version());
  }

  void lock(TransItem& item) {
    lock(item.key<versioned_value*>());
  }
  void unlock(TransItem& item) {
    unlock(item.key<versioned_value*>());
  }
  bool check(const TransItem& item, const Transaction& t) {
    if (is_inter(item)) {
      auto n = untag_inter(item.key<leaf_type*>());
      auto cur_version = n->full_version_value();
      auto read_version = item.template read_value<typename unlocked_cursor_type::nodeversion_value_type>();
      //      if (cur_version != read_version)
      //printf("node versions disagree: %d vs %d\n", cur_version, read_version);
      // XXXXXX node_aborts
      return cur_version == read_version;
        //&& !(cur_version & (unlocked_cursor_type::nodeversion_type::traits_type::lock_bit));
    }
    auto e = item.key<versioned_value*>();
    auto read_version = item.template read_value<Version>();
    //    if (read_version != e->version)
    //printf("leaf versions disagree: %d vs %d\n", e->version, read_version);
    bool valid = validityCheck(item, e);
    if (!valid)
      return false;
#if NOSORT
    bool lockedCheck = true;
#else
    bool lockedCheck = !is_locked(e->version()) || item.has_lock(t);
#endif
    return lockedCheck && ((read_version & valid_check_only_bit) || versionCheck(read_version, e->version()));
  }
  void install(TransItem& item, Transaction::tid_type) {
    assert(!is_inter(item));
    auto e = item.key<versioned_value*>();
    assert(is_locked(e->version()));
    if (has_delete(item)) {
      if (!has_insert(item)) {
        assert(!(e->version() & invalid_bit));
        e->version() |= invalid_bit;
        fence();
      }
      // TODO: hashtable did this in afterC, we're doing this now, unclear really which is better
      // (if we do it now, we take more time while holding other locks, if we wait, we make other transactions abort more
      // from looking up an invalid node)
      auto &s = item.flags() & copyvals_bit ? 
	item.template write_value<std::string>()
	: (std::string&)item.template write_value<void*>();
      bool success = remove(Str(s));
      // no one should be able to remove since we hold the lock
      (void)success;
      assert(success);
      return;
    }
    if (!has_insert(item)) {
      value_type& v = item.flags() & copyvals_bit ?
	item.template write_value<value_type>()
	: (value_type&)item.template write_value<void*>();
      e->set_value(v);
    }
    // also marks valid if needed
    inc_version(e->version());
  }

  void cleanup(TransItem& item, bool committed) {
      if (!committed && has_insert(item)) {
        // remove node
        auto& stdstr = item.flags() & copyvals_bit ?
	  item.template write_value<std::string>()
	  : (std::string&)item.template write_value<void*>();
        // does not copy
        Str s(stdstr);
        bool success = remove(s);
        (void)success;
        assert(success);
    }
  }

  template <typename ValType>
  static inline void *pack(const ValType& value) {
    assert(sizeof(ValType) <= sizeof(void*));
    void *placed_val = *(void**)&value;
    return placed_val;
  }

  bool remove(const Str& key, threadinfo_type& ti = mythreadinfo) {
    cursor_type lp(table_, key);
    bool found = lp.find_locked(*ti.ti);
    //    ti.ti->rcu_register(lp.value());
    lp.finish(found ? -1 : 0, *ti.ti);
    // rcu the value
    return found;
  }

  void print() {
    //    table_.print();
  }
  

  // these are mostly for concurrent.cc (which currently requires specifically named methods)
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
      return value_type();
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
  value_type transRead_nocheck(Transaction& , int ) { return value_type(); }
  void transWrite_nocheck(Transaction&, int , value_type ) {}
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
  // called once we've checked our own writes for a found put()
  template <bool CopyVals = true>
  void reallyHandlePutFound(Transaction& t, TransProxy& item, versioned_value *e, Str key, const value_type& value) {
    // resizing takes a lot of effort, so we first check if we'll need to
    // (values never shrink in size, so if we don't need to resize, we'll never need to)
    auto *new_location = e;
    bool needsResize = e->needsResize(value);
    if (needsResize) {
      if (!has_insert(item)) {
        // TODO: might be faster to do this part at commit time but easiest to just do it now
        lock(e);
        // we had a weird race condition and now this element is gone. just abort at this point
        if (e->version() & invalid_bit) {
          unlock(e);
          t.abort();
          return;
        }
        e->version() |= invalid_bit;
        // should be ok to unlock now because any attempted writes will be forced to abort
        unlock(e);
      }
      // does the actual realloc. at this point e is marked invalid so we don't have to worry about
      // other threads changing e's value
      new_location = e->resizeIfNeeded(value);
      // e can't get bigger so this should always be true
      assert(new_location != e);
      if (!has_insert(item)) {
        // copied version is going to be invalid because we just had to mark e invalid
        new_location->version() &= ~invalid_bit;
      }
      cursor_type lp(table_, key);
      // TODO: not even trying to pass around threadinfo here
      bool found = lp.find_locked(*mythreadinfo.ti);
      (void)found;
      assert(found);
      lp.value() = new_location;
      lp.finish(0, *mythreadinfo.ti);
      // now rcu free "e"
    }
#if READ_MY_WRITES
    if (has_insert(item)) {
      new_location->set_value(value);
    } else
#endif
    {
      if (new_location == e) {
	if (CopyVals)
	  item.add_write(value).add_flags(copyvals_bit);
	else
	  item.add_write(pack(value));
      } else {
	if (CopyVals)
	  t.new_item(this, new_location).add_write(value).add_flags(copyvals_bit);
	else
	  t.new_item(this, new_location).add_write(pack(value));
      }
    }
  }

  // returns true if already in tree, false otherwise
  // handles a transactional put when the given key is already in the tree
  template <bool CopyVals = true, bool INSERT=true, bool SET=true>
  bool handlePutFound(Transaction& t, versioned_value *e, Str key, const value_type& value) {
    auto item = t_item(t, e);
    if (!validityCheck(item, e)) {
      t.abort();
      return false;
    }
#if READ_MY_WRITES
    if (has_delete(item)) {
      // delete-then-insert == update (technically v# would get set to 0, but this doesn't matter
      // if user can't read v#)
      if (INSERT) {
        item.assign_flags(0);
        assert(!has_delete(item));
        reallyHandlePutFound<CopyVals>(t, item, e, key, value);
      } else {
        // delete-then-update == not found
        // delete will check for other deletes so we don't need to re-log that check
      }
      return false;
    }
    // make sure this item doesn't get deleted (we don't care about other updates to it though)
    if (!item.has_read() && !has_insert(item))
#endif
    {
      item.add_read(valid_check_only_bit);
    }
    if (SET) {
      reallyHandlePutFound<CopyVals>(t, item, e, key, value);
    }
    return true;
  }

  template <typename NODE, typename VERSION>
  void ensureNotFound(Transaction& t, NODE n, VERSION v) {
    // TODO: could be more efficient to use fresh_item here, but that will also require more work for read-then-insert
    auto item = t_read_only_item(t, tag_inter(n));
    if (!item.has_read()) {
      item.add_read(v);
    }
  }

  template <typename NODE, typename VERSION>
  bool updateNodeVersion(Transaction& t, NODE *node, VERSION prev_version, VERSION new_version) {
    (void)t, (void)node, (void)prev_version, (void)new_version;
#if READ_MY_WRITES
    if (auto node_item = t.check_item(this, tag_inter(node))) {
      if (node_item->has_read() &&
          prev_version == node_item->template read_value<VERSION>()) {
        node_item->update_read(node_item->template read_value<VERSION>(),
                               new_version);
        return true;
      }
    }
#endif
    return false;
  }

  template <typename T>
  TransProxy t_item(Transaction& t, T e) {
#if READ_MY_WRITES
    return t.item(this, e);
#else
    return t.fresh_item(this, e);
#endif
  }

  template <typename T>
  TransProxy t_read_only_item(Transaction& t, T e) {
#if READ_MY_WRITES
    return t.read_item(this, e);
#else
    return t.fresh_item(this, e);
#endif
  }

  bool has_insert(const TransItem& item) {
      return item.flags() & insert_bit;
  }
  bool has_delete(const TransItem& item) {
      return item.flags() & delete_bit;
  }

  bool validityCheck(const TransItem& item, versioned_value *e) {
    return //likely(has_insert(item)) || !(e->version & invalid_bit);
      likely(!(e->version() & invalid_bit)) || has_insert(item);
  }

  static constexpr Version lock_bit = 1U<<(sizeof(Version)*8 - 1);
  static constexpr Version invalid_bit = 1U<<(sizeof(Version)*8 - 2);
  static constexpr Version valid_check_only_bit = 1U<<(sizeof(Version)*8 - 3);
  static constexpr Version version_mask = ~(lock_bit|invalid_bit|valid_check_only_bit);

  static constexpr uintptr_t internode_bit = 1<<0;

  static constexpr TransItem::flags_type insert_bit = TransItem::user0_bit;
  static constexpr TransItem::flags_type delete_bit = TransItem::user0_bit<<1;
  static constexpr TransItem::flags_type copyvals_bit = TransItem::user0_bit<<2;

  template <typename T>
  static T* tag_inter(T* p) {
    return (T*)((uintptr_t)p | internode_bit);
  }
  template <typename T>
  static T* untag_inter(T* p) {
    return (T*)((uintptr_t)p & ~internode_bit);
  }
  template <typename T>
  static bool is_inter(T* p) {
    return (uintptr_t)p & internode_bit;
  }
  static bool is_inter(const TransItem& t) {
      return is_inter(t.key<versioned_value*>());
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

  void atomicRead(Transaction& t, versioned_value *e, Version& vers, value_type& val) {
    Version v2;
    do {
      v2 = e->version();
      if (is_locked(v2))
	t.abort();
      fence();
      get_val(val, e);
      fence();
      vers = e->version();
      fence();
    } while (vers != v2);
  }

  template <typename ValType>
  void get_val(ValType& val, versioned_value *e) {
    val = e->read_value();
  }
  void get_val(std::string& val, versioned_value *e) {
    auto str = (Str)e->read_value();
    val.assign(str.data(), str.length());
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

template <typename V, typename Box>
__thread typename MassTrans<V, Box>::threadinfo_type MassTrans<V, Box>::mythreadinfo;

template <typename V, typename Box>
constexpr typename MassTrans<V, Box>::Version MassTrans<V, Box>::invalid_bit;

