#pragma once

#include <cassert> 
#include <utility>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"
#include "RBTreeInternal.hh"

#define DEBUG 1
#if DEBUG
extern TransactionTid::type lock;
#endif

template<typename K, typename T> class RBTreeIterator;
template <typename K, typename T> class RBTree;

template <typename T>
class rbwrapper : public T {
  public:
    typedef TransactionTid::type Version;
    explicit inline rbwrapper(const T& x) : T(x) {
    }
    inline const T& value() const {
        return *this;
    }
    inline T& mutable_value() {
        return *this;
    }
    inline Version& nodeversion() {
        return rblinks_.nodeversion_;
    }
    inline Version& version() {
        return rblinks_.valueversion_;
    }
    rblinks<rbwrapper<T> > rblinks_;
};

// Define a custom key-value pair type that contains versions and also
// has proper comparison methods
template <typename K, typename T>
class rbpair {
public:
    typedef TransactionTid::type Version;

    explicit rbpair(const K& key, const T& value) : pair_(std::pair<const K, T>(key, value)) {}
    explicit rbpair(std::pair<const K, T>& kvp) : pair_(kvp) {}

    inline const K& key() const {
        return pair_.first;
    }
    inline T& writeable_value() {
        return pair_.second;
    }
    inline bool operator<(const rbpair& rhs) const {
        return (key() < rhs.key());
    }
    inline bool operator>(const rbpair& rhs) const {
        return (key() > rhs.key());
    }

private:
    // key-value pair associated with a version for the data
    std::pair<const K, T> pair_;
};

template <typename K, typename T> class RBProxy;

template <typename K, typename T>
class RBTree : public Shared {
    friend class RBTreeIterator<K, T>;
    friend class RBProxy<K, T>;

    typedef TransactionTid::type Version;
    typedef versioned_value_struct<T> versioned_value;

    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr Version insert_bit = TransactionTid::user_bit1;

    typedef RBTreeIterator<K, T> iterator;
    typedef const RBTreeIterator<K, T> const_iterator;
    
    typedef rbwrapper<rbpair<K, T>> wrapper_type;

    typedef std::pair<std::pair<wrapper_type*, Version>, std::pair<wrapper_type*, Version>> boundary_nodes;
    typedef std::pair<std::pair<rbnodeptr<wrapper_type>, Version>, bool> found_node;

public:
    RBTree() {
        treelock_ = 0;
        sizeversion_ = 0;
        size_ = 0;
#if DEBUG
        stats_ = {0,0,0,0,0,0};
#endif
    }
    
    typedef rbtree<wrapper_type> internal_tree_type;

    // capacity 
    inline size_t size() const;
    // lookup
    inline size_t count(const K& key) const;
    // element access
    inline RBProxy<K, T> operator[](const K& key);
    // modifiers
    inline size_t erase(const K& key);

    // STAMP-compatible nontransactional methods
    bool nontrans_insert(const K& key, const T& value);
    bool nontrans_contains(const K& key);
    bool nontrans_remove(const K& key);
    T nontrans_find(const K& key); // returns T() if not found, works for STAMP
    
    // iterators
    // TODO should not need to lock?
    iterator begin() {
        lock(&treelock_);
        wrapper_type* start = wrapper_tree_.r_.limit_[0];
        if (start == nullptr) {
            // tree empty, read tree version
            Sto::item(this, tree_key_).add_read(treeversion());
        } else {
            // READ-MY-WRITES: skip our own deletes!
            while(has_delete(Sto::item(this, start))) {
                Sto::item(this, (reinterpret_cast<uintptr_t>(start)|0x1)).add_read(start->nodeversion());
                start = rbalgorithms<wrapper_type>::next_node(start);
            }
            if (start != nullptr) {
                if (is_phantom_node(start, start->version())) {
                    unlock(&treelock_);
                    Sto::abort();
                }
                // valid start node, read nodeversion
                Sto::item(this, (reinterpret_cast<uintptr_t>(start)|0x1)).add_read(start->nodeversion());
            }
        }
        unlock(&treelock_);
        return iterator(this, start);
    }

    iterator end() {
        return iterator(this, nullptr);
    }

    inline void lock(TransItem& item);
    inline void unlock(TransItem& item);
    inline bool check(const TransItem& item, const Transaction& trans);
    inline void install(TransItem& item, const Transaction& t);
    inline void cleanup(TransItem& item, bool committed);
#if DEBUG
    inline void print_absent_reads();
#endif

private:
    inline size_t debug_size() const {
        return wrapper_tree_.size();
    }
    inline Version& treeversion() {
        return wrapper_tree_.treeversion_;
    }
    inline wrapper_type* get_next(wrapper_type* node) {
        lock(&treelock_);
        wrapper_type* next_node = rbalgorithms<wrapper_type>::next_node(node);
        // READ-MY-WRITES: skip our own deletes
        while (has_delete(Sto::item(this, next_node))) {
            // add read of nodeversions of all deleted nodes; gosh, read-my-write is really a nightmare...
            Sto::item(this, (reinterpret_cast<uintptr_t>(next_node) | 0x1)).add_read(next_node->nodeversion());
            next_node = rbalgorithms<wrapper_type>::next_node(next_node);
        }

        if (next_node != nullptr) {
            if (is_phantom_node(next_node, next_node->version())) {
                unlock(&treelock_);
                Sto::abort();
            }
            Sto::item(this, (reinterpret_cast<uintptr_t>(next_node)|0x1)).add_read(next_node->nodeversion());
        }
        Sto::item(this, (reinterpret_cast<uintptr_t>(node)|0x1)).add_read(node->nodeversion());
        unlock(&treelock_);
        return next_node;
    }

    inline wrapper_type* get_prev(wrapper_type* node) {
        lock(&treelock_);
        // check if we are at the end() node (i.e. nullptr)
        wrapper_type* prev_node = (node == nullptr) ? wrapper_tree_.r_.limit_[1] : rbalgorithms<wrapper_type>::prev_node(node);
        // check that we are not begin (i.e. prev_node is not null)
        if (prev_node == nullptr) {
            unlock(&treelock_);
            Sto::abort();
        }
        // READ-MY-WRITES: skip our own deletes
        while (has_delete(Sto::item(this, prev_node))) {
            Sto::item(this, (reinterpret_cast<uintptr_t>(prev_node) | 0x1)).add_read(prev_node->nodeversion());
            prev_node = rbalgorithms<wrapper_type>::prev_node(prev_node);
        }
        // check again after reading-my-writes...
        if (prev_node == nullptr) {
            unlock(&treelock_);
            Sto::abort();
        }

        if (is_phantom_node(prev_node, prev_node->version())) {
            unlock(&treelock_);
            Sto::abort();
        }
        if (node) {
            Sto::item(this, (reinterpret_cast<uintptr_t>(node)|0x1)).add_read(node->nodeversion());
        }
        if (prev_node) {
            Sto::item(this, (reinterpret_cast<uintptr_t>(prev_node)|0x1)).add_read(prev_node->nodeversion());
        }
        unlock(&treelock_);
        return prev_node;
    }

    // A (hard) phantom node is a node that's being inserted but not yet
    // committed by another transaction. It should be treated as invisible
    inline bool is_phantom_node(wrapper_type* node, Version& version) const {
        auto item = Sto::item(const_cast<RBTree<K, T>*>(this), node);
        return (is_inserted(version) && !has_insert(item) && !has_delete(item));
    }

    // A soft phantom node is a node that's marked inserted by the current
    // transaction. Its value information is visible (and only visible) to
    // the current transaction
    inline bool is_soft_phantom(wrapper_type* node, Version& version) const {
        auto item = Sto::item(const_cast<RBTree<K, T>*>(this), node);
        return (is_inserted(version) && (has_insert(item) || has_delete(item)));
    }

    // increment or decrement the offset size of the transaction's tree
    inline void change_size_offset(ssize_t delta) {
        auto size_item = Sto::item(this, size_key_);
        ssize_t prev_offset = size_item.has_write() ? size_item.template write_value<ssize_t>() : 0;
        size_item.add_write(prev_offset + delta);
        assert(size_ + size_item.template write_value<ssize_t>() >= 0);
#if DEBUG
        TransactionTid::lock(::lock);
        printf("\tbase size: %lu\n", size_); 
        printf("\toffset: %ld\n", size_item.template write_value<ssize_t>());
        TransactionTid::unlock(::lock);
#endif 
    }

    // Find and return a pointer to the rbwrapper. Abort if value inserted and not yet committed.
    // return values: (node*, found, boundary), boundary only valid if !found
    // NOTE: this function must be surrounded by a lock in order to ensure we add the correct nodeversions
    inline results<wrapper_type> find_or_abort(rbwrapper<rbpair<K, T>>& rbkvp, bool insert) const {
        auto results = wrapper_tree_.find_any(rbkvp, 
                rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()),
                insert);
        wrapper_type* x = results.node.node();
        Version version = results.valueversion;

        // PRESENT GET
        if (results.found) {
            auto item = Sto::item(const_cast<RBTree<K, T>*>(this), x);
            // check if item is inserted by not committed yet 
            if (is_inserted(version)) {
                // check if item was inserted by this transaction
                if (has_insert(item) || (has_delete(item))) {
                    return results;
                } else {
                    // some other transaction inserted this node and hasn't committed
#if DEBUG
                    TransactionTid::lock(::lock);
                    printf("Aborted in find_or_abort\n");
                    TransactionTid::unlock(::lock);
#endif
                    Sto::abort();
                    // unreachable
                    return results;
                }
            }
            // add a read of the node version for a present get
            if (!insert)
                item.add_read(version);
        
        // ABSENT GET
        } else {
            // not an insert (on RHS), add a read of boundary nodes and read of treeversion if empty tree
            if (!x && !insert)
                Sto::item(const_cast<RBTree<K, T>*>(this), tree_key_).add_read(version);
            if (insert) {
                if (x) {
                    // we currently do not allow insertions under phantom nodes
                    if (is_phantom_node(x, version)) {
#if DEBUG
                        TransactionTid::lock(::lock);
                        printf("Aborted in find_or_abort (insertion under phantom node)\n");
                        TransactionTid::unlock(::lock);
#endif
                        Sto::abort();
                        return results;
                    }
                }
            } else {
                // add reads of boundary nodes, marking them as nodeversion ptrs
                for (unsigned int i = 0; i < 2; ++i) {
                    auto n = (i == 0)? results.boundaries.first : results.boundaries.second;
                    if (n) {
                        Version v = (i == 0) ? results.bversions.first : results.bversions.second;
#if DEBUG
                        TransactionTid::lock(::lock);
                        printf("\t#Tracking boundary 0x%lx (k %d), nv 0x%lx\n", (unsigned long)n, n->key(), v);
                        TransactionTid::unlock(::lock);
#endif
                        Sto::item(const_cast<RBTree<K, T>*>(this),
                                        (reinterpret_cast<uintptr_t>(n)|0x1)).add_read(v);
                    }
                }
            }
        }
        // item was committed or DNE, so return results
        return results;
    }

    // Insert nonexistent key with empty value
    // return value is a pointer to the inserted node 
    inline wrapper_type* insert_absent(rbnodeptr<wrapper_type> found_p, 
            std::pair<unsigned long int, unsigned long int> nodeversions, const K& key) {
#if DEBUG
            stats_.absent_insert++;
#endif
            // use in-place constructor so that we don't mix up c++'s new and c's free()
            wrapper_type* n = (wrapper_type*)malloc(sizeof(wrapper_type));
            new (n) wrapper_type(rbpair<K, T>(key, T()));
            // insert new node under parent
            bool side = (found_p.node() == nullptr)? false :
                    wrapper_tree_.r_.node_compare(*n, *found_p.node()) > 0;
            wrapper_tree_.insert_commit(n, found_p, side);
            // invariant: the node's insert_bit should be set
            assert(is_inserted(n->version()));
            // if tree is empty (i.e. no parent), we increment treeversion 
            if (found_p.node() == nullptr) {
                Sto::item(this, tree_key_).add_write(0);
            // else we increment the parent version 
            } else {
                // XXX we should get the old and new versions from internal tree find_any
                auto item = Sto::item(this, reinterpret_cast<uintptr_t>(found_p.node()) | 1);
                // update our own read if necessary
                if (item.has_read()) {
                    item.update_read(nodeversions.first, nodeversions.second);
                }
            }
            // add write and insert flag of item (value of rbpair) with @value
            Sto::item(this, n).add_write(T()).add_flags(insert_tag);
            unlock(&treelock_);
            // add a write to size with incremented value
            change_size_offset(1);
            return n;
    }

    // Insert key and empty value if key does not exist 
    // If key exists, then add a read of the item version and return the node
    // return value is a reference to the found or inserted node 
    inline wrapper_type* insert(const K& key) {
        lock(&treelock_);
        auto node = rbwrapper<rbpair<K, T>>( rbpair<K, T>(key, T()) );
        results<wrapper_type> results = this->find_or_abort(node, true);
        rbnodeptr<wrapper_type> x_rbnp = results.node;
        Version version = results.valueversion;
        auto pnodeversions = results.pnodeversions;
        wrapper_type* x = x_rbnp.node();
        auto found = results.found;
        
        // INSERT: kvp did not exist
        if (!found)
            return insert_absent(x_rbnp, pnodeversions, key);

        // UPDATE: kvp is already inserted into the tree
        else {
#if DEBUG
            stats_.present_insert++;
#endif
            auto item = Sto::item(this, x);
            
            // insert-my-delete
            if (has_delete(item)) {
                item.clear_flags(delete_tag);
                // recover from delete-my-insert (engineer's induction all over the place...)
                if (is_inserted(version)) {
                    // okay to directly update value since we are the only txn
                    // who can access it
                    item.add_flags(insert_tag);
                    x->writeable_value() = T();
                }
                // overwrite value
                item.add_write(T());
                unlock(&treelock_);
                // we have to update the value of the size we will write
                change_size_offset(1);
                return x; 
            }
            // operator[] on RHS (THIS IS A READ!)
            // don't need to add a write to size because size isn't changing
            // STO won't add read of items in our write set
            item.add_read(version);
            unlock(&treelock_);
            return x;
        }
    }

    static void lock(Version *v) {
        TransactionTid::lock(*v);
    }
    static void unlock(Version *v) {
        TransactionTid::unlock(*v);
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

    internal_tree_type wrapper_tree_;
    // only add a write to size if we erase or do an absent insert
    size_t size_;
    mutable Version sizeversion_;
    mutable Version treelock_;
    // used to mark whether a key is for the tree structure (for tree version checks)
    // or a pointer (which will always have the lower 3 bits as 0)
    static constexpr uintptr_t tree_bit = 1U<<0;
    void* tree_key_ = (void*) tree_bit;
    static constexpr uintptr_t size_bit = 1U<<1;
    void* size_key_ = (void*) size_bit;
    static constexpr uintptr_t start_bit = 1U<<2;
    void* start_key_ = (void*) start_bit;
#if DEBUG
    mutable struct stats {
        int absent_insert, absent_delete, absent_count;
        int present_insert, present_delete, present_count;
    } stats_;
#endif
};

template<typename K, typename T>
class RBTreeIterator : public std::iterator<std::bidirectional_iterator_tag, rbwrapper<rbpair<K, T>>> {
public:
    typedef rbwrapper<rbpair<K, T>> wrapper;
    typedef RBTreeIterator<K, T> iterator;
    typedef RBProxy<K, T> proxy_type;
    typedef std::pair<const K, proxy_type> proxy_pair_type;

    RBTreeIterator(RBTree<K, T> * tree, wrapper* node) : tree_(tree), node_(node), proxy_pair_(nullptr) {
        this->update_proxy_pair();
    }
    RBTreeIterator(const RBTreeIterator& itr) : tree_(itr.tree_), node_(itr.node_), proxy_pair_(nullptr) {
        this->update_proxy_pair();
    }
    ~RBTreeIterator() {
        this->destroy_proxy_pair();
    }
    
    RBTreeIterator& operator=(const RBTreeIterator& v) {
        tree_ = v.tree_;
        node_ = v.node_;
        this->update_proxy_pair();
        return *this;
    }
   
    bool operator==(iterator other) const {
        return (tree_ == other.tree_) && (node_ == other.node_);
    }
    
    bool operator!=(iterator other) const {
        return !(operator==(other));
    }
   
    // Dereference operator on iterators; returns reference to std::pair<const K, T>
    proxy_pair_type& operator*() {
        // add a read of the version to make sure the value hasn't changed at commit time
        Sto::item(tree_, node_).add_read(node_->version());
        this->update_proxy_pair();
        return *proxy_pair_;
    }

    proxy_pair_type* operator->() {
        // add a read of the version to make sure the value hasn't changed at commit time
        Sto::item(tree_, node_).add_read(node_->version());
        this->update_proxy_pair();
        return proxy_pair_;
    }
    
    // This is the prefix case
    iterator& operator++() { 
        node_ = tree_->get_next(node_);
        this->update_proxy_pair();
        return *this; 
    }
    
    // This is the postfix case
    iterator operator++(int) {
        RBTreeIterator<K, T> clone(*this);
        node_ = tree_->get_next(node_);
        this->update_proxy_pair();
        return clone;
    }
    
    iterator& operator--() { 
        node_ = tree_->get_prev(node_);
        this->update_proxy_pair();
        return *this; 
    }
    
    iterator operator--(int) {
        RBTreeIterator<K, T> clone(*this);
        node_ = tree_->get_prev(node_);
        this->update_proxy_pair();
        return clone;
    }
        
private:
    inline void destroy_proxy_pair() {
        if (proxy_pair_ != nullptr) {
            delete proxy_pair_;
        }
    }
    inline void update_proxy_pair() {
        if (proxy_pair_ != nullptr) {
            delete proxy_pair_;
        }
        if (node_ == nullptr) {
            proxy_pair_ = nullptr;
            return;
        } else {
            proxy_pair_type* new_pair = new std::pair<const K, RBProxy<K, T>>(node_->key(), proxy_type(*tree_, node_));
            proxy_pair_ = new_pair;
            return;
        }
    }

    RBTree<K, T> * tree_;
    wrapper* node_;
    proxy_pair_type* proxy_pair_;
};

// STL-ish interface wrapper returned by RBTree::operator[]
// differentiate between reads and writes
template <typename K, typename T>
class RBProxy {
public:
    typedef RBTree<K, T> transtree_t;
    typedef rbwrapper<rbpair<K, T>> wrapper_type;
    typedef TransactionTid::type Version;

    explicit RBProxy(transtree_t& tree, wrapper_type* node)
        : tree_(tree), node_(node) {};

    // get the latest write value
    operator T() {
        auto item = Sto::item(&tree_, node_);
        if (item.has_write()) {
            return item.template write_value<T>();
        } else {
            // validate the read of the node, abort if someone has updated
            auto value = node_->writeable_value();
            auto curr_version = node_->version();
            assert(item.has_read());
            if (item.template read_value<Version>() != curr_version || RBTree<K, T>::is_locked(curr_version)) {
                Sto::abort();
            }
            return value; 
        }
    }
    RBProxy& operator=(const T& value) {
        auto item = Sto::item(&tree_, node_);
        item.add_write(value);
        return *this;
    };
    RBProxy& operator=(RBProxy& other) {
        auto item = Sto::item(&tree_, node_);
        item.add_write((T)other);
        return *this;
    };
private:
    transtree_t& tree_;
    wrapper_type* node_;
};

template <typename K, typename T>
inline size_t RBTree<K, T>::size() const {
    auto size_item = Sto::item(const_cast<RBTree<K, T>*>(this), size_key_);
    if (!size_item.has_read()) {
        size_item.add_read(sizeversion_);
    } else {
    }

    ssize_t offset = (size_item.has_write()) ? size_item.template write_value<ssize_t>() : 0;
    return size_ + offset;
}

template <typename K, typename T>
inline size_t RBTree<K, T>::count(const K& key) const {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    // should have added a read of boundary nodes if absent
    auto results = find_or_abort(idx_pair, false);
    bool found = results.found;
#if DEBUG
    (!found) ? stats_.absent_count++ : stats_.present_count++;
#endif
    if (found) {
        wrapper_type* x = results.node.node();
        auto item = Sto::item(const_cast<RBTree<K, T>*>(this), x);
        // read my deletes
        if (has_delete(item)) {
            return 0;
        }
    }
    return (found) ? 1 : 0;
}

template <typename K, typename T>
inline RBProxy<K, T> RBTree<K, T>::operator[](const K& key) {
    // either insert empty value or return present value
    auto node = insert(key);
    return RBProxy<K, T>(*this, node);
}

template <typename K, typename T>
inline size_t RBTree<K, T>::erase(const K& key) {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    // add a read of boundary nodes if absent erase
    auto results = find_or_abort(idx_pair, false);
    bool found = results.found;
    wrapper_type* x = results.node.node();
    Version version = results.valueversion;
   
    // PRESENT ERASE
    if (found) {
#if DEBUG
        stats_.present_delete++;
#endif
        auto item = Sto::item(this, x);
        
        // item marked as inserted and not yet installed
        if (is_inserted(version)) {
            // mark to delete at install time
            if (has_insert(item)) {
                item.add_write(0).clear_flags(insert_tag).add_flags(delete_tag);
                change_size_offset(-1);
                return 1;
            } else if (has_delete(item)) {
                // DO NOT UPDATE SIZE HERE
                // insert-delete-delete
                return 0;
            } else {
#if DEBUG
                TransactionTid::lock(::lock);
                printf("Aborted in erase (insert bit set)\n");
                TransactionTid::unlock(::lock);
#endif
                Sto::abort();
                // unreachable
                return 0;
            }
        // we are deleting our deletes (of item we didn't insert)
        } else if (has_delete(item)) {
            return 0; 
        }
        // found item that has already been installed and not deleted
        item.add_write(0).add_flags(delete_tag);
        // add a write to size item of the current size minus one
        // because we will delete the element from the tree
        change_size_offset(-1);
        return 1;

    // ABSENT ERASE
    } else {
#if DEBUG
        stats_.absent_delete++;
#endif
        return 0;
    }
}

template <typename K, typename T>
inline void RBTree<K, T>::lock(TransItem& item) {
    if (item.key<void*>() == size_key_) {
        lock(&sizeversion_);
    } else if (item.key<void*>() == tree_key_) {
        lock(&treeversion());
    } else { 
        auto node = item.key<wrapper_type*>();
        lock(&node->version());
    }
}
    
template <typename K, typename T>
inline void RBTree<K, T>::unlock(TransItem& item) {
    if (item.key<void*>() == size_key_) {
        unlock(&sizeversion_);
    } else if (item.key<void*>() == tree_key_) {
        unlock(&treeversion());
    } else {
        auto node = item.key<wrapper_type*>();
        unlock(&node->version());
    }
}
   
template <typename K, typename T>
inline bool RBTree<K, T>::check(const TransItem& item, const Transaction& trans) {
    auto e = item.key<uintptr_t>();
    bool is_treekey = ((uintptr_t)e == (uintptr_t)tree_key_);
    bool is_sizekey = ((uintptr_t)e == (uintptr_t)size_key_);
    bool is_structured = (e & uintptr_t(1)) && !is_treekey;
    Version read_version = item.read_value<Version>();
    Version curr_version;

    // set up the correct current version to check: either sizeversion, treeversion, item version, or nodeversion
    if (is_sizekey) {
        curr_version = sizeversion_;
    } else if (is_treekey) {
        curr_version = treeversion();
    } else if (is_structured) {
        wrapper_type* n = reinterpret_cast<wrapper_type*>(e & ~uintptr_t(1));
        curr_version = n->nodeversion();
#if DEBUG
        TransactionTid::lock(::lock);
        printf("\t#read %p nv 0x%lx, exp %lx\n", n, curr_version, read_version);
        TransactionTid::unlock(::lock);
#endif
    } else {
        curr_version = reinterpret_cast<wrapper_type*>(e)->version();
    }

    bool same_version;
    if (is_structured) {
        same_version = (read_version == curr_version);
    } else {
        same_version = (read_version ^ curr_version) <= TransactionTid::lock_bit;
    }
    bool not_locked = !is_locked(curr_version) || item.has_lock(trans);
#if DEBUG
    bool check_fails = !(same_version && not_locked);
    if (check_fails && !is_sizekey && !is_treekey) {
        wrapper_type* node = reinterpret_cast<wrapper_type*>(e & ~uintptr_t(1));
        int k_ = node? node->key() : 0;
        int v_ = node? node->writeable_value() : 0;
        TransactionTid::lock(::lock);
        printf("Check failed at TItem %p (key=%d, val=%d)\n", (void *)e, k_, v_);
        TransactionTid::unlock(::lock);
    }
    TransactionTid::lock(::lock);
    if (!same_version)
        printf("\tVersion mismatch: %lx -> %lx\n", read_version, curr_version);
    if (!not_locked)
        printf("\tVersion locked\n");
    TransactionTid::unlock(::lock);
#endif
    return same_version && not_locked;
}

// key-versionedvalue pairs with the same key will have two different items
template <typename K, typename T>
inline void RBTree<K, T>::install(TransItem& item, const Transaction& t) {
    (void) t;
    // we don't need to check for nodeversion updates because those are done during execution
    auto e = item.key<wrapper_type*>();
    // we did something to an empty tree, so update treeversion
    if ((void*)e == (wrapper_type*)tree_key_) {
        assert(is_locked(treeversion()));
        TransactionTid::inc_invalid_version(treeversion());
    // we changed the size of the tree, so update size
    } else if ((void*)e == (wrapper_type*)size_key_) {
        assert(is_locked(sizeversion_));
        size_ += item.template write_value<ssize_t>();
        TransactionTid::inc_invalid_version(sizeversion_);
        assert((ssize_t)size_ >= 0);
    } else {
        assert(is_locked(e->version()));
        assert(((uintptr_t)e & 0x1) == 0);
        bool deleted = has_delete(item);
        bool inserted = has_insert(item);
        // should never be both deleted and inserted...
        // sanity check to make sure we handled read_my_writes correctly
        assert(!(deleted && inserted));
        // actually erase the element when installing the delete
        if (deleted) {
            // actually erase
            // TODO get rid of treelock
            lock(&treelock_);
            // this increments the valueversion and nodeversion of the deleted node
            wrapper_tree_.erase(*e);
            erase_inserted(&e->rblinks_.valueversion_);
            Transaction::rcu_free(e);
            // increment value version 
            unlock(&treelock_);
        } else if (inserted) {
            // BUMMER...
            e->writeable_value() = item.template write_value<T>();
            erase_inserted(&e->version());
        // updated
        } else { 
            // already checked that value version has not changed (i.e. no one else deleted)
            e->writeable_value() = item.template write_value<T>(); 
            TransactionTid::inc_invalid_version(e->version());
        }
    }
}

template <typename K, typename T>
inline void RBTree<K, T>::cleanup(TransItem& item, bool committed) {
    if (!committed) {
        // if item has been tagged deleted or structured, don't need to do anything 
        // if item has been tagged inserted, then we erase the item
        if (has_insert(item) || has_delete(item)) {
            auto e = item.key<wrapper_type*>();
            assert(((uintptr_t)e & 0x1) == 0);
            if (!is_inserted(e->version()))
                return;
            // XXX remove these locks here
            lock(&treelock_);
            wrapper_tree_.erase(*e);
            erase_inserted(&e->rblinks_.valueversion_);
            Transaction::rcu_free(e);
            unlock(&treelock_);
        }
    }
}

template <typename K, typename T>
bool RBTree<K, T>::nontrans_insert(const K& key, const T& value) {
    wrapper_type idx_pair(rbpair<K, T>(key, value));
    auto results = wrapper_tree_.find_any(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()),
            true);
    auto pair = results.first;
    wrapper_type* x = pair.first.node();
    bool found = pair.second;
    if (!found) {
        rbnodeptr<wrapper_type> p = pair.first;
        wrapper_type* n = (wrapper_type*)malloc(sizeof(wrapper_type));
        new (n) wrapper_type(rbpair<K, T>(key, value));
        erase_inserted(&n->version());
        bool side = (x == nullptr)? false : (wrapper_tree_.r_.node_compare(*n, *x) > 0);
        wrapper_tree_.insert_commit(n, p, side);
    }
    return !found;
}

template <typename K, typename T>
bool RBTree<K, T>::nontrans_contains(const K& key) {
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = wrapper_tree_.find_any(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()), true);
    return results.first.second;
}

template <typename K, typename T>
T RBTree<K, T>::nontrans_find(const K& key) {
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = wrapper_tree_.find_any(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()), true);
    auto pair = results.first;
    bool found = pair.second;
    if (found) {
        return pair.first.node()->writeable_value();
    } else {
        return T();
    }
}

template <typename K, typename T>
bool RBTree<K, T>::nontrans_remove(const K& key) {
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = wrapper_tree_.find_any(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()), true);
    auto pair = results.first;
    bool found = pair.second;
    if (found) {
        wrapper_type* n = pair.first.node();
        wrapper_tree_.erase(*n);
        free(n);
    }
    return found;
}


#if DEBUG 
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
