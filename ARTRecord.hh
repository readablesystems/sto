#pragma once
#include <iostream>
#include <cassert>

#include "Interface.hh"
#include "TWrapped.hh"

template <typename V> class ARTTree;
static TransItem::flags_type ART_RECORD_TRANS_ITEM_DELETE_BIT = TransItem::user0_bit << 1;

// ARTRecords are values associated with the keys in the ART.
// Currently, ARTRecords are not an ARTNode.
template <typename V, typename W = TNonopaqueWrapped<V>>
  class ARTRecord {
  public:
    
    typedef TNonopaqueVersion Version_type;
    
    ARTRecord(V val, ARTTree<V>* tree, std::string key) :
      val_(val), tree_(tree), key_(key), vers_(Sto::initialized_tid() | TransactionTid::user_bit), status_bits(), creation_version_number(vers_) { // val_(val) {

      // Setting the inserted bit in Sto
      Sto::item(tree_, this).add_flags(TransItem::user0_bit);
      Sto::item(tree_,this).add_write(val);
    }

    V read() {
      if ((vers_.value() & TransactionTid::user_bit) &&
          !(Sto::item(tree_, this).flags() & TransItem::user0_bit)) {
        std::cout << "[" << TThread::id() << "]" <<
          "On read, found a newly record newly inserted from another transaction. Aborting. Record key is " << key_ << std::endl;
        Sto::abort();
      }
      
      // Enforces READ MY WRITES
      auto item = Sto::item(tree_, this);
      
      if (item.has_write()) {
        return item.template write_value<V>();
      } else {
        return val_.read(item, vers_);
      }
    
      // return val_;
    }
    
    void update(const V& value) {
      if ((vers_.value() & TransactionTid::user_bit) &&
          !(Sto::item(tree_, this).flags() & TransItem::user0_bit)) {
        std::cout << "[" << TThread::id() << "]" <<
          "On update, found a newly record newly inserted from another transaction. " <<
          "Aborting. Record key is " << key_ << std::endl;
        Sto::abort();
      }

      if (this->is_deleted()) {
        std::cout << "We found a case where the value was set for deletion but "
                  << "wasn\'t deleted yet, and we're updating this value!" <<
          std::endl;
        Sto::abort();
      }
      
      // Non-blind write
      auto item = Sto::item(tree_, this);
      item.add_write(value);
      item.observe(vers_);
      
      // val_ = value;
    }

    // Lets Sto know that the record will be deleted upon commit.
    void set_deleted() {
      if (Sto::item(tree_, this).flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT) {
        std::cout << "Delete bit already set. Should I do something here?" <<
          std::endl;
      }
      Sto::item(tree_, this).add_flags(ART_RECORD_TRANS_ITEM_DELETE_BIT);
      Sto::item(tree_,this).add_write(val_.access());
    }

    // Only for inserts called on records that were marked by Sto as deleted but still
    // in the same transaction as the call to 'delete'.
    void set_undeleted() {
      Sto::item(tree_, this).clear_flags(ART_RECORD_TRANS_ITEM_DELETE_BIT);
      Sto::item(tree_,this).add_write(val_.access());
    }

    void set_version(Version_type version) {
      vers_.set_version(version);
    }

    Version_type version() {
      return vers_;
    }

    // Sets the status bit for deleted in the ARTRecord
    // to value 'on'. Assumes the bit was not already set
    // to that value.
    void set_deleted_bit(bool on) {
      if (on) {
        //assert(!is_deleted() && "Delete bit already set");
        status_bits |= 1;
      } else {
        //assert(is_deleted() && "Delete bit not set");
        // TODO: here we're setting the inserted bit to false as well. Is this
        // fine?
        // status_bits &= 0;
        status_bits &= ~(1ul);
      }
    }

    bool is_deleted() {
      return (status_bits & 1) == 1;
    }

    // TODO: I think I should delete this bit of code
    bool is_normal() {
      return !(is_deleted());
    }


    // TODO: there's an issue here in that anyone can access these
    // variables. Look for a way to make these variables more private
    // while still allowing them to be accessed in the Sto methods
    // defined in ARTTree.
  public:
    W val_;
    // V val_;
    ARTTree<V>* tree_;
    std::string key_;
    Version_type vers_;

    // A byte representing the status bits of the record.
    // For now, only 2 bits are defined as follows:
    // 8                           2          1        0
    // -------------------------------------------------
    // |         undefined         | inserted | deleted |
    // -------------------------------------------------
    char status_bits;
    Version_type creation_version_number;
  };
    

   
