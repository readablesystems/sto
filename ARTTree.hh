#pragma once

#include "ARTNode256.hh"
#include "ARTNode48.hh"
#include "ARTNode16.hh"
#include "ARTNode4.hh"
#include "Interface.hh"
#include "TWrapped.hh"

template <typename V>
class ARTTree : public TObject {
  public:
    ARTTree() {
      root_ = new ARTNode256<V>(this);
    }

    inline bool lookup(std::string key, V& retval) {
      return root_->lookup(key, retval);
    }

    inline void update(std::string key, V value) {
      root_->update(key, value);
    }

    inline void insert(std::string key, V value) {
      root_->insert(key, value);
    }

    inline void remove(std::string key) {
      root_->remove(key);
    }

  private:

    // Transactional methods

  bool lock(TransItem& item, Transaction& txn) override {
    return txn.try_lock(item, item.key<ARTRecord<V>*>()->vers_);
  }

  bool check(TransItem& item, Transaction&) override {
    
    // Case in which the item in the read set is a record.
    if ((item.key<uintptr_t>() & (uint64_t)(1)) == (uint64_t)0) {
      ARTRecord<V>* record = item.key<ARTRecord<V>*>();
      bool ok = item.check_version(record->vers_);
      bool isDeleted = !(record->is_deleted());

      // printf("In check! ok is %d, and isDeleted is %d. We want both to be true\n", ok, isDeleted);
      
      return (ok && isDeleted);
    }
    
    // Case where item is the leaf node following the path of a record that was
    // searched for but not found.
    else {
      // printf("In check for nodes! Saved header is %x, current header is %x\n", item.read_value<uint64_t>(),
      //       ((ARTNode<V>*)(item.key<uintptr_t>() - (uint64_t)1))->get_header());
      return item.read_value<uint64_t>() ==
              ((ARTNode<V>*)(item.key<uintptr_t>() - (uint64_t)1))->get_header();
    }
  }
  
  void install(TransItem& item, Transaction& txn) override {
    ARTRecord<V>* record = item.key<ARTRecord<V>*>();
    
    if (item.flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT) {
      // Setting delete bit here so that transaction lock can be released
      // earlier. Actually deleting the node occurs when cleanup is called.
      record->set_deleted_bit(true);
    } 

    record->val_.write(std::move(item.template write_value<V>()));
    // Note that the flags are not saved here! That's why you don't
    // need to reset the version number here.
    txn.set_version_unlock(record->vers_, item);
  }
  
  void unlock(TransItem& item) override {
    item.key<ARTRecord<V>*>()->vers_.unlock();
  }

  void cleanup(TransItem& item, bool committed) override {
    ARTRecord<V>* record = item.key<ARTRecord<V>*>();

    // Clear the entry if we have a delete that committed or an insert
    // that aborted.
    if (item.flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT && committed) {
      root_->cleanup(record->key_);
    } else if (item.flags() & TransItem::user0_bit && !committed) {
      root_->cleanup(record->key_);
    }
  }

  private:
  ARTNode<V>* root_;
};


