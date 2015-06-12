#include <utility>
#include "MassTrans.hh"
#include "local_vector.hh"

#define MAX_THREADS_ 32
#define INIT_SET_SIZE_ 512

template <bool Opacity = false>
class MassTrans_nd : public MassTrans<void*, versioned_value_struct<void*>, Opacity> {
  typedef MassTrans<void*, versioned_value_struct<void*>, Opacity> supertype;
  typedef typename supertype::leaf_type leaf_type;
  typedef typename supertype::versioned_value versioned_value;
  typedef typename supertype::unlocked_cursor_type unlocked_cursor_type;
  typedef typename supertype::cursor_type cursor_type;
  typedef typename supertype::value_type value_type;
  typedef typename supertype::Version Version;
  typedef typename supertype::Str Str;
  typedef typename supertype::threadinfo_type threadinfo_type;

struct LocalItem {
  std::string key;
  void* value;
};

typedef local_vector<LocalItem, INIT_SET_SIZE_> LocalItemsType;

public:
  MassTrans_nd(): supertype() { }

  struct oput_entry {
    uint64_t order;
    std::string value;
  };
  
  template <typename StringType>
  void ndtransMax(Transaction& t, StringType key, uint64_t value) {
    LocalItem it = item(key, localItemsArray[Transaction::threadid]; 
    uint64_t* value_ptr = new uint64_t;
    *value_ptr = value;  
    ndtransPut(t, key, value_ptr, max_bit, this->mythreadinfo);
  }

  template <typename StringType>
  void ndtransAdd(Transaction& t, StringType key, int value) {
    int* value_ptr = new int;
    *value_ptr = value;  
    ndtransPut(t, key, value_ptr, add_bit, this->mythreadinfo); // TODO: what if the key doesn't already exist
  }
  
  template <typename StringType>
  void ndtransOPut(Transaction& t, StringType key, uint64_t order, const std::string& value) {
    oput_entry* ent_ = new oput_entry;
    ent_->order = order; 
    ent_->value = value;
    ndtransPut(t, key, ent_, oput_bit, this->mythreadinfo);
  }
  
  template <typename ValType>
  bool ndtransGet(Transaction& t, Str key, ValType& retval) { 
    return ndtransRead(t, key, retval, this->mythreadinfo);
  }

  template <typename ValType>
  bool ndtransRead(Transaction& t, Str key, ValType& retval, threadinfo_type& ti) {
    unlocked_cursor_type lp(this->table_, key);
    bool found = lp.find_unlocked(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      //      __builtin_prefetch(&e->version);
      auto item = this->t_read_only_item(t, e);
      item.add_flags(ndread_bit);
      if (!this->validityCheck(item, e)) {
        t.abort();
        return false;
      }
      //      __builtin_prefetch();
      //__builtin_prefetch(e->value.data() - sizeof(std::string::size_type)*3);
#if READ_MY_WRITES
      if (this->has_delete(item)) {
        return false;
      }
      if (item.has_write()) {
        // read directly from the element if we're inserting it
        if (this->has_insert(item)) {
	  this->assign_val(retval, e->read_value());
        } else {
	  if (item.flags() & this->copyvals_bit)
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
      read(t, e, elem_vers, retval); // No need to do atomic read here
      item.add_read(elem_vers);
      if (Opacity)
	this->check_opacity(t, e->version());
    } else {
      this->ensureNotFound(t, lp.node(), lp.full_version_value());
    }
    return found;
  }


  template <bool CopyVals = true, bool INSERT = true, bool SET = true, typename StringType>
  bool ndtransPut(Transaction &t, StringType& key, const value_type& value, uint64_t op_bit,  threadinfo_type& ti) {
    // optimization to do an unlocked lookup first
    if (SET) {
      unlocked_cursor_type lp(this->table_, key);
      bool found = lp.find_unlocked(*ti.ti);
      if (found) {
        return handleNdPutFound<CopyVals, INSERT, SET>(t, lp.value(), key, value, op_bit);
      } else {
        if (!INSERT) {
          this->ensureNotFound(t, lp.node(), lp.full_version_value());
          return false;
        }
      }
    }

    cursor_type lp(this->table_, key);
    bool found = lp.find_insert(*ti.ti);
    if (found) {
      versioned_value *e = lp.value();
      lp.finish(0, *ti.ti);
      return handleNdPutFound<CopyVals, INSERT, SET>(t, e, key, value, op_bit);
    } else {
      //      auto p = ti.ti->allocate(sizeof(versioned_value), memtag_value);
      versioned_value* val = (versioned_value*)versioned_value::make(value, this->invalid_bit);
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

      if (this->updateNodeVersion(t, orig_node, orig_version, upd_version)) {
        // add any new nodes as a result of splits, etc. to the read/absent set
#if !ABORT_ON_WRITE_READ_CONFLICT
        for (auto&& pair : lp.new_nodes()) {
          t.new_item(this, this->tag_inter(pair.first)).add_read(pair.second);
        }
#endif
      }
      auto item = t.new_item(this, val);
      if (std::is_same<const std::string, const StringType>::value) {
	if (CopyVals)
	  item.add_write(key).add_flags(this->copyvals_bit);
	else
	  item.add_write(this->pack(key));
      } else
        // force a copy
	// technically a user could request CopyVals=false and pass non-std::string keys
	// but we don't support that
        item.add_write(std::string(key)).add_flags(this->copyvals_bit);

      item.add_flags(this->insert_bit).add_flags(op_bit);
      return found;
    }
  }

private:

  static void read(Transaction& t, versioned_value *e, Version& vers, value_type& val) {
    vers = e->version(); 
    supertype::assign_val(val, e->read_value());
  }

  // called once we've checked our own writes for a found put()
  template <bool CopyVals = true>
  void reallyHandleNdPutFound(Transaction& t, TransProxy& item, versioned_value *e, Str key, const value_type& value, uint64_t op_bit) {
    // resizing takes a lot of effort, so we first check if we'll need to
    // (values never shrink in size, so if we don't need to resize, we'll never need to)
    auto *new_location = e;
    bool needsResize = e->needsResize(value);
    if (needsResize) {
      if (!this->has_insert(item)) {
        // TODO: might be faster to do this part at commit time but easiest to just do it now
        this->lock(e);
        // we had a weird race condition and now this element is gone. just abort at this point
        if (e->version() & this->invalid_bit) {
          this->unlock(e);
          t.abort();
          return;
        }
        e->version() |= this->invalid_bit;
        // should be ok to unlock now because any attempted writes will be forced to abort
        this->unlock(e);
      }
      // does the actual realloc. at this point e is marked invalid so we don't have to worry about
      // other threads changing e's value
      new_location = e->resizeIfNeeded(value);
      // e can't get bigger so this should always be true
      assert(new_location != e);
      if (!this->has_insert(item)) {
        // copied version is going to be invalid because we just had to mark e invalid
        new_location->version() &= ~this->invalid_bit;
      }
      cursor_type lp(this->table_, key);
      // TODO: not even trying to pass around threadinfo here
      bool found = lp.find_locked(*(this->mythreadinfo.ti));
      (void)found;
      assert(found);
      lp.value() = new_location;
      lp.finish(0, *(this->mythreadinfo.ti));
      // now rcu free "e"
      e->deallocate_rcu(*(this->mythreadinfo.ti));
    }
#if READ_MY_WRITES
    if (this->has_insert(item)) {
      new_location->set_value(value);
    } else
#endif
    {
      if (new_location == e) {
	if (CopyVals)
	  item.add_write(value).add_flags(this->copyvals_bit).add_flags(op_bit);
	else
	  item.add_write(this->pack(value)).add_flags(op_bit);
      } else {
	if (CopyVals)
	  t.new_item(this, new_location).add_write(value).add_flags(this->copyvals_bit).add_flags(op_bit);
	else
	  t.new_item(this, new_location).add_write(this->pack(value)).add_flags(op_bit);
      }
    }
  }

  // returns true if already in tree, false otherwise
  // handles a transactional put when the given key is already in the tree
  template <bool CopyVals = true, bool INSERT=true, bool SET=true>
  bool handleNdPutFound(Transaction& t, versioned_value *e, Str key, const value_type& value, uint64_t op_bit) {
    auto item = this->t_item(t, e);
    if (!this->validityCheck(item, e)) {
      t.abort();
      return false;
    }
#if READ_MY_WRITES
    if (this->has_delete(item)) {
      // delete-then-insert == update (technically v# would get set to 0, but this doesn't matter
      // if user can't read v#)
      if (INSERT) {
        item.clear_flags(this->delete_bit);
        assert(!this->has_delete(item));
        reallyHandleNdPutFound<CopyVals>(t, item, e, key, value, op_bit);
      } else {
        // delete-then-update == not found
        // delete will check for other deletes so we don't need to re-log that check
      }
      return false;
    }
    // make sure this item doesn't get deleted (we don't care about other updates to it though)
    if (!item.has_read() && !this->has_insert(item))
#endif
    {
      // XXX: I'm pretty sure there's a race here-- we should grab this
      // version before we check if the node is valid
      Version v = e->version();
      fence();
      item.add_read(v);
      if (Opacity)
	this->check_opacity(t, e->version());
    }
    if (SET) {
      reallyHandleNdPutFound<CopyVals>(t, item, e, key, value, op_bit);
    }
    return true;
  }

public:
  bool check(const TransItem& item, const Transaction& t) {

    bool ndmax = item.flags() & max_bit;
    bool ndadd = item.flags() & add_bit;
    bool ndoput = item.flags() & oput_bit;    
    bool ndread = item.flags() & ndread_bit;
    if (ndmax || ndadd || ndoput || ndread) return true;

    if (this->is_inter(item)) {
      auto n = this->untag_inter(item.key<leaf_type*>());
      auto cur_version = n->full_version_value();
      auto read_version = item.template read_value<typename unlocked_cursor_type::nodeversion_value_type>();
      return cur_version == read_version;
    }
    auto e = item.key<versioned_value*>();
    auto read_version = item.template read_value<Version>();
    bool valid = this->validityCheck(item, e);
    if (!valid)
      return false;
#if NOSORT
    bool lockedCheck = true;
#else
    bool lockedCheck = !this->is_locked(e->version()) || item.has_lock(t);
#endif
    return lockedCheck && this->versionCheck(read_version, e->version());
  }

  void install(TransItem& item, const Transaction& t) {
    assert(!this->is_inter(item));
    auto e = item.key<versioned_value*>();
    assert(this->is_locked(e->version()));
   
    bool ndmax = item.flags() & max_bit;
    bool ndadd = item.flags() & add_bit;
    bool ndoput = item.flags() & oput_bit;
    
    if (ndmax || ndadd || ndoput) {
      assert(!this->has_delete(item));
    
      value_type prevVal = e->read_value();
      value_type& v = item.flags() & this->copyvals_bit ?
        item.template write_value<value_type>()
        : (value_type&)item.template write_value<void*>();
    
      if (this->has_insert(item)) {
        Version v = e->version() & ~this->invalid_bit;
        fence();
        e->version() = v;
        return;
      }
      bool updated = false;
      
      if (ndmax) {
        if (* ((uint64_t*) v) > *((uint64_t*)prevVal)) {
          e->set_value(v);
          updated = true;
        }
      }
     
      if (ndadd) {
        uint64_t new_value = *(((uint64_t*)prevVal) + *((int*)v));
        e->set_value(&new_value);
        updated = true;
      }

      if (ndoput) {
        oput_entry* prev_entry = (oput_entry*) (prevVal);
        oput_entry* new_entry = (oput_entry*) (v);
	if (new_entry->order > prev_entry->order) {
          e->set_value(v);
          updated = true;
        }
     
      }
     
      if (updated) {
        if (Opacity)
          TransactionTid::set_version(e->version(), t.commit_tid());
        else 
          TransactionTid::inc_invalid_version(e->version());
      }
      return;

    }

    if (this ->has_delete(item)) {
      if (!this->has_insert(item)) {
        assert(!(e->version() & this->invalid_bit));
        e->version() |= this->invalid_bit;
        fence();
      }
      // TODO: hashtable did this in afterC, we're doing this now, unclear really which is better
      // (if we do it now, we take more time while holding other locks, if we wait, we make other transactions abort more
      // from looking up an invalid node)
      auto &s = item.flags() & this->copyvals_bit ? 
	item.template write_value<std::string>()
	: (std::string&)item.template write_value<void*>();
      bool success = this->remove(Str(s));
      // no one should be able to remove since we hold the lock
      (void)success;
      assert(success);
      return;
    }
    if (!this->has_insert(item)) {
      value_type& v = item.flags() & this->copyvals_bit ?
	item.template write_value<value_type>()
	: (value_type&)item.template write_value<void*>();
      e->set_value(v);
    }
    if (Opacity)
      TransactionTid::set_version(e->version(), t.commit_tid());
    else if (this->has_insert(item)) {
      Version v = e->version() & ~this->invalid_bit;
      fence();
      e->version() = v;
    } else
      TransactionTid::inc_invalid_version(e->version());

  }

  template <typename T> LocalItem item(T key, LocalItemsType& localSet) {
      LocalItem* ti = find_item(key);
      if (!ti) {
          localSet.emplace_back(key);
          ti = &localSet.back();
      }
      return *ti;
  }

  LocalItem* find_item(void* key, LocalItemsType& localSet) {
    for (auto it = localSet.begin(); it != localSet.end(); ++it) {
      if (it->key == key) return &*it;
    }
    return NULL;
  }

  static constexpr TransItem::flags_type max_bit = TransItem::user0_bit<<3;
  static constexpr TransItem::flags_type add_bit = TransItem::user0_bit<<4;
  static constexpr TransItem::flags_type oput_bit = TransItem::user0_bit<<5;
  static constexpr TransItem::flags_type ndread_bit = TransItem::user0_bit << 6;

private:
  LocalItemsType localItemsArray[MAX_THREADS_];

};

