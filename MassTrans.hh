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
#include <iostream>
//#include "Logger.hh"

#define RCU 1
#define ABORT_ON_WRITE_READ_CONFLICT 0

#ifndef READ_MY_WRITES
#define READ_MY_WRITES 1
#endif

#define USE_TID_AS_VERSION 1


#include "Debug_rcu.hh"

typedef stuffed_str<uint64_t> versioned_str;

struct versioned_str_struct : public versioned_str {
  typedef Masstree::Str value_type;
  typedef uint64_t version_type;
  
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

  inline void deallocate_rcu(threadinfo& ti) {
    ti.deallocate_rcu(this, this->capacity() + sizeof(versioned_str_struct), memtag_value);
  }
};

template <typename V, typename Box = versioned_value_struct_logging<V, std::string>, bool Opacity = false>
  class MassTrans : public Shared {
    
    friend class Checkpointer;
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

  void set_tree_id(uint8_t new_tree_id) {
      tree_id = new_tree_id;
    }
    
  uint8_t get_tree_id() {
      return tree_id;
  }
    
    static void static_init() {
    Transaction::epoch_advance_callback = [] (unsigned) {
      // just advance blindly because of the way Masstree uses epochs
      globalepoch++;
    };
  }

  static void thread_init() {
#if !RCU
    mythreadinfo.ti = new threadinfo;
    return;
#else

    if (!mythreadinfo.ti) {
      auto* ti = threadinfo::make(threadinfo::TI_PROCESS, Transaction::threadid);
      mythreadinfo.ti = ti;
    }
    assert(mythreadinfo.ti != NULL);
    std::cout << Transaction::threadid << std::endl;
    Transaction::tinfo[Transaction::threadid].trans_start_callback = [] () {
      assert(mythreadinfo.ti != NULL);
      mythreadinfo.ti->rcu_start();
    };
    std::cout << Transaction::threadid << std::endl;
    Transaction::tinfo[Transaction::threadid].trans_end_callback = [] () {
      mythreadinfo.ti->rcu_stop();
    };
#endif
  }

  bool insert(Str key,  versioned_value*  val, threadinfo_type&ti = mythreadinfo) {
    cursor_type lp(table_, key);
    bool found = lp.find_insert(*ti.ti);
    lp.value() = val;
      
    lp.finish(1, *ti.ti);
    return !found;
  }
    
  bool insert_if_absent(Str key,  versioned_value*  val, threadinfo_type&ti = mythreadinfo) {
    cursor_type lp(table_, key);
    bool found = lp.find_insert(*ti.ti);
    if (!found) {
      lp.value() = val;
    }
    lp.finish(!found, *ti.ti);
    return !found;
  }
    
  bool get(Str key, versioned_value*& value, threadinfo_type& ti = mythreadinfo) {
    unlocked_cursor_type lp(table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found){
      versioned_value *e = lp.value();
      value = e;
    }
    return found;
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
	  assign_val(retval, e->read_value());
        } else {
	  if (item.flags() & copyvals_bit)
	    retval = item.template write_value<value_type>();
	  else {
	    // TODO: should we refcount, copy, or...?
	    retval = (value_type&)item.template write_value<void*>();
	  }
        }
        return true;
      }
#endif
      Version elem_vers;
      atomicRead(t, e, elem_vers, retval);
      item.add_read(elem_vers);
      if (Opacity)
	check_opacity(t, e->version());
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
      
      //since we are using global tid, logical delete and holding the key for an epoch is not required.
      Version v = e->version();
      fence();
      auto item = t_item(t, e);
      bool valid = !(v & invalid_bit);
#if READ_MY_WRITES
      if (!valid && has_insert(item)) {
        if (has_delete(item)) {
          // insert-then-delete then delete, so second delete should return false
          return false;
        }
        // otherwise this is an insert-then-delete
	// has_insert() is used all over the place so we just keep that flag set
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
      item.add_read(v);
      if (Opacity)
	check_opacity(t, e->version());
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
        //TOOD: Not required
        /*versioned_value *e = lp.value();
        bool deleted = (e->version() & deleted_bit);
        if (!deleted) {
          return handlePutFound<INSERT, SET>(t, lp.value(), key, value);
        } else {
          if (!INSERT) {
            auto& item = t_item(t, e);
            t.add_read(item, e->version());
            return false;
          }
        }*/

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
    Version version = invalid_bit;
    if (found) {
      versioned_value *e = lp.value();
      lp.finish(0, *ti.ti);
      return handlePutFound<CopyVals, INSERT, SET>(t, e, key, value);
    } else {

      //      auto p = ti.ti->allocate(sizeof(versioned_value), memtag_value);
      versioned_value* val = (versioned_value*)versioned_value::make(key, value, version);
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
    static bool query_callback_overload(Str key, versioned_value_struct_logging<V2, std::string> *val, Callback c) {
    return c(key, val->read_value());
  }

  template <typename Callback>
  static bool query_callback_overload(Str key, versioned_str_struct *val, Callback c) {
    return c(key, val);
  }

  // unused (we just stack alloc if no allocator is passed)
  class DefaultValAllocator {
  public:
    value_type* operator()() {
      assert(0);
      return new value_type();
    }
  };

  // range queries
  template <typename Callback, typename ValAllocator = DefaultValAllocator>
  void transQuery(Transaction& t, Str begin, Str end, Callback callback, ValAllocator *va = NULL, threadinfo_type& ti = mythreadinfo) {
    auto node_callback = [&] (leaf_type* node, typename unlocked_cursor_type::nodeversion_value_type version) {
      this->ensureNotFound(t, node, version);
    };
    auto value_callback = [&] (Str key, versioned_value* e) {
      // TODO: this needs to read my writes
      auto item = this->t_read_only_item(t, e);
#if READ_MY_WRITES
      if (has_delete(item)) {
        return true;
      }
      if (item.has_write()) {
        // read directly from the element if we're inserting it
        if (has_insert(item)) {
	  return range_query_has_insert(callback, key, e, va);
	  //return callback(key, val);
        } else {
	  if (item.flags() & copyvals_bit)
	    return callback(key, item.template write_value<value_type>());
	  else {
	    // TODO: should we refcount, copy, or...?
	    return callback(key, (value_type&)item.template write_value<void*>());
	  }
        }
      }
#endif
      // not sure of a better way to do this
      value_type stack_val;
      value_type& val = va ? *(*va)() : stack_val;
      Version v;
      atomicRead(t, e, v, val);
      item.add_read(v);
      if (Opacity)
	check_opacity(t, e->version());
      // key and val are both only guaranteed until callback returns
      return callback(key, val);//query_callback_overload(key, val, callback);
    };

    range_scanner<decltype(node_callback), decltype(value_callback)> scanner(end, node_callback, value_callback);
    table_.scan(begin, true, scanner, *ti.ti);
  }

  template <typename Callback, typename ValAllocator = DefaultValAllocator>
  void transRQuery(Transaction& t, Str begin, Str end, Callback callback, ValAllocator *va = NULL, threadinfo_type& ti = mythreadinfo) {
    auto node_callback = [&] (leaf_type* node, typename unlocked_cursor_type::nodeversion_value_type version) {
      this->ensureNotFound(t, node, version);
    };
    auto value_callback = [&] (Str key, versioned_value* e) {
      auto item = this->t_read_only_item(t, e);
      // not sure of a better way to do this
      value_type stack_val;
      value_type& val = va ? *(*va)() : stack_val;
#if READ_MY_WRITES
      if (has_delete(item)) {
        return true;
      }
      if (item.has_write()) {
        // read directly from the element if we're inserting it
        if (has_insert(item)) {
	  return range_query_has_insert(callback, key, e, va);
        } else {
	  if (item.flags() & copyvals_bit)
	    return callback(key, item.template write_value<value_type>());
	  else {
	    return callback(key, (value_type&)item.template write_value<void*>());
	  }
        }
      }
#endif
      Version v;
      atomicRead(t, e, v, val);
      item.add_read(v);
      if (Opacity)
	check_opacity(t, e->version());
      return callback(key, val);
    };

    range_scanner<decltype(node_callback), decltype(value_callback), true> scanner(end, node_callback, value_callback);
    table_.rscan(begin, true, scanner, *ti.ti);
  }

#if READ_MY_WRITES
  template <typename Callback, typename ValAllocator>
  // for some reason inlining this/not making it a function gives a 5% slowdown on g++...
  static __attribute__((noinline)) bool range_query_has_insert(Callback callback, Str key, versioned_value *e, ValAllocator *va) {
    value_type stack_val;
    value_type& val = va ? *(*va)() : stack_val;
    assign_val(val, e->read_value());
    return callback(key, val);
  }
#endif

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

  
  static uint64_t tid_from_version(versioned_value* e) {
    return TransactionTid::get_version(e->version());
  }
    
  static bool is_invalid(versioned_value* e) {
    return (e->version() & invalid_bit);
  }
  
  static void make_invalid(versioned_value* e) {
    e->version() |= invalid_bit;
  }
    
  /*
  static bool is_deleted(versioned_value* e) {
    return (e->version() & deleted_bit);
  }
    
  static void make_deleted(versioned_value* e) {
    e->version() |= deleted_bit;
  }*/
    
    static void make_valid(versioned_value* e) {
      assert(is_invalid(e));
      Version cur = e->version();
      cur &= ~invalid_bit;
      e->version() = cur;
    }
    
  static void set_tid(versioned_value* e, uint64_t tid) {
    TransactionTid::set_version(e->version(), tid);
  }
  
  bool check(const TransItem& item, const Transaction& t) {
    if (is_inter(item)) {
      auto n = untag_inter(item.key<leaf_type*>());
      auto cur_version = n->full_version_value();
      auto read_version = item.template read_value<typename unlocked_cursor_type::nodeversion_value_type>();
      return cur_version == read_version;
    }
    auto e = item.key<versioned_value*>();
    auto read_version = item.template read_value<Version>();
    bool valid = validityCheck(item, e);
    if (!valid)
      return false;
#if NOSORT
    bool lockedCheck = true;
#else
    bool lockedCheck = !is_locked(e->version()) || item.has_lock(t);
#endif
    return lockedCheck && versionCheck(read_version, e->version());
  }

  void install(TransItem& item, const Transaction& t) {
    assert(!is_inter(item));
    auto e = item.key<versioned_value*>();
    assert(is_locked(e->version()));
    if (has_delete(item)) {
      if (!has_insert(item)) {
        assert(!(e->version() & invalid_bit));
        e->version() |= invalid_bit;
        //make_deleted(e);
        fence();
        //return;
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
    if (USE_TID_AS_VERSION || Opacity)
      TransactionTid::set_version(e->version(), t.commit_tid());
    else if (has_insert(item)) {
      Version v = e->version() & ~invalid_bit;
      fence();
      e->version() = v;
    } else
      TransactionTid::inc_invalid_version(e->version());
  }

    // Get space required to encode the write data
    uint64_t spaceRequired(TransItem & item) {
      assert(item.has_write());
      uint64_t space_needed = 0;
      space_needed += 1;
      auto e = item.key<versioned_value*>();
      std::string s = (std::string) e->read_key();
      //std::cout<<"Key "<<s.data()<<std::endl;
      const uint32_t k_nbytes = s.size();
      //std::cout<<"Key size "<<k_nbytes<<std::endl;
      space_needed += sizeof(k_nbytes);
      //std::cout<<"bytes Key size "<<sizeof(k_nbytes)<<std::endl;
      space_needed += k_nbytes;
      value_type value;
      if (has_insert(item)) {
        value = e->read_value();
      } else {
        value = item.flags() & copyvals_bit ?
        item.template write_value<value_type>()
        : (value_type&)item.template write_value<void*>();
      }
      
      //std::cout<<"value "<<value<<std::endl;

      uint32_t v_nbytes = sizeof(value);
      if (has_delete(item)){
        v_nbytes = 0;
      }
      //std::cout<<"value size "<<v_nbytes<<std::endl;
      //std::cout<<"bytes value size "<<sizeof(v_nbytes)<<std::endl;

      space_needed += sizeof(v_nbytes);
      if (!has_delete(item))
        space_needed += v_nbytes;
      return space_needed;
    
    }
    
    // Get the log data inorder to do logging
    uint8_t* writeLogData(TransItem & item, uint8_t* p) {
      uint8_t tree_id = (uint8_t) get_tree_id();
      memcpy(p, (char *) &tree_id, 1);
      p += 1;
      
      auto e = item.key<versioned_value*>();
      std::string s = (std::string) e->read_key();
      const uint32_t k_nbytes = s.size();
      uint8_t* p2 = p;
      p = write(p, k_nbytes);
      assert(p-p2 == sizeof(k_nbytes));
      memcpy(p, s.data(), k_nbytes);
       //std::cout<<"Key size while writing "<<k_nbytes<<std::endl;
       //std::cout<<"bytes Key size while writing "<<sizeof(k_nbytes)<<std::endl;
      p += k_nbytes;
      
      value_type value;
      if (has_insert(item)) {
        value = e->read_value();
      } else {
        value = item.flags() & copyvals_bit ?
        item.template write_value<value_type>()
        : (value_type&)item.template write_value<void*>();
      }
      
      uint32_t v_nbytes = sizeof(value);
      if (has_delete(item)){
        v_nbytes = 0;
        //std::cout << "Logging remove key " << s << " from tree " << (int)tree_id << std::endl;
      }
      //std::cout<<"value size while writing"<<v_nbytes<<std::endl;
      //std::cout<<"bytes value size while writing"<<sizeof(v_nbytes)<<std::endl;

      uint8_t* p1 = p;
      p = write(p, v_nbytes);
      assert(p-p1 == sizeof(v_nbytes));
      if (v_nbytes) {
        memcpy(p, &value, v_nbytes);
        int vv = *((int *)p);
        assert(vv == value);
        p += v_nbytes;
      }
      return p;
      
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
#if RCU
    lp.value()->deallocate_rcu(*ti.ti);
#endif
    lp.finish(found ? -1 : 0, *ti.ti);
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
#if RCU
      e->deallocate_rcu(*mythreadinfo.ti);
#endif
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
        item.clear_flags(delete_bit);
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
      // XXX: I'm pretty sure there's a race here-- we should grab this
      // version before we check if the node is valid
      Version v = e->version();
      fence();
      item.add_read(v);
      if (Opacity)
	check_opacity(t, e->version());
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
    if (Opacity) {
      // XXX: would be annoying to do this with node versions
    }
  }

  template <typename NODE, typename VERSION>
  bool updateNodeVersion(Transaction& t, NODE *node, VERSION prev_version, VERSION new_version) {
    if (auto node_item = t.check_item(this, tag_inter(node))) {
      if (node_item->has_read() &&
          prev_version == node_item->template read_value<VERSION>()) {
        node_item->update_read(node_item->template read_value<VERSION>(),
                               new_version);
        return true;
      }
    }
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

  static bool has_insert(const TransItem& item) {
      return item.flags() & insert_bit;
  }
  static bool has_delete(const TransItem& item) {
      return item.flags() & delete_bit;
  }

  static bool validityCheck(const TransItem& item, versioned_value *e) {
    return //likely(has_insert(item)) || !(e->version & invalid_bit);
      likely(!(e->version() & invalid_bit)) || has_insert(item);
  }

  static constexpr Version lock_bit = (Version)1<<(sizeof(Version)*8 - 1);
  static constexpr Version invalid_bit = TransactionTid::user_bit1;
  //(Version)1<<(sizeof(Version)*8 - 2);
  static constexpr Version version_mask = ~(lock_bit|invalid_bit);

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

  static void check_opacity(Transaction& t, Version& v) {
    Version v2 = v;
    fence();
    t.check_opacity(v);
  }

  static bool versionCheck(Version v1, Version v2) {
    return TransactionTid::same_version(v1, v2);
    //return ((v1 ^ v2) & version_mask) == 0;
  }
  static void inc_version(Version& v) {
    assert(0);
    assert(is_locked(v));
    Version cur = v & version_mask;
    cur = (cur+1) & version_mask;
    // set new version and ensure invalid bit is off
    v = (cur | (v & ~version_mask)) & ~invalid_bit;
  }

   static bool is_locked(Version v) {
    return TransactionTid::is_locked(v);
  }
  static void lock(Version *v) {
    TransactionTid::lock(*v);
#if 0
    while (1) {
      Version cur = *v;
      if (!(cur&lock_bit) && bool_cmpxchg(v, cur, cur|lock_bit)) {
        break;
      }
      relax_fence();
    }
#endif
  }
  static void unlock(Version *v) {
    TransactionTid::unlock(*v);
#if 0
    assert(is_locked(*v));
    Version cur = *v;
    cur &= ~lock_bit;
    *v = cur;
#endif
  }

  static void atomicRead(Transaction& t, versioned_value *e, Version& vers, value_type& val) {
    Version v2;
    do {
      v2 = e->version();
      if (is_locked(v2))
	t.abort();
      fence();
      assign_val(val, e->read_value());
      fence();
      vers = e->version();
      fence();
    } while (vers != v2);
  }

  template <typename ValType>
  static void assign_val(ValType& val, const ValType& val_to_assign) {
    val = val_to_assign;
  }
  static void assign_val(std::string& val, Str val_to_assign) {
    val.assign(val_to_assign.data(), val_to_assign.length());
  }
 
  struct table_params : public Masstree::nodeparams<15,15> {
    typedef versioned_value* value_type;
    typedef Masstree::value_print<value_type> value_print_type;
    typedef threadinfo threadinfo_type;
  };
  typedef Masstree::basic_table<table_params> table_type;
  typedef Masstree::unlocked_tcursor<table_params> unlocked_cursor_type;
  typedef Masstree::tcursor<table_params> cursor_type;
  table_type table_;
  uint8_t tree_id;
    
  public:
    typedef Masstree::node_base<table_params> node_base_type;
    typedef Masstree::internode<table_params> internode_type;
    typedef Masstree::leaf<table_params> leaf_type;
    typedef Masstree::leaf<table_params> node_type;
    typedef typename node_base_type::nodeversion_type nodeversion_type;
    typedef Masstree::Str key_type;
    typedef uint64_t key_slice;
    typedef versioned_value versioned_value_type;
    
    
    // Tree walk related algorithms
    
    /**
     * The low level callback interface is as follows:
     *
     * Consider a scan in the range [a, b):
     *   1) on_resp_node() is called at least once per node which
     *      has a responibility range that overlaps with the scan range
     *   2) invoke() is called per <k, v>-pair such that k is in [a, b)
     *
     * The order of calling on_resp_node() and invoke() is up to the implementation.
     */
    class low_level_search_range_callback {
    public:
      virtual ~low_level_search_range_callback() {}
      
      /**
       * This node lies within the search range (at version v)
       */
      virtual void on_resp_node(const node_base_type* n, uint64_t version) = 0;
      
      /**
       * This key/value pair was read from node n @ version
       */
      virtual bool invoke(const key_type &k, versioned_value v, const node_base_type* n, uint64_t version ) = 0;
    };
    
    template <bool Reverse>
    class low_level_search_range_scanner {
    public:
      low_level_search_range_scanner(const Str* upper,
                                     low_level_search_range_callback& callback)
      : boundary_(upper), boundary_compar_(false), callback_(callback) {}
      
      template <typename ITER, typename KEY>
      void check(const ITER& iter,
                 const KEY& key) {
        int min = std::min(boundary_->length(), key.prefix_length());
        int cmp = memcmp(boundary_->data(), key.full_string().data(), min);
        if (!Reverse) {
          if (cmp < 0 || (cmp == 0 && boundary_->length() <= key.prefix_length()))
            boundary_compar_ = true;
          else if (cmp == 0) {
            uint64_t last_ikey = iter.node()->ikey0_[iter.permutation()[iter.permutation().size() - 1]];
            uint64_t slice = string_slice<uint64_t>::make_comparable(boundary_->data() + key.prefix_length(), std::min(boundary_->length() - key.prefix_length(), 8));
            boundary_compar_ = slice <= last_ikey;
          }
        } else {
          if (cmp >= 0)
            boundary_compar_ = true;
        }
      }
  
      void visit_leaf(const Masstree::scanstackelt<table_params>& iter,
                      const Masstree::key<uint64_t>& key, threadinfo&) {
        this->n_ = iter.node();
        this->v_ = iter.full_version_value();
        callback_.on_resp_node(this->n_, this->v_);
        if (this->boundary_)
          this->check(iter, key);
      }
      
      bool visit_value(const Masstree::key<uint64_t>& key,
                       versioned_value* value, threadinfo&) {
        if (this->boundary_compar_) {
          lcdf::Str bs(this->boundary_->data(), this->boundary_->length());
          if ((!Reverse && bs <= key.full_string()) ||
              ( Reverse && bs >= key.full_string()))
            return false;
        }
        return callback_.invoke(key.full_string(), *value, this->n_, this->v_);
      }
    private:
      const Str* boundary_;
      bool boundary_compar_;
      Masstree::leaf<table_params>* n_;
      uint64_t v_;
      low_level_search_range_callback& callback_;
    };
    
    /**
     * For all keys in [lower, *upper), invoke callback in ascending order.
     * If upper is NULL, then there is no upper bound
     *
     
     * This function by default provides a weakly consistent view of the b-tree. For
     * instance, consider the following tree, where n = 3 is the max number of
     * keys in a node:
     *
     *              [D|G]
     *             /  |  \
     *            /   |   \
     *           /    |    \
     *          /     |     \
     *   [A|B|C]<->[D|E|F]<->[G|H|I]
     *
     * Suppose we want to scan [A, inf), so we traverse to the leftmost leaf node
     * and start a left-to-right walk. Suppose we have emitted keys A, B, and C,
     * and we are now just about to scan the middle leaf node.  Now suppose
     * another thread concurrently does delete(A), followed by a delete(H).  Now
     * the scaning thread resumes and emits keys D, E, F, G, and I, omitting H
     * because H was deleted. This is an inconsistent view of the b-tree, since
     * the scanning thread has observed the deletion of H but did not observe the
     * deletion of A, but we know that delete(A) happens before delete(H).
     *
     * The weakly consistent guarantee provided is the following: all keys
     * which, at the time of invocation, are known to exist in the btree
     * will be discovered on a scan (provided the key falls within the scan's range),
     * and provided there are no concurrent modifications/removals of that key
     *
     * Note that scans within a single node are consistent
     *
     * XXX: add other modes which provide better consistency:
     * A) locking mode
     * B) optimistic validation mode
     *
     * the last string parameter is an optional string buffer to use:
     * if null, a stack allocated string will be used. if not null, must
     * ensure:
     *   A) buf->empty() at the beginning
     *   B) no concurrent mutation of string
     * note that string contents upon return are arbitrary
     */
    void
    search_range_call(const key_type &lower,
                      const key_type *upper,
                      low_level_search_range_callback &callback,
                      std::string *buf = nullptr) const {
      low_level_search_range_scanner<false> scanner(upper, callback);
      threadinfo ti;
      table_.scan(lcdf::Str(lower.data(), lower.length()), true, scanner, ti);
    }
    
    // (lower, upper]
    void
    rsearch_range_call(const key_type &upper,
                       const key_type *lower,
                       low_level_search_range_callback &callback,
                       std::string *buf = nullptr) const;
    
    
    class search_range_callback : public low_level_search_range_callback {
    public:
      virtual void on_resp_node(const node_type* n, uint64_t version) {}
      virtual bool invoke (const Str &k, versioned_value v, const node_type* n, uint64_t version) {
        return invoke(k, v);
      }
      virtual bool invoke(const Str &k, versioned_value v) = 0;
    };
    
    /**
     * [lower, *upper)
     *
     * Callback is expected to implement bool operator()(key_slice k, value_type v),
     * where the callback returns true if it wants to keep going, false otherwise
     */
    template <typename F>
    inline void search_range(const key_type &lower,
                 const key_type *upper,
                 F& callback,
                 std::string *buf = nullptr) const;
    
    /**
     * (*lower, upper]
     *
     * Callback is expected to implement bool operator()(key_slice k, value_type v),
     * where the callback returns true if it wants to keep going, false otherwise
     */
    template <typename F>
    inline void rsearch_range(const key_type &upper,
                  const key_type *lower,
                  F& callback,
                  std::string *buf = nullptr) const;

};
    
typedef MassTrans<int> concurrent_btree;

template <typename V, typename Box, bool Opacity>
__thread typename MassTrans<V, Box, Opacity>::threadinfo_type MassTrans<V, Box, Opacity>::mythreadinfo;

template <typename V, typename Box, bool Opacity>
constexpr typename MassTrans<V, Box, Opacity>::Version MassTrans<V, Box, Opacity>::invalid_bit;

