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
      root_->remove(key, true);
    }

  private:

    // transactional methods

  bool lock(TransItem& item, Transaction& txn) override {
    return txn.try_lock(item, item.key<ARTRecord<V>*>()->vers_);
  }

  bool check(TransItem& item, Transaction&) override {

    // Case in which the item in the read set is a record.
    if ((item.key<uintptr_t>() & (uint64_t)(1)) == (uint64_t)0) {
      ARTRecord<V>* record = item.key<ARTRecord<V>*>();
      bool ok = item.check_version(record->vers_);
      bool isDeleted = !(record->is_deleted());
      printf("Calling Check: ok is: %d, isDeleted is: %d\n", ok, isDeleted);
      return (ok && isDeleted);
    }
    // Case where item is the leaf node following the path of a record that was
    // searched for but not found.
    else {
      printf("Sto read value: %lx, Current header: %lx\n",
             item.read_value<uint64_t>(),
             ((ARTNode<V>*)(item.key<uintptr_t>() - (uint64_t)1))->get_header());
      
      return item.read_value<uint64_t>() ==
              ((ARTNode<V>*)(item.key<uintptr_t>() - (uint64_t)1))->get_header();
    }
  }
  
  void install(TransItem& item, Transaction& txn) override {
    ARTRecord<V>* record = item.key<ARTRecord<V>*>();
    
    if (item.flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT) {
      // Setting delete bit here so that transaction lock can be released
      // earlier. Actually deleting the node occurs when cleanup is called.
      // std::cout << "Record with key " << record->key_ << " and value " << record->read() << std::endl;
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
    
    if (item.flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT && committed) {
        // Actually remove the node from the tree
      root_->remove(record->key_, false);
    } else if (item.flags() & TransItem::user0_bit && !committed) {
      // TODO: Have remove the parent ARTNode if it only contains
      // this record and not anything. Recursively remove if the same
      // is true for the parents of those parents?
      root_->remove(record->key_, false);
    }
  }

  // TODO: override print?
  
  private:
  ARTNode<V>* root_;
};


