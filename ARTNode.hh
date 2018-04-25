#pragma once
#include <cstdint>
#include <iostream>
#include <cassert>
#include <string>
#include <vector>

#include "ARTRecord.hh"
#include "Interface.hh"
#include "compiler.hh"

// TODO: Do I need all of these?
template <typename V> class ARTNode256;
template <typename V> class ARTNode48;
template <typename V> class ARTNode16;
template <typename V> class ARTNode4;
template <typename V> class ARTTree;

static const int NUM_STATUS_BITS = 3;
static const int EMPTY = -1;

// The null terminator is the position in any node where a record can be held.
// We use the terminator so that there are no possible nodes in the subtree with
// a record at its root.
static const unsigned char RECORD_BYTE = '\0';

struct byte_pointer_pair {
  unsigned char byte;
  void* node_pointer;
};

template <typename V>
class ARTNode {
public:
  typedef uint64_t header_type;

  ARTNode(ARTTree<V>* tree) : header_(), is_leaf_(true), tree_(tree) {}
  
  virtual ~ARTNode<V>() {}
  
  // User methods

  // Finds the record associated with a specific key.
  // Returns false if the record was not found. retval does not change value.
  // Returns true if the record was found. retval takes on the value found in the record.
  bool lookup(std::string key, V& retval) {
    bool success;
    // find_parent_node(key, retval, success, this->get_version_number());
    find_parent_node(key, retval, success, this->get_header());
    
    if (!success) {
      // TODO: add header of the leaf node to the read set.
      // Consider giving the pointer to the leaf node to the TItem, then to mark
      // it as a leaf node by changing the least significant bit to 1, then check
      // in the check to determine which type of node it is, then check
      // the leaf node's version number appropriately.
      // In the check method, access the value of header at execution by calling
      // item.read_value<header_type>()
      // Resolves phantom problem.
    }
    
    return success;
  }

  // Updates the record associated with a key to value.
  // If a record did not previously exist, creates a new record with the value.
  void update(std::string key, const V& value) {
    ARTRecord<V>* record;
    bool created_new_record = false;
    find_parent_node_insert(key, value, record, created_new_record);
    if (!created_new_record) {
      record->update(value);
    }
  }

  // Inserts a record with value value into the tree by following the path given by key.
  // Creates new nodes in the tree as needed (lazy expansion not yet supported).
  void insert(std::string key, const V& value) {
    update(key, value);
  }


  // Removes the record associated with a key, if it exists.
  void remove(std::string key, bool sto) {
    ARTNode<V>* parent_node = find_parent_node(key, this->get_header());
    if (parent_node == NULL) {
      return;
    }

    // Spinlock
    while(true) {
      if (parent_node->try_lock())
        break;
    }

    header_type saved_header = parent_node->get_header_no_lock() & (~1ul);
    ARTRecord<V>* record = parent_node->find_record();

    // Case where record wasn't found.
    if (record == NULL) {
      Sto::item(tree_, (ARTNode<V>*)((uintptr_t)parent_node + (uint64_t)1)).
        add_read(/*current_version_number*/ saved_header);

      parent_node->unlock();
      return;
    }

    // Check that the bit set for newly inserted nodes in a separate transaction
    // isn't set. If it is, we must abort since we're trying to delete a node
    // that shouldn't be observed by this transaction.
    if ((record->vers_.value() & TransactionTid::user_bit) &&
        !(Sto::item(tree_, record).flags() & TransItem::user0_bit)) {
      std::cout << "Found a newly record newly inserted from another " <<
        "transaction. Aborting" << std::endl;
      // Making sure to unlock the nodes so other transactions can access it.
      parent_node->unlock();
      Sto::abort();
    }

    
    // Tell Sto to remove the record instead of actually removing the
    // node.

    if (sto) {
      record->set_deleted();
    } else {
      parent_node->remove_record();
    }
    
    if(parent_node->must_shrink()) {
      // TODO: handle the shrinking of the node.
    }
    
    // Should I keep this here? How can I better
    // keep track of whether a node is a leaf or not?
    if(parent_node->is_empty()) { 
      parent_node->is_leaf_ = true;
    }
    parent_node->update_version_number();
    // std::cout << "updated version number" << std::endl;
    parent_node->unlock();
  }

  void print_info_recursive() {
    this->print_info();
    std::vector<void *>* children = get_children();
    for(int i = 0; i < children->size(); i++) {
      // TODO: this method is wack
      ((ARTNode256<V>*)children->at(i))->print_info_recursive();
      //((ARTNode48<V>*)children->at(i))->print_info_recursive();
      //((ARTNode16<V>*)children->at(i))->print_info_recursive();
      //((ARTNode4<V>*)children->at(i))->print_info_recursive();
    }
  }

  // range() {}

  // static methods

  static inline void lock(ARTNode<V>* node) {
    while (true) {
      if(node->try_lock())
        break;
    }
  }

  static inline void unlock(ARTNode<V>* node) {
    node->unlock();
  }

  // Returns the current version number.
  inline uint64_t get_version_number() {
    return header_ >> NUM_STATUS_BITS;
  }

  // Returns the whole header, waiting until the current_header doesn't have its
  // lock bit set to return.
  inline uint64_t get_header() {
    // std::cout << "Header: " << std::hex << header_ << std::endl;
    // printf("[%d] Header: %lx\n", TThread::id(), header_);
    while (true) {
      header_type current_header = header_;
      if (current_header & 1) {
        continue;
      }
      return current_header;
    }

  }

  // Returns the whole header of a node. Doesn't wait for unlock. This is used
  // in cases where the node is already locked and we wish to extract the header
  // of that node.
  inline uint64_t get_header_no_lock() {
    return header_;
  }

  // virtual public methods

  // Prints the node's type, version number, whether it's a leaf, and children.
  virtual void print_info() {
    std::cout <<
      "Node Address: " << this << "\n" <<
      "Node Type: ARTNode256\n" <<
      "Version Number: " << this->get_version_number() << "\n" <<
      "Is Leaf: " << this->is_leaf() << "\n" <<
      "Children:" << std::endl;
  };

private:
  
  // Helper methods

  // Traverses the ART to find the node that matches the key.
  // If path finds nonexisting nodes, or if parent node is
  // not found, returns null. Note that this method should
  // take care of lazy expansion (though not yet!).
  // Also takes care of reading value from record found, retrying if needed.
  ARTNode<V>* find_parent_node(std::string key, V& retval, bool& success,
                               /*uint64_t current_version_number,*/
                               uint64_t current_header,
                               uint64_t depth = 0) {
    if (this->is_invalid()) {
      // Invalid check will return NULL, making parent be NULL.
      // However, the loop will retry because (at least as of now)
      // next is invalid and will never be revalidated.
      return NULL;
    }
    
    unsigned char next_byte = key[depth];
    if (depth == key.length()) {
      success = lookup_and_read_record(retval, current_header);
      return this;
    }
    ARTNode<V>* parent = NULL;
    ARTNode<V>* next;
    do {
      next = find_child(next_byte);

      // Check that the header has not changed when going down an extra level in
      // traversal. If so, means an update or remove  happened during that time,
      // so you must retry.
      if (current_header != header_) {
        current_header = this->get_header();
        continue;
      }
      
      if (next == NULL) {
        success = false;

        // Observe the version number of the lowest node in the path in STO to
        // add to the read set. We manually add one to the pointer value make
        // the least significant bit different from that of records in the read
        // set, since all pointers have their least significant bit set to 0.
        Sto::item(tree_, (ARTNode<V>*)((uintptr_t)this + (uint64_t)1)).
          add_read(/*current_version_number*/ current_header);
        return NULL;
      }
      // Observe the version number of the parent
      // current_version_number = next->get_version_number();
      current_header = next->get_header();

      // while(next->is_locked());
      
      parent = next->find_parent_node(key, retval, success,
                                      /*current_version_number*/ current_header,
                                      depth + 1);
    } while(next->is_invalid());
    return parent;
  }

  // Traverses the ART to find the node that matches the key.
  // If path finds nonexisting nodes, or if parent node is
  // not found, returns null. Note that this method should
  // take care of lazy expansion (though not yet!).
  ARTNode<V>* find_parent_node(std::string key, uint64_t current_header,
                               uint64_t depth = 0) {
    if (this->is_invalid()) {
      // Invalid check will return NULL, making parent be NULL.
      // However, the loop will retry because (at least as of now)
      // next is invalid and will never be revalidated.
      return NULL;
    }
    
    unsigned char next_byte = key[depth];
    if (depth == key.length()) {
      return this;
    }
    ARTNode<V>* parent = NULL;
    ARTNode<V>* next;
    do {
      next = find_child(next_byte);

      // Check that the header has not changed when going down an extra level in
      // traversal. If so, means an update or remove  happened during that time,
      // so you must retry.
      if (current_header != header_) {
        current_header = this->get_header();
        continue;
      }

      if (next == NULL) {
        // Observe the version number of the lowest node in the path in STO to
        // add to the read set. We manually add one to the pointer value make
        // the least significant bit different from that of records in the read
        // set, since all pointers have their least significant bit set to 0.
        Sto::item(tree_, (ARTNode<V>*)((uintptr_t)this + (uint64_t)1)).
          add_read(/*current_version_number*/ current_header);
        return NULL;
      }
      // Observe the version number of the parent
      current_header = next->get_header();

      // while(next->is_locked());
      
      parent = next->find_parent_node(key, current_header, depth + 1);
    } while(next->is_invalid());
    return parent;
  }

  bool lookup_and_read_record(V& retval, header_type current_header) {

    ARTRecord<V>* record;
    
    do {
      while(this->is_locked());
      record = this->find_record();
      if (record != NULL) {
        retval = record->read();
      }
    } while(this->is_locked());
    
    if (record == NULL) {
      // Observe the version number of the lowest node in the path in STO to
      // add to the read set. We manually add one to the pointer value make
      // the least significant bit different from that of records in the read
      // set, since all pointers have their least significant bit set to 0.
      Sto::item(tree_, (ARTNode<V>*)((uintptr_t)this + (uint64_t)1)).
        add_read(/*current_version_number*/ current_header);
      return false;
    }

    if (record->is_deleted() || (Sto::item(tree_, record).flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT)) {
      return false;
    }

    return true;
  }

  // Traverse the ART to find the node that matches the key.
  // If the path finds nonexisting nodes, of if parent node is
  // not found, will create new nodes until the path is complete.
  // Note that this method implements lazy expansion (though not yet!).
  ARTNode<V>* find_parent_node_insert(std::string key, V value, ARTRecord<V>*& record, bool& new_record_created, uint64_t depth = 0) {
    // Initial Invalid check
    if (this->is_invalid())
      return NULL;

    // Handles the accessing the record from the leaf node.
    if (depth == key.length()) {
      return handle_leaf_node(record, value, key, new_record_created);        
    }
    
    unsigned char next_byte = key[depth];

    // 'child' MUST be NULL here when I call find_or_create_child
    ARTNode<V>* child;
    ARTNode<V>* found_node;
    
    do {
      
      if (!find_or_create_child(child, next_byte)) {
        // In this case, 'child' is either set to NULL or the new grown
        // node that must replace the current node.
        return child;
      }
      
      found_node = child->find_parent_node_insert(key, value, record,
                                                  new_record_created, depth + 1);

      // Child must have been invalid, meaning we must retry.
      if(found_node == NULL) {
        continue;
      }

      // Invalid check
      if (this->is_invalid()) {
        if (child->must_grow())
          unlock(child);
        return NULL;
      }
    
      if (child->must_grow()) {
        lock(this);
        
        // Invalid check
        if (this->is_invalid()) {
          unlock(child);
          unlock(this);
          return NULL;
        }
        
        this->replace_child(found_node, next_byte);
        // Since we held on the the old child's lock to grow the node,
        // we must now release it.
        
        unlock(child);
        unlock(this);
        continue;
      }
    } while(child->is_invalid() || (found_node->is_invalid() && !found_node->must_grow()));

    return found_node;
  }

  // Handles accessing the leaf node to extract the record, creating a new one if
  // needed. Used in find_parent_node_insert() recursive call, meaning a return a NULL
  // represents the need for a node to grow.
  inline ARTNode<V>* handle_leaf_node(ARTRecord<V>*& record, V value, std::string key, bool& new_record_created) {
    record = find_record();

    if (record != NULL) {
      // If the record exists, make sure that it's not to be deleted in
      // another transaction. If so, then the transaction aborts.
      if(record->is_deleted() &&
         !(Sto::item(tree_, record).flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT)) {
        Sto::abort();
      }

      // See if the record was 'deleted' in the same transaction as it's currently being inserted.
      // If so, we'll tell Sto to not remove the record anymore, and the record's value is updated.
      if ((Sto::item(tree_, record).flags() & ART_RECORD_TRANS_ITEM_DELETE_BIT)
          && !record->is_deleted()) {
        record->set_undeleted();
        record->update(value);
      }
    }
      
    if (record == NULL) {
      lock(this);

      // Invalid check
      if (this->is_invalid()) {
        unlock(this);
        return NULL;
      }

      // Handles case where another thread inserted a record before the
      // lock on the record was acquired.
      if (record != NULL) {
        return this;
      }
        
      if(!this->is_full()) {
        // Since the record is new, it gets initialized with the
        // inserted status bit set. That way any other transaction
        // that finds the node before this transaction has committed
        // isn't allowed to use it.
        record = new ARTRecord<V>(value, tree_, key);

        // printf("[%d] Header: %lx\n", TThread::id(), header_);
        this->insert_record(record);
        new_record_created = true;

        // Update the version number only if you haven't
        TransProxy item = Sto::item(tree_, (ARTNode<V>*)((uintptr_t)this +
                                                        (uint64_t)1));

        header_type saved_header = header_ & (~1ul);
        this->update_version_number();
        // Check if the item is already in the read set of the current
        // transaction. If so, then check that the headers (excluding the lock
        // bit) are the same. If the headers are the same, update the version
        // number.
        if (item.has_read()) {
          item.update_read(saved_header, header_ & (~1ul));
        }
        
        unlock(this);
      } else {
        ARTNode<V>* new_node = this->grow();
        this->set_must_grow();
        // TODO: remove this method
        // Done so that if the node was in the read set, growing this node will
        // be noticed by Sto.
        // this->update_version_number();

        header_type saved_header = header_ & (~1ul);
        this->invalidate();

        // Update the version number only if you haven't
        TransProxy item = Sto::item(tree_, (ARTNode<V>*)((uintptr_t)this +
                                                         (uint64_t)1));
        // Check if the item is already in the read set of the current
        // transaction. If so, then check that the headers (excluding the lock
        // bit) are the same. If the headers are the same, update the version
        // number.
        if (item.has_read()) {
          item.update_read(saved_header, header_ & (~1ul));
        }

        // Note that unlock(this) was not called upon return.
        // Since we must grow the node, we still hold on to this lock.
        return new_node; 
      }
    }
    
    return this;
  }

  // Modifies 'child' to point to the child of the current ARTNode.
  // If 'child' was not found, it is assigned to a new internal node.
  // If the current node is invalid at any time, or if the current node must
  // grow to accomodate the child, 'child' gets set to NULL.
  // Precondition: 'child' can only be NULL before this method call.
  // Postcondition: The find_parent_node_insert method will return 'child'
  //   if false is returned.
  inline bool find_or_create_child(ARTNode<V>*& child, unsigned char next_byte) {
    child = find_child(next_byte);      
    if (child == NULL) {
      lock(this);
      
      // Invalid check
      if (this->is_invalid()) {
        unlock(this);
        
        // Assuming child is returned, NULL will be returned,
        // meaning the parent of 'child' (the current node) is
        // invalid and its parent must retry.
        child = NULL; 
        return false;

        // return NULL;
      }

      // Handles when another thread added a child just before the
      // current thread locked the child.
      // ASSUMES 'child' WAS NOT ASSIGNED TO ANYTHING BEFOREHAND.
      child = find_child(next_byte);
      if (child != NULL) {
        unlock(this);
        return true;
      }

      if (this->is_full()) {
        ARTNode<V>* new_node = this->grow();
        this->set_must_grow();
        // this->update_version_number();
        this->invalidate(); // TODO: figure out if should also invalidate the node when you need to grow it.
        
        // Note that unlock(this) was not called upon return.
        // Since we must grow the node, we still hold on to this lock.
        
        // False is returned, meaning new_node will be returned in find_parent_node_insert,
        // which will trigger the parent node to replace the old full child with the grown node.
        child = new_node;
        return false;

        // return new_node;
      }
      
      child = new ARTNode4<V>(tree_);
      assert(!this->is_full() && "A");
      this->add_child(child, next_byte);
      this->update_version_number();
      // std::cout << "updated version number" << std::endl;
      unlock(this);
    }
    return true;
  }

  // Returns whether the current node is locked at the moment.
  // TODO: test this method when needed.
  inline bool is_locked() {
    return header_ & 1;
  }

  // Returns whether the current node is invalidated due to
  // the parent node replacing the current node with another node and
  // storing the new node's location.
  inline bool is_valid() {
    return header_ & 2;
  }

  // Return whether the ARTNode is a leaf node. Used to handle lazy
  // expansion (a future optimization)
  inline bool is_leaf() {
    return is_leaf_;
  }

  // Atomic exchange of lock. Use the static method 'lock(ARTNode<V>*)' for a full
  // spinlock implementation.
  inline bool try_lock() {
    header_type header = header_;
    return ::bool_cmpxchg((header_type*)(&header_), header & ~((uint64_t)(1)), header | 1);
  }

  // Unlocks a node. The node must have been locked before. Use the static method
  // 'unlock(ARTNode<V>*)' for convenience and consistency with the locking mechanisms of this class.
  inline void unlock() {
    assert(is_locked() && "Attempt to unlock a node that is already unlocked");
    header_--; // TODO: Is there a better way to do this?
  }

  // Sets the 'must grow' status bit to true.
  inline void set_must_grow() {
    // assert(is_locked()) ?
    header_ |= 4;
  }

  // Sets the 'invalid' bit to true.
  inline void invalidate() {
    // assert(is_locked()) ?
    header_ |= 2;
  }

  // Returns the 'must grow' status bit.
  inline bool must_grow() {
    return header_ & 4;
  }

  // Returns the 'invalid' status bit.
  inline bool is_invalid() {
    return header_ & 2;
  }

  // Updates the header in the ARTNode, thereby incrementing the version number.
  // This method should be called after every update to the ARTNode.
  inline void update_version_number() {
    header_ = header_ + (1 << NUM_STATUS_BITS);
  }
  
  // Virtual methods

  // Inserts a pointer in parent_node that leads to a record.
  // The correct index to place the pointer is given by the char RECORD_BYTE.
  // Precondition: No record already exists at the given key,
  // and the node has enough space to insert the record.
  virtual void insert_record(ARTRecord<V>* record) = 0;
 
  // Removes a pointer in a parent node and deletes the record at the pointer's
  // address. The correct index to remove is given by the char RECORD_BYTE.
  // Precondition: A record must exist for the given key.
  virtual void remove_record() = 0;

  // Used to index correctly for each type of ARTNode.
  // Returns null if a child for any of the nodes traversed was not found.
  // Does NOT take care of lazy expansion or path compression (find_parent_node
  // does that at the moment).
  virtual ARTNode<V>* find_child(unsigned char byte) = 0;

  // Finds the record associated with the ARTNode at the byte RECORD_BYTE.
  // Returns null if no record was found.
  virtual ARTRecord<V>* find_record() = 0;

  // Adds a pointer to the child node in the current node.
  // Precondition: Node must not already have a value at the position
  // next_byte, and node must have space to add the child.
  virtual void add_child(ARTNode<V>* child, unsigned char next_byte) = 0;

  // Replaces a child node pointer in the current node with a pointer to a new child
  // Precondition: A pointer must already exist at the position given by next_byte.
  virtual void replace_child(ARTNode<V>* new_child, unsigned char next_byte) = 0;

  // Returns a vector of all children found in the node.
  virtual std::vector<void *>* get_children() = 0;

  // Returns whether the node has is at full capacity.
  virtual bool is_full() = 0;

  // Return whether the node is empty
  virtual bool is_empty() = 0;

  // Return whether the node is at the threshold where it should shrink
  // to a smaller node.
  virtual bool must_shrink() = 0;

  // Returns a newly allocated ARTNode that is the next size larger than
  // the current node and contains all the children the current node contains.
  // Note that grow for ARTNode256 is not possible and will throw an error.
  virtual ARTNode<V>* grow() = 0;

  // Contains the version number, lock bit, invalid bit, and one other
  // unused status bit.
  //
  // 64                     3           2         1      0
  // -----------------------------------------------------
  // |    version number    | must grow | invalid | lock |
  // -----------------------------------------------------
  volatile header_type header_;
  
  // Used for lazy expansion (I think)
  bool is_leaf_;

protected:
  // The ARTTree that this node pertains to.
  ARTTree<V>* tree_;
};
