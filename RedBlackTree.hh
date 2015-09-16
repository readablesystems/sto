#pragma once

#include <vector>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

// XXX taken from PrioQueue, unsure if necessary
enum Status{
    AVAILABLE,
    EMPTY,
    BUSY,
};

// XXX rblinks?
// XXX rbpriv = private rbtree methods, i.e.
//      rbcompare --> compares two types? rbreshape?) rbrep0/rbrep1?
//      XXX rbcomparator --> why are there two?
// rbalgorithms --> iterate through rbtree
// why have rbiterator and rbconst_iterator? what do these do?

// XXX what do all the templates and inlines do? (esp under class rbtree)
    // insert commits, checks, unique_check
    // delete node
    // find_any
    // find_first
    // find
    // erase
    // unlinkempty
    // size
    // check
    // insert
    // root
    // iterator/iterate?

template <typename T>
struct RBNode{
    typedef versioned_value_struct<T> versioned_value; // define versioned value of type T to be named versioned_value
    typedef TransactionTid::type Version; // the type of the transaction id is a Version
public:
    versioned_value* val; // each node will have a value and a version
    Version ver; // the transaction id of the node
    Status status;
   
    // constructor?
    RBNode(versioned_value* val_) : val(val_), ver(0), status(BUSY) {}

    // XXX inline? TAKEN FROM RBNODEPTR
    RBNode& child(bool isright);
    RBNode& load_color();
    set_child
    parent
    black_parent
    find_child
    children_same_color

    red
    change_color
    reverse_color

    rotate
    rotate_left
    rotate_right
    flip()

    // XXX should these be put in rbtree, instead of rbnode?
    size()
    check()
    output()
};

template <typename T, bool Opacity = false> // XXX Opacity?
class RBTree: public Shared {
    typedef TransactionTid::type Version;
    typedef VersionFunctions<Version> Versioning;
    typedef versioned_value_struct<T> versioned_value;
    
    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    
    static constexpr Version insert_bit = TransactionTid::user_bit1;
    static constexpr Version delete_bit = TransactionTid::user_bit1<<1;
    static constexpr Version dirty_bit = TransactionTid::user_bit1<<2;
public:
    RBTree() : nodes_() {
        // initialize private variables
        size_ = 0;
        // locks and versions go here
    }


    
    
    
    void lock(versioned_value *e) {
        lock(&e->version());
    }
    void unlock(versioned_value *e) {
        unlock(&e->version());
    }
    
    // TRANSACTIONAL METHODS
    void lock(TransItem& item) {
        if (item.key<int>() == push_key) {
            lock(&pushversion_);
        } else if (item.key<int>() == pop_key){
            lock(&popversion_);
        } else {
            lock(item.key<versioned_value*>());
        }
    }
    
    void unlock(TransItem& item) {
        if (item.key<int>() == push_key) {
            unlock(&pushversion_);
        } else if (item.key<int>() == pop_key){
            unlock(&popversion_);
        } else {
            unlock(item.key<versioned_value*>());
        }
    }
    
    bool check(const TransItem& item, const Transaction& trans){
    }
    
    
    void install(TransItem& item, const Transaction& t) {
    }
    
    void cleanup(TransItem& item, bool committed) {
    }
    
    // Used for debugging
    void print() {
        for (int i =0; i < size_; i++) {
            std::cout << nodes_[i].val->read_value() << "[" << (!is_inserted(nodes_[i].val->version()) && !is_deleted(nodes_[i].val->version())) << "] ";
        }
        std::cout << std::endl;
    }
    
private:
    static void lock(Version *v) {
        TransactionTid::lock(*v);
    }
    
    static void unlock(Version *v) {
        TransactionTid::unlock(*v);
    }
    
    static bool is_locked(Version v) {
        return TransactionTid::is_locked(v);
    }

    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_tag;
    }
    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_tag;
    }
    
    static bool is_inserted(Version v) {
        return v & insert_bit;
    }
    
    static void erase_inserted(Version* v) {
        *v = *v & (~insert_bit);
    }
    
    static void mark_inserted(Version* v) {
        *v = *v | insert_bit;
    }
    
    static bool is_dirty(Version v) {
        return v & dirty_bit;
    }
    
    static void erase_dirty(Version* v) {
        *v = *v & (~dirty_bit);
    }
    
    static void mark_dirty(Version* v) {
        *v = *v | dirty_bit;
    }
            
    static bool is_deleted(Version v) {
        return v & delete_bit;
    }
            
    static void erase_deleted(Version* v) {
        *v = *v & (~delete_bit);
    }
            
    static void mark_deleted(Version* v) {
        *v = *v | delete_bit;
    }

    std::vector<RBNode<T>> nodes_;
    Version put_locks_here_;
    Version put_versions_here_;
    int size_;

