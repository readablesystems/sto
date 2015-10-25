#pragma once

#include <cassert> 
#include <utility>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"
#include "RBTreeInternal.hh"

#define PRINT_DEBUG 1 // Set this to 1 to print some debugging statements.

template <typename K, typename T>
class RBTree;

template <typename T>
class rbwrapper : public T {
  public:
    typedef TransactionTid::type Version;
    template <typename... Args> inline rbwrapper(Args&&... args)
    : T(std::forward<Args>(args)...) {
    }
    inline rbwrapper(const T& x)
    : T(x) {
    }
    inline rbwrapper(T&& x) noexcept
    : T(std::move(x)) {
    }
    inline const T& value() const {
        return *this;
    }
    inline T& mutable_value() {
        return *this;
    }
    inline std::pair<Version, Version>&& inc_nodeversion() {
        TransactionTid::lock(rblinks_.nodeversion_);
        Version old_readv = TransactionTid::unlocked(rblinks_.nodeversion_);
        TransactionTid::inc_invalid_version(rblinks_.nodeversion_);
        Version new_readv = TransactionTid::unlocked(rblinks_.nodeversion_);
        TransactionTid::unlock(rblinks_.nodeversion_);
        return std::move(std::make_pair(old_readv, new_readv));
    }
    inline Version& nodeversion() {
        return rblinks_.nodeversion_;
    }
    rblinks<rbwrapper<T> > rblinks_;
};

// Define a custom key-value pair type that contains versions and also
// has proper comparison methods
template <typename K, typename T>
class rbpair {
public:
    typedef TransactionTid::type Version;
    typedef versioned_value_struct<std::pair<K, T>> versioned_pair;

    static constexpr Version insert_bit = TransactionTid::user_bit1;

    explicit rbpair(const K& key, const T& value)
    : pair_(std::pair<K, T>(key, value), TransactionTid::increment_value + insert_bit) {}
    explicit rbpair(std::pair<K, T>& kvp)
    : pair_(kvp, TransactionTid::increment_value + insert_bit) {}

    inline const K& key() const {
        return pair_.read_value().first;
    }
    inline Version& version() {
        return pair_.version();
    }
    inline T& writeable_value() {
        return pair_.writeable_value().second;
    }
    inline bool operator<(const rbpair& rhs) const {
        return (key() < rhs.key());
    }
    inline bool operator>(const rbpair& rhs) const {
        return (key() > rhs.key());
    }

private:
    // key-value pair associated with a version for the data
    versioned_pair pair_;
};

template <typename K, typename T>
class RBProxy;

template <typename K, typename T>
class RBTree : public Shared {
    typedef TransactionTid::type Version;
    typedef versioned_value_struct<T> versioned_value;

    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr TransItem::flags_type update_tag = TransItem::user0_bit<<2;
    static constexpr Version insert_bit = TransactionTid::user_bit1;
    static constexpr Version delete_bit = TransactionTid::user_bit1<<1;

public:
    RBTree() {
        treelock_ = 0;
        treeversion_ = 0;
#if PRINT_DEBUG
        stats_ = {0,0,0,0,0,0};
#endif
    }
    
    typedef rbwrapper<rbpair<K, T>> wrapper_type;
    typedef rbtree<wrapper_type> internal_tree_type;
    
    // lookup
    inline size_t count(const K& key) const;
    // element access
    inline RBProxy<K, T> operator[](const K& key);
    // modifiers
    inline size_t erase(const K& key);

    void lock(versioned_value *e) {
        lock(&e->version());
    }
    void unlock(versioned_value *e) {
        unlock(&e->version());
    }

    inline void lock(TransItem& item);
    inline void unlock(TransItem& item);
    inline bool check(const TransItem& item, const Transaction& trans);
    inline void install(TransItem& item, const Transaction& t);
    inline void cleanup(TransItem& item, bool committed);
#if PRINT_DEBUG
    inline void print_absent_reads();
#endif

private:
    inline size_t debug_size() const {
        return wrapper_tree_.size();
    }

    // Find and return a pointer to the rbwrapper. Abort if value inserted and not yet committed.
    // return values: (node*, found, boundary), boundary only valid if !found
    // NOTE: this function must be surrounded by a lock in order to ensure we add the correct nodeversions
    inline std::pair<std::pair<wrapper_type*, bool>, std::pair<wrapper_type*, wrapper_type*>>
    find_or_abort(rbwrapper<rbpair<K, T>>& rbkvp, bool insert) const {
        auto results = wrapper_tree_.find_any(rbkvp, rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
        auto pair = results.first;
        auto x = pair.first;
        auto found = pair.second;

        // PRESENT GET
        if (found) {
            auto item = Sto::item(const_cast<RBTree<K, T>*>(this), x);
            // check if item is inserted by not committed yet 
            if (is_inserted(x->version())) {
                // check if item was inserted by this transaction
                if (has_insert(item)) {
                    return results;
                } else if (has_delete(item)) {
                    return results;
                } else {
                    // some other transaction inserted this node and hasn't committed
                    unlock(&treelock_);
                    Sto::abort();
                    // unreachable
                    return results;
                }
            }
            // add a read of the node version for a present get
            if (!insert)
                item.add_read(x->version());
        
        // ABSENT GET
        } else {
            // XXX this code only works with coarse-grain locking
            if (!insert) {
                // add a read of treeversion if tree is empty
                if (!x)
                    Sto::item(const_cast<RBTree<K, T>*>(this), tree_key_).add_read(treeversion_);
                // add a read for all nodes in the path, marking them as nodeversion ptrs
                for (unsigned int i = 0; i < 2; ++i) {
                    wrapper_type* n = (i == 0)? results.second.first : results.second.second;
                    if (n)
                        Sto::item(const_cast<RBTree<K, T>*>(this),
                                        reinterpret_cast<uintptr_t>(n) | 1U).add_read(n->nodeversion());
                }
            }
        }
        // item was committed or DNE, so return results
        return results;
    }

    // Insert <@key, @value>, optionally force an update if @key exists
    // return value is a reference to kvp.second
    inline T& insert_update(const K& key, const T& value, bool force) {
        lock(&treelock_);
        auto node = rbwrapper<rbpair<K, T>>( rbpair<K, T>(key, T()) );
        auto pair = this->find_or_abort(node, true).first;
        auto x = pair.first;
        auto found = pair.second;
        
        // INSERT: kvp did not exist
        if (!found) {
#if PRINT_DEBUG
            stats_.absent_insert++;
#endif
            auto n = new rbwrapper<rbpair<K, T> >(  rbpair<K, T>(key, value)  );
            // insert now returns the parent, before re-balancing, under which the
            // new leaf is inserted
            auto p = wrapper_tree_.insert(*n);
            // invariant: the node's insert_bit should be set
            assert(is_inserted(n->version()));
            // if tree is empty (i.e. no parent), we increment treeversion 
            if (!x) {
                Sto::item(this, tree_key_).add_write(0);
            // else we increment the parent version 
            } else {
                assert(p); //XXX ugly code; this is only true because of coarse-grained locking
                assert(p.node() == x); // XXX same problem as above
                // returned pair is the versions (unlocked) before and after the increment
                auto versions = p.node()->inc_nodeversion();
                auto item = Sto::item(this, reinterpret_cast<uintptr_t>(p.node()) | 1);
                // update our own read if necessary
                if (item.has_read())
                    item.update_read(versions.first, versions.second);
            }
            // add write and insert flag of item (value of rbpair) with @value
            Sto::item(this, n).add_write(value).add_flags(insert_tag);
            unlock(&treelock_);
            return n->writeable_value();

        // UPDATE: kvp is already inserted into the tree
        } else {
#if PRINT_DEBUG
            stats_.present_insert++;
#endif
            auto item = Sto::item(this, x);
            if (is_deleted(x->version())) {
                //XXX actually we should assert here
                // this entire path should be unreachable
                unlock(&treelock_);
                Sto::abort();
                // should be unreachable
                return x->writeable_value();
            
            // read-my-delete
            } else if (has_delete(item)) {
                item.clear_flags(delete_tag).add_flags(update_tag);
                // recover from delete-my-insert (engineer's induction all
                // over the place...
                if (is_inserted(x->version()))
                    item.add_flags(insert_tag);
                // either overwrite value or put empty value
                if (force) {
                    item.add_write(value);
                } else {
                    item.add_write(T());
                }
                unlock(&treelock_);
                return item.template write_value<T>();
            
            // read_my_writes
            } else if (item.has_write()) {
                // operator[] used on LHS, overwrite
                if (force) {
                    item.add_write(value).add_flags(update_tag);
                }
                unlock(&treelock_);
                // return the last value written (i.e. the old value if we didn't force)
                return item.template write_value<T>();
            }

            // accessing a regular key
            if (force) {
                item.add_write(value).add_flags(update_tag);
            }
            unlock(&treelock_);
            return item.template write_value<T>(); 
        }
    }

    static void lock(Version *v) {
        TransactionTid::lock(*v);
    }
    static void unlock(Version *v) {
        TransactionTid::unlock(*v);
    }
    static bool has_update(const TransItem& item) {
        return item.flags() & update_tag;
    }
    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_tag;
    }
    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_tag;
    }
    static bool is_locked(Version v) {
        return TransactionTid::is_locked(v);
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
    static bool is_deleted(Version v) {
        return v & delete_bit;
    }
    static void erase_deleted(Version* v) {
        *v = *v & (~delete_bit);
    }
    static void mark_deleted(Version* v) {
        *v = *v | delete_bit;
    }

    internal_tree_type wrapper_tree_;
    mutable Version treelock_;
    mutable Version treeversion_;
    // used to mark whether a key is for the tree structure (for tree version checks)
    // or a pointer (which will always have the lower 3 bits as 0)
    static constexpr uintptr_t tree_bit = 1U<<0;
    void* tree_key_ = (void*) tree_bit;
#if PRINT_DEBUG
    mutable struct stats {
        int absent_insert, absent_delete, absent_count;
        int present_insert, present_delete, present_count;
    } stats_;
#endif
 
    friend class RBProxy<K, T>;
};

// STL-ish interface wrapper returned by RBTree::operator[]
template <typename K, typename T>
class RBProxy {
public:
    typedef RBTree<K, T> transtree_t;
    explicit RBProxy(transtree_t& tree, const K& key)
        : tree_(tree), key_(key) {};
    // when we just do a read of the item (using operator[] on the RHS), we don't
    // want to force an update
    operator T() {
        return tree_.insert_update(key_, T(), false);
    }
    RBProxy& operator=(const T& value) {
        tree_.insert_update(key_, value, true);
        return *this;
    };
    RBProxy& operator=(RBProxy& other) {
        tree_.insert_update(key_, (T)other, true);
        return *this;
    };
private:
    transtree_t& tree_;
    const K& key_;
};

template <typename K, typename T>
inline size_t RBTree<K, T>::count(const K& key) const {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    lock(&treelock_);
    auto results = find_or_abort(idx_pair, false);
    unlock(&treelock_);
    auto pair = results.first;
    auto found = pair.second;
#if PRINT_DEBUG
    (!found) ? stats_.absent_count++ : stats_.present_count++;
#endif
    if (found) {
        auto item = Sto::item(const_cast<RBTree<K, T>*>(this), pair.first);
        if (is_inserted(pair.first->version()) && has_delete(item)) {
            // read my insert-then-delete
            return 0;
        }
    }
    return (found) ? 1 : 0;
}

template <typename K, typename T>
inline RBProxy<K, T> RBTree<K, T>::operator[](const K& key) {
    return RBProxy<K, T>(*this, key);
}

template <typename K, typename T>
inline size_t RBTree<K, T>::erase(const K& key) {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    lock(&treelock_);
    auto results = find_or_abort(idx_pair, false);
    unlock(&treelock_);
    auto pair = results.first; 
    auto x = pair.first;
    auto found = pair.second;
   
    // PRESENT ERASE
    if (found) {
#if PRINT_DEBUG
        stats_.present_delete++;
#endif
        auto item = Sto::item(this, x);
        // item marked at install as deleted, not deleted yet so abort
        if (is_deleted(x->version())) {
            Sto::abort();
            return 0;
        }
        // item marked as inserted and not yet installed
        if (is_inserted(x->version())) {
            // mark to delete at install time
            if (has_insert(item)) {
                item.add_write(0).clear_flags(insert_tag).add_flags(delete_tag);
                return 1;
            } else if (has_delete(item)) {
                // insert-then-delete
                return 0;
            } else {
                Sto::abort();
                // unreachable
                return 0;
            }
        }
        // found item that has already been installed and not deleted
        item.add_write(0).add_flags(delete_tag);
        return 1;

    // ABSENT ERASE
    } else {
#if PRINT_DEBUG
        stats_.absent_delete++;
#endif
        return 0;
    }
}

template <typename K, typename T>
inline void RBTree<K, T>::lock(TransItem& item) {
    if (item.key<void*>() == tree_key_) {
        lock(&treeversion_);
    } else { 
        lock(item.key<versioned_value*>());
    }
}
    
template <typename K, typename T>
inline void RBTree<K, T>::unlock(TransItem& item) {
    if (item.key<void*>() == tree_key_) {
        unlock(&treeversion_);
    } else {
        unlock(item.key<versioned_value*>());
    }
}
   
template <typename K, typename T>
inline bool RBTree<K, T>::check(const TransItem& item, const Transaction& trans) {
    auto e = item.key<uintptr_t>();
    bool is_structured = e & uintptr_t(1);
    auto read_version = item.read_value<Version>();
    Version curr_version;

    // set up the correct current version to check: either treeversion, item version, or nodeversion
    if ((void*)e == (wrapper_type*)tree_key_) {
        curr_version = treeversion_;
    } else if (is_structured) {
        curr_version = reinterpret_cast<wrapper_type*>(e & ~uintptr_t(1))->nodeversion();
    } else {
        curr_version = reinterpret_cast<wrapper_type*>(e)->version();
    }

    bool same_version = (read_version ^ curr_version) <= TransactionTid::lock_bit;
    bool not_locked = (!is_locked(curr_version) || item.has_lock(trans));
    // to handle the case that we check nodeversion, but the node (parent) is deleted
    bool not_deleted = !is_deleted(curr_version);
    return same_version && not_locked && not_deleted;
}

// key-versionedvalue pairs with the same key will have two different items
template <typename K, typename T>
inline void RBTree<K, T>::install(TransItem& item, const Transaction& t) {
    (void) t;
    // we don't need to check for nodeversion updates because those are done during execution
    auto e = item.key<wrapper_type*>();
    if ((void*)e != (wrapper_type*)tree_key_) {
        assert(is_locked(e->version()));

        bool deleted = has_delete(item);
        bool inserted = has_insert(item);
        bool updated = has_update(item);
        // should never be both deleted and inserted...
        // sanity check to make sure we handled read_my_writes correctly
        assert(!(deleted && inserted));
        assert(deleted || inserted || updated);
        // actually erase the element when installing the delete
        if (deleted) {
            // mark deleted (in case rcu free doesn't free immediately)
            mark_deleted(&e->version());
            // actually erase
            lock(&treelock_);
            wrapper_tree_.erase(*e);
            unlock(&treelock_);
            Transaction::rcu_free(e);
        } else if (inserted) {
            erase_inserted(&e->version());
        } else if (updated) {
            // update, but item deleted by another transaction - we should reinsert the value
            if (is_deleted(e->version())) {
                lock(&treelock_);
                auto n = new rbwrapper<rbpair<K, T> >(  rbpair<K, T>(e->key(), item.template write_value<T>())  );
                wrapper_tree_.insert(*n);
                unlock(&treelock_);
            // else install the update
            } else {
                e->writeable_value() = item.template write_value<T>(); 
                lock(&e->version());
                TransactionTid::inc_invalid_version(e->version());
                unlock(&e->version());
            }
        }
    // we did something to an empty tree, so update treeversion
    } else {
        assert(is_locked(treeversion_));
        TransactionTid::inc_invalid_version(treeversion_);
    }
}

template <typename K, typename T>
inline void RBTree<K, T>::cleanup(TransItem& item, bool committed) {
    if (!committed) {
        // if item has been tagged deleted or structured, don't need to do anything 
        // if item has been tagged inserted, then we erase the item
        if (has_insert(item)) {
            auto e = item.key<wrapper_type*>();
            lock(&treelock_);
            wrapper_tree_.erase(*e);
            unlock(&treelock_);
            mark_deleted(&e->version());
            erase_inserted(&e->version());
            Transaction::rcu_free(e);
        }
    }
}

#if PRINT_DEBUG 
template <typename K, typename T>
inline void RBTree<K, T>::print_absent_reads() {
    std::cout << "absent inserts: " << stats_.absent_insert << std::endl;
    std::cout << "absent deletes: " << stats_.absent_delete << std::endl;
    std::cout << "absent counts: " << stats_.absent_count << std::endl;
    std::cout << "present inserts: " << stats_.present_insert << std::endl;
    std::cout << "present deletes: " << stats_.present_delete << std::endl;
    std::cout << "present counts: " << stats_.present_count << std::endl;
    std::cout << "size: " << debug_size() << std::endl;
}
#endif
