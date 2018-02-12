#pragma once

#include <cassert>
#include <utility>
#include "TaggedLow.hh"
#include "Interface.hh"
#include "TWrapped.hh"
#include "RBTreeInternal.hh"

#ifndef STO_NO_STM
#include "Transaction.hh"
#endif

#define DEBUG 0
#if DEBUG
extern TransactionTid::type lock;
#endif

template <typename K, typename T, bool GlobalSize> class RBTreeIterator;
template <typename K, typename T, bool GlobalSize> class RBTree;

template <typename P>
class rbwrapper : public P {
  public:
    typedef typename P::key_type key_type;
    typedef typename P::value_type value_type;

    explicit inline rbwrapper(const P& x)
    : P(x) {
    }
    inline const P& rbpair() const {
        return *this;
    }
    inline P& mutable_rbpair() {
        return *this;
    }
    rblinks<rbwrapper<P> > rblinks_;
};

// Define a custom key-value pair type that contains versions and also
// has proper comparison methods
template <typename K, typename T>
class rbpair {
public:
    typedef TWrapped<std::pair<const K, T>> wrapped_pair;
    typedef typename wrapped_pair::version_type version_type;
    typedef typename version_type::type raw_version;
    typedef K key_type;
    typedef T value_type;

    static constexpr TransactionTid::type insert_bit = TransactionTid::user_bit;

/*
    template <typename... Args>
    explicit rbpair(Args&&... args)
        : pair_(std::forward<Args>(args)...), vers_(TransactionTid::increment_value + insert_bit) {}
*/

    explicit rbpair(const K& key, const T& value)
    : key_(key), val_(value),
      vers_(Sto::initialized_tid() + insert_bit),
      nodevers_(Sto::initialized_tid()), hohvers_() {}
    explicit rbpair(std::pair<const K, T>& kvp)
    : key_(kvp.first), val_(kvp.second),
      vers_(Sto::initialized_tid() + insert_bit),
      nodevers_(Sto::initialized_tid()), hohvers_() {}

    // version getters
    version_type& version() {
        return vers_;
    }
    const version_type& version() const {
        return vers_;
    }
    version_type& nodeversion() {
        return nodevers_;
    }
    const version_type& nodeversion() const {
        return nodevers_;
    }

    // transactional access to key-value pair
    void lock() {
        vers_.lock();
    }
    void unlock() {
        if (vers_.is_locked_here())
            vers_.unlock();
    }
    bool check(const TransItem& item) {
        return item.check_version(vers_);
    }
    void install(TransItem& item, const Transaction& txn) {
        val_.write(item.template write_value<T>());
        txn.set_version(vers_);
    }

    // transactional access to node version
    void lock_nv() {
        nodevers_.lock();
    }
    void unlock_nv() {
        if (nodevers_.is_locked_here())
            nodevers_.unlock();
    }
    bool check_nv(const TransItem& item) {
        return item.check_version(nodevers_);
    }
    void install_nv(const Transaction& txn) {
        assert(nodevers_.is_locked_here());
        txn.set_version(nodevers_);
    }

    // key access and comparisons
    const K& key() const {
        return key_;
    }
    bool operator<(const rbpair& rhs) const {
        return (key() < rhs.key());
    }
    bool operator>(const rbpair& rhs) const {
        return (key() > rhs.key());
    }

    T read_value(TransProxy& item, const version_type& version) const {
        return val_.read(item, version);
    }
    T& writeable_value() {
        return val_.access();
    }

    // hand-over-hand validation stuff
    void lock_hohversion() {
        hohvers_.lock();
    }
    void unlock_hohversion() {
        assert(hohvers_.is_locked_here());
        TVersion nv(hohvers_.value() + TransactionTid::increment_value);
        hohvers_.set_version_unlock(nv);
    }
    version_type unlocked_hohversion() const {
        version_type v = hohvers_;
        while (v.is_locked()) {
            acquire_fence();
            v = hohvers_;
        }
        return v;
    }
    bool validate_hohversion(version_type old_v) const {
        acquire_fence();
        return (hohvers_ == old_v);
    }

private:
    // key-value pair associated with a version for the data
    const K key_;
    TWrapped<T> val_;
    version_type vers_;
    version_type nodevers_;
    version_type hohvers_;
};

template <typename K, typename T, bool GlobalSize> class RBProxy;

template <typename K, typename T, bool GlobalSize>
class RBTree 
#ifndef STO_NO_STM
: public TObject
#endif
{
    friend class RBTreeIterator<K, T, GlobalSize>;
    friend class RBProxy<K, T, GlobalSize>;

    typedef TransactionTid::type RWVersion;
    typedef TWrapped<std::pair<const K, T>> wrapped_pair;
    typedef typename wrapped_pair::version_type Version;
    typedef Version version_type;

    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr TransactionTid::type insert_bit = TransactionTid::user_bit;

    typedef RBTreeIterator<K, T, GlobalSize> iterator;
    typedef const RBTreeIterator<K, T, GlobalSize> const_iterator;

public:
    RBTree() {
        sizeversion_ = 0;
        size_ = 0;
        treelock_ = 0;
#if DEBUG
        stats_ = {0,0,0,0,0,0};
#endif
    }

    typedef rbwrapper<rbpair<K, T>> wrapper_type;
    typedef rbtree<wrapper_type> internal_tree_type;

    typedef rbnodeptr<wrapper_type> internal_ptr_type;
    typedef std::tuple<wrapper_type*, Version> node_info_type;
    typedef std::pair<node_info_type, node_info_type> boundaries_type;

    // capacity 
    inline size_t size() const;
    // lookup
    inline size_t count(const K& key) const;
    // element access
    inline RBProxy<K, T, GlobalSize> operator[](const K& key);
    // modifiers
    inline size_t erase(const K& key);

    // STAMP-compatible nontransactional methods
    bool nontrans_insert(const K& key, const T& value);
    bool nontrans_contains(const K& key);
    bool nontrans_remove(const K& key);
    bool nontrans_remove(const K& key, T& oldval);
    T nontrans_find(const K& key); // returns T() if not found, works for STAMP
    bool nontrans_find(const K& key, T& val);

    bool stamp_insert(const K& key, const T& val);
    T stamp_find(const K& key);

/*
    void lock(wrapped_pair *e) {
        e->lock();
    }
    void unlock(wrapped_pair *e) {
        e->unlock();
    }
*/

  __attribute__((always_inline)) std::tuple<wrapper_type*, Version, bool, boundaries_type> verified_lookup(rbwrapper<rbpair<K, T>>& rbkvp) const {
        do {
            auto initial = treelock_;
	    fence();
	    if (TransactionTid::is_locked(initial)) {
                relax_fence();
                continue;
            }
	    auto results = wrapper_tree_.find_any(rbkvp,
                             rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
	    fence();
	    if (initial == treelock_)
                return results;
	    relax_fence();
	} while(1);
    }

#ifndef STO_NO_STM
/*
    // iterators
    iterator begin() {
        lock(&treelock_);
        wrapper_type* start = wrapper_tree_.r_.limit_[0];
        if (start == nullptr) {
            // tree empty, read tree version
            // TODO(nate): this would race without the lock
            Sto::item(this, tree_key_).observe(treeversion_);
        } else {
            // READ-MY-WRITES: skip our own deletes!
            while(has_delete(Sto::item(this, start))) {
                Sto::item(this, (reinterpret_cast<uintptr_t>(start)|0x1)).observe(start->nodeversion());
                start = rbalgorithms<wrapper_type>::next_node(start);
            }
            if (start != nullptr) {
                if (is_phantom_node(start)) {
                    unlock(&treelock_);
                    Sto::abort();
                }
                // valid start node, read nodeversion
                Sto::item(this, (reinterpret_cast<uintptr_t>(start)|0x1)).observe(start->nodeversion());
            }
        }
        unlock(&treelock_);
        return iterator(this, start);
    }

    iterator end() {
        return iterator(this, nullptr);
    }

*/
    bool lock(TransItem& item, Transaction&) override;
    void unlock(TransItem& item) override;
    bool check(TransItem& item, Transaction& trans) override;
    void install(TransItem& item, Transaction& t) override;
    void cleanup(TransItem& item, bool committed) override;
#if DEBUG
    void print_absent_reads();
#endif
    void print(std::ostream& w, const TransItem& item) const override;

private:
    size_t debug_size() const {
        return wrapper_tree_.size();
    }
/*
    // XXX should we inline these?
    inline wrapper_type* get_next(wrapper_type* node) {
        lock(&treelock_);
        wrapper_type* next_node = rbalgorithms<wrapper_type>::next_node(node);
        // READ-MY-WRITES: skip our own deletes
        while (has_delete(Sto::item(this, next_node))) {
            // add read of nodeversions of all deleted nodes; gosh, read-my-write is really a nightmare...
            Sto::item(this, (reinterpret_cast<uintptr_t>(next_node) | 0x1)).observe(next_node->nodeversion());
            next_node = rbalgorithms<wrapper_type>::next_node(next_node);
        }

        if (next_node != nullptr) {
            if (is_phantom_node(next_node)) {
                unlock(&treelock_);
                Sto::abort();
            }
            Sto::item(this, (reinterpret_cast<uintptr_t>(next_node)|0x1)).observe(next_node->nodeversion());
        }
        Sto::item(this, (reinterpret_cast<uintptr_t>(node)|0x1)).observe(node->nodeversion());
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
            Sto::item(this, (reinterpret_cast<uintptr_t>(prev_node) | 0x1)).observe(prev_node->nodeversion());
            prev_node = rbalgorithms<wrapper_type>::prev_node(prev_node);
        }
        // check again after reading-my-writes...
        if (prev_node == nullptr) {
            unlock(&treelock_);
            Sto::abort();
        }

        if (is_phantom_node(prev_node)) {
            unlock(&treelock_);
            Sto::abort();
        }
        if (node) {
            Sto::item(this, (reinterpret_cast<uintptr_t>(node)|0x1)).observe(node->nodeversion());
        }
        if (prev_node) {
            Sto::item(this, (reinterpret_cast<uintptr_t>(prev_node)|0x1)).observe(prev_node->nodeversion());
        }
        unlock(&treelock_);
        return prev_node;
    }
*/

    // A (hard) phantom node is a node that's being inserted but not yet
    // committed by another transaction. It should be treated as invisible
    inline bool is_phantom_node(wrapper_type* node, Version val_ver) const {
        auto item = Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), node);
        return (is_inserted(val_ver) && !has_insert(item) && !has_delete(item));
    }

    // A soft phantom node is a node that's marked inserted by the current
    // transaction. Its value information is visible (and only visible) to
    // the current transaction
    inline bool is_soft_phantom(wrapper_type* node) const {
        Version& val_ver = node->version();
        auto item = Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), node);
        return (is_inserted(val_ver) && (has_insert(item) || has_delete(item)));
    }

    // increment or decrement the offset size of the transaction's tree
    inline void change_size_offset(ssize_t delta) {
        if (!GlobalSize)
            return;
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

    // *Read-only* lookup operation
    // Find and return a pointer to the rbwrapper. Abort if value inserted and not yet committed (by another txn).
    // return values: (node*, version, found, boundary), boundary only valid if !found
    // XXX node can point to the *found* node or its immediate parent or nothing (in case of empty tree)
    // version can be the node's value version or treeversion_ (in case of empty tree)
    // NOTE: this function must be surrounded by a lock in order to ensure we add the correct nodeversions
    inline std::tuple<wrapper_type*, Version, bool, boundaries_type>
    find_or_abort(rbwrapper<rbpair<K, T>>& rbkvp) const {
        auto results = verified_lookup(rbkvp);

        // extract information from results
        wrapper_type* x = std::get<0>(results);
        Version val_ver = std::get<1>(results);
        bool found = std::get<2>(results);
        boundaries_type& boundaries = std::get<3>(results);

        // PRESENT GET
        if (found) {
            auto item = Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), x);
            // check if item is inserted by not committed yet 
            if (is_inserted(val_ver)) {
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
            // add a read of the value version for a present get
            item.observe(val_ver);
        
        // ABSENT GET
        } else {
            // add a read of treeversion if empty tree
            if (!x) {
                Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), tree_key_).observe(val_ver);
            }

            // add reads of boundary nodes, marking them as nodeversion ptrs
            for (unsigned int i = 0; i < 2; ++i) {
                node_info_type& binfo = (i == 0)? boundaries.first : boundaries.second;
                wrapper_type* n = std::get<0>(binfo);
                Version v = std::get<1>(binfo);
                if (n) {
#if DEBUG
                    TransactionTid::lock(::lock);
                    printf("\t#Tracking boundary 0x%lx (k %d), nv 0x%lx\n", (unsigned long)n, n->key(), v);
                    TransactionTid::unlock(::lock);
#endif
                    Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this),
                                    (reinterpret_cast<uintptr_t>(n)|0x1)).observe(v);
                }
            }
        }
        // item was committed or DNE, so return results
        return results;
    }

    // Read-write lookup operation
    // Returns: <node : wrapper_type*, ver : Version, found : bool, boundary : boundaries_type, parent : node_info_type>
    // Atomic: looks for rbkvp.key() in the rbtree and inserts the key with an empty value if not found; aborts if found
    // a phantom node
    // @node: always points to the found/inserted node
    // @ver: if inserted, nodeversion of @node; otherwise value version of @node
    // @boundary: boundary nodes info (*pre-insertion* state) of the inserted/found node
    // @parent: parent of the returned node, prior to any insertions
    inline std::tuple<wrapper_type*, Version, bool, boundaries_type, node_info_type>
    find_or_insert(wrapper_type& rbkvp) {
        lock_write(&treelock_);
        auto results = wrapper_tree_.find_insert(rbkvp,
                           rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
        unlock_write(&treelock_);

        bool found = std::get<2>(results);
        wrapper_type* ans = std::get<0>(results);
        Version ver = std::get<1>(results);
        if (found && is_phantom_node(ans, ver)) {
            Sto::abort();
        }
        return results;
    }

    // DEPRECATED
    // Insert nonexistent key with empty value
    // return value is a pointer to the inserted node 
    inline wrapper_type* insert_absent(rbnodeptr<wrapper_type> found_p, const K& key) {
#if DEBUG
            stats_.absent_insert++;
#endif
            // use in-place constructor so that we don't mix up c++'s new and c's free()
            // XXX(nate): we'd need to an rcu delete if T is a nontrivial type.
            wrapper_type* n = (wrapper_type*)malloc(sizeof(wrapper_type));
            new (n) wrapper_type(rbpair<K, T>(key, T()));
            // insert new node under parent
            bool side = (found_p.node() == nullptr)? false :
                    wrapper_tree_.r_.node_compare(*n, *found_p.node()) > 0;
            wrapper_tree_.insert_commit(n, found_p, side);
            // rbnodeptr<wrapper_type> p = wrapper_tree_.insert(*n);
            // invariant: the node's insert_bit should be set
            assert(is_inserted(n->version()));
            // if tree is empty (i.e. no parent), we increment treeversion 
            if (found_p.node() == nullptr) {
                Sto::item(this, tree_key_).add_write(0);
            // else we increment the parent version 
            } else {
                auto versions = found_p.node()->inc_nodeversion();
                auto item = Sto::item(this, reinterpret_cast<uintptr_t>(found_p.node()) | 1);
                // update our own read if necessary
                if (item.has_read()) {
                    item.update_read(versions.first, versions.second);
                }
            }
            // add write and insert flag of item (value of rbpair) with @value
            Sto::item(this, n).add_write(T()).add_flags(insert_tag);
            // if we actually changed begin, increment startversion_ 
            //if (wrapper_tree_.r_.limit_[0] == n) {
            //    Sto::item(this, start_key_).add_write(0);
            //}
            // add a write to size with incremented value
            change_size_offset(1);
            return n;
    }

    // Insert key and empty value if key does not exist 
    // If key exists, then add a read of the item version and return the node
    // return value is a reference to the found or inserted node 
    inline wrapper_type* insert(const K& key) {
        rbwrapper<rbpair<K, T>> node( rbpair<K, T>(key, T()) );
        auto results = this->find_or_insert(node);
        wrapper_type* x = std::get<0>(results);
        Version ver = std::get<1>(results);
        bool found = std::get<2>(results);
        boundaries_type& boundaries = std::get<3>(results);
        node_info_type& pinfo = std::get<4>(results);
        wrapper_type* p = std::get<0>(pinfo);
        // p_ver is always nodeversion (not used right now)
        //Version p_ver = std::get<1>(pinfo);

        // INSERT: kvp did not exist
        // @ver is *nodeversion*
        if (!found) {
#if DEBUG
            stats_.absent_insert++;
#endif
            wrapper_type* lhs = std::get<0>(boundaries.first);
            wrapper_type* rhs = std::get<0>(boundaries.second);
            if (p == nullptr) {
                // tree was empty, increment treeversion at COMMIT TIME
                assert(lhs == nullptr && rhs == nullptr);
                Sto::item(this, tree_key_).add_write(0);
            } else {
                // mark to update nodeversion at commit time
                auto item = Sto::item(this, reinterpret_cast<uintptr_t>(p) | 0x1);
                item.add_write(0);

                // add the newly inserted node to boundary node set if it is adjacent to any tracked
                // boundary node
                if (Sto::item(this, reinterpret_cast<uintptr_t>(lhs) | 0x1).has_read()
                    || Sto::item(this, reinterpret_cast<uintptr_t>(rhs) | 0x1).has_read()) {
                    Sto::item(this, reinterpret_cast<uintptr_t>(x) | 0x1).observe(ver);
                }
            }
            Sto::item(this, x).add_write(T()).add_flags(insert_tag);
            change_size_offset(1);
            return x;
        }
        // UPDATE: kvp is already inserted into the tree
        // @ver is *value version*
        else {
#if DEBUG
            stats_.present_insert++;
#endif
            auto item = Sto::item(this, x);
            
            // insert-my-delete
            if (has_delete(item)) {
                item.clear_flags(delete_tag);
                // recover from delete-my-insert (engineer's induction all over the place...)
                if (is_inserted(ver)) {
                    item.add_flags(insert_tag);
                    // x->writeable_value() = T();
                }
                // overwrite value
                item.add_write(T());
                // we have to update the value of the size we will write
                change_size_offset(1);
                return x;
            }
            // operator[] on RHS (THIS IS A READ!)
            // don't need to add a write to size because size isn't changing
            // STO won't add read of items in our write set

            // This is not ideal because a blind write also reads this version
            // It's a compromise since we don't want to acquire any more locks
            // at install time (to avoid deadlocks)
            item.observe(ver);
            return x;
        }
    }

#endif /* !STO_NO_STM */
#ifdef STO_NO_STM
    static void change_size_offset(int){}
#endif

    static void lock_read(RWVersion *v) {TransactionTid::lock_read(*v);}
    static void lock_write(RWVersion *v) {TransactionTid::lock_write(*v);}
    static void unlock_read(RWVersion *v) {TransactionTid::unlock_read(*v);}
    static void unlock_write(RWVersion *v) {
        TransactionTid::inc_write_version(*v);
        TransactionTid::unlock_write(*v);
    }

    static bool has_insert(const TransItem& item) {
        return item.flags() & insert_tag;
    }
    static bool has_delete(const TransItem& item) {
        return item.flags() & delete_tag;
    }
    static bool is_inserted(Version& v) {
        return v.value() & insert_bit;
    }
    static void erase_inserted(Version& v) {
        v.value() &= ~insert_bit;
    }

    internal_tree_type wrapper_tree_;
    // only add a write to size if we erase or do an absent insert
    size_t size_;
    Version sizeversion_;
    // XXX: this isn't actually a rwlock anymore so we could just make it a
    // normal tid or something
    mutable RWVersion treelock_;
    // used to mark whether a key is for the tree structure (for tree version checks)
    // or a pointer (which will always have the lower 3 bits as 0)
    static constexpr uintptr_t tree_bit = 1U<<0;
    static constexpr uintptr_t tree_key_ = tree_bit;
    static constexpr uintptr_t size_bit = 1U<<1;
    static constexpr uintptr_t size_key_ = size_bit;
    static constexpr uintptr_t start_bit = 1U<<2;
    static constexpr uintptr_t start_key_ = start_bit;
#if DEBUG
    mutable struct stats {
        int absent_insert, absent_delete, absent_count;
        int present_insert, present_delete, present_count;
    } stats_;
#endif
};

#ifndef STO_NO_STM

template<typename K, typename T, bool GlobalSize>
class RBTreeIterator : public std::iterator<std::bidirectional_iterator_tag, rbwrapper<rbpair<K, T>>> {
public:
    typedef rbwrapper<rbpair<K, T>> wrapper;
    typedef RBTreeIterator<K, T, GlobalSize> iterator;
    typedef RBProxy<K, T, GlobalSize> proxy_type;
    typedef std::pair<const K, proxy_type> proxy_pair_type;

    RBTreeIterator(RBTree<K, T, GlobalSize> * tree, wrapper* node) : tree_(tree), node_(node), proxy_pair_(nullptr) {
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
        Sto::item(tree_, node_).observe(node_->version());
        this->update_proxy_pair();
        return *proxy_pair_;
    }

    proxy_pair_type* operator->() {
        // add a read of the version to make sure the value hasn't changed at commit time
        Sto::item(tree_, node_).observe(node_->version());
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
        RBTreeIterator<K, T, GlobalSize> clone(*this);
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
        RBTreeIterator<K, T, GlobalSize> clone(*this);
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
            proxy_pair_type* new_pair = new std::pair<const K, RBProxy<K, T, GlobalSize>>(node_->key(), proxy_type(*tree_, node_));
            proxy_pair_ = new_pair;
            return;
        }
    }

    RBTree<K, T, GlobalSize> * tree_;
    wrapper* node_;
    proxy_pair_type* proxy_pair_;
};

// STL-ish interface wrapper returned by RBTree::operator[]
// differentiate between reads and writes
template <typename K, typename T, bool GlobalSize>
class RBProxy {
public:
    typedef RBTree<K, T, GlobalSize> transtree_t;
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
            return node_->read_value(item, node_->version());
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

template <typename K, typename T, bool GlobalSize>
inline size_t RBTree<K, T, GlobalSize>::size() const {
    always_assert(GlobalSize);
    auto size_item = Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), size_key_);
    if (!size_item.has_read()) {
        size_item.observe(sizeversion_);
    }

    ssize_t offset = (size_item.has_write()) ? size_item.template write_value<ssize_t>() : 0;
    return size_ + offset;
}

template <typename K, typename T, bool GlobalSize>
inline size_t RBTree<K, T, GlobalSize>::count(const K& key) const {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));

    // find_or_abort() tracks boundary nodes if key is absent
    auto results = find_or_abort(idx_pair);

    wrapper_type* node = std::get<0>(results);
    bool found = std::get<2>(results);
#if DEBUG
    (!found) ? stats_.absent_count++ : stats_.present_count++;
#endif
    if (found) {
        auto item = Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), node);
        if (has_delete(item)) {
            // read my deletes
            return 0;
        }
    }
    return (found) ? 1 : 0;
}

template <typename K, typename T, bool GlobalSize>
inline RBProxy<K, T, GlobalSize> RBTree<K, T, GlobalSize>::operator[](const K& key) {
    // either insert empty value or return present value
    auto node = insert(key);
    return RBProxy<K, T, GlobalSize>(*this, node);
}

template <typename K, typename T, bool GlobalSize>
inline size_t RBTree<K, T, GlobalSize>::erase(const K& key) {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    // add a read of boundary nodes if absent erase
    auto results = find_or_abort(idx_pair);
    wrapper_type* x = std::get<0>(results);
    bool found = std::get<2>(results);
    Version ver = std::get<1>(results);
   
    // PRESENT ERASE
    if (found) {
#if DEBUG
        stats_.present_delete++;
#endif
        auto item = Sto::item(this, x);
        
        // item marked as inserted and not yet installed
        if (is_inserted(ver)) {
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

template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::lock(TransItem& item, Transaction& txn) {
    if (item.key<uintptr_t>() == size_key_)
        return txn.try_lock(item, sizeversion_);
    else if (item.key<uintptr_t>() == tree_key_)
        return txn.try_lock(item, wrapper_tree_.treeversion_);
    else {
        uintptr_t x = item.key<uintptr_t>();
        wrapper_type* n = reinterpret_cast<wrapper_type*>(x & ~uintptr_t(1));
        if (((!(x & 1) && !has_delete(item))
             || n->nodeversion().is_locked_here()
             || txn.try_lock(item, n->nodeversion()))
            && ((x & 1) || txn.try_lock(item, n->version())))
            return true;
        else {
            n->unlock_nv();
            return false;
        }
    }
}

template <typename K, typename T, bool GlobalSize>
void RBTree<K, T, GlobalSize>::unlock(TransItem& item) {
    if (item.key<uintptr_t>() == size_key_) {
        sizeversion_.unlock();
    } else if (item.key<uintptr_t>() == tree_key_) {
        wrapper_tree_.treeversion_.unlock();
    } else {
        uintptr_t x = item.key<uintptr_t>();
        wrapper_type* n = reinterpret_cast<wrapper_type*>(x & ~uintptr_t(1));
        if (!(x & 1)) {
            n->unlock();
            if (!has_delete(item))
                return;
        }
        n->unlock_nv();
    }
}

template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::check(TransItem& item, Transaction&) {
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
        curr_version = wrapper_tree_.treeversion_;
    } else if (is_structured) {
        wrapper_type* n = reinterpret_cast<wrapper_type*>(e & ~uintptr_t(1));
        return n->check_nv(item);
#if DEBUG
        TransactionTid::lock(::lock);
        printf("\t#read %p nv 0x%lx, exp %lx\n", n, curr_version, read_version);
        TransactionTid::unlock(::lock);
#endif
    } else {
        wrapper_type* n = reinterpret_cast<wrapper_type*>(e);
        return n->check(item);
    }
    fence();

    // XXX this is now wrong -- treeversion and sizeversion currently doesn't conform to
    // the TVersion interface
    if (curr_version.check_version(read_version))
        return true;
#if DEBUG
    if (!is_sizekey && !is_treekey) {
        wrapper_type* node = reinterpret_cast<wrapper_type*>(e & ~uintptr_t(1));
        int k_ = node? node->key() : 0;
        int v_ = node? node->writeable_value() : 0;
        TransactionTid::lock(::lock);
        printf("Check failed at TItem %p (key=%d, val=%d)\n", (void *)e, k_, v_);
        TransactionTid::unlock(::lock);
    }
    TransactionTid::lock(::lock);
    if ((curr_version ^ read_version) >= (TransactionTid::lock_bit << 1))
        printf("\tVersion mismatch: %lx -> %lx\n", read_version, curr_version);
    if (TransactionTid::is_locked_elsewhere(curr_version))
        printf("\tVersion locked elsewhere\n");
    TransactionTid::unlock(::lock);
#endif
    return false;
}

// key-versionedvalue pairs with the same key will have two different items
template <typename K, typename T, bool GlobalSize>
void RBTree<K, T, GlobalSize>::install(TransItem& item, Transaction& t) {
    // we don't need to check for nodeversion updates because those are done during execution
    wrapper_type* e = item.key<wrapper_type*>();
    // we did something to an empty tree, so update treeversion
    if (e == (wrapper_type*)tree_key_) {
        assert(wrapper_tree_.treeversion_.is_locked_here());
        t.set_version_unlock(wrapper_tree_.treeversion_, item);
    // we changed the size of the tree, so update size
    } else if (e == (wrapper_type*)size_key_) {
        always_assert(GlobalSize);
        assert(sizeversion_.is_locked_here());
        size_ += item.template write_value<ssize_t>();
        t.set_version_unlock(sizeversion_, item);
        assert((ssize_t)size_ >= 0);
    } else if (uintptr_t(e) & uintptr_t(1)) {
        auto n = reinterpret_cast<wrapper_type*>(uintptr_t(e) & ~uintptr_t(1));
        n->install_nv(t);
    } else {
        assert(e->version().is_locked_here());
        assert(((uintptr_t)e & 0x1) == 0);
        bool deleted = has_delete(item);
        bool inserted = has_insert(item);
        // should never be both deleted and inserted...
        // sanity check to make sure we handled read_my_writes correctly
        assert(!(deleted && inserted));
        // actually erase the element when installing the delete
        if (deleted) {
            // actually erase
            lock_write(&treelock_);
            wrapper_tree_.erase(*e);
            unlock_write(&treelock_);

            e->version().set_version(t.commit_tid());
            e->install_nv(t);
            Transaction::rcu_free(e);
        } else {
            // inserts/updates should be handled the same way
            e->install(item, t);
        }
    }
}

template <typename K, typename T, bool GlobalSize>
void RBTree<K, T, GlobalSize>::cleanup(TransItem& item, bool committed) {
    if (!committed) {
        // if item has been tagged deleted or structured, don't need to do anything 
        // if item has been tagged inserted, then we erase the item
        if (has_insert(item) || has_delete(item)) {
            auto e = item.key<wrapper_type*>();
            assert(((uintptr_t)e & 0x1) == 0);
            if (!is_inserted(e->version()))
                return;
            lock_write(&treelock_);
            wrapper_tree_.erase(*e);
            unlock_write(&treelock_);
            // invalidate the nodeversion after we erase
            e->nodeversion().set_nonopaque();
            Transaction::rcu_free(e);
        }
    }
}

template <typename K, typename T, bool GlobalSize>
void RBTree<K, T, GlobalSize>::print(std::ostream& w, const TransItem& item) const {
    w << "{RBTree<" << typeid(K).name() << "," << typeid(T).name() << "> " << (void*) this;
    if (item.key<uintptr_t>() == size_key_)
        w << ".size";
    else if (item.key<uintptr_t>() == tree_key_)
        w << ".tree";
    else {
        uintptr_t x = item.key<uintptr_t>();
        if (x & 1)
            w << "." << (void*) (x & ~uintptr_t(1)) << "V";
        else
            w << "." << (void*) x;
    }
    if (item.has_read())
        w << " R" << item.read_value<version_type>();
    if (item.has_write()) {
        if (item.key<uintptr_t>() == size_key_)
            w << " Δ" << item.write_value<ssize_t>();
        else if (item.key<uintptr_t>() == tree_key_
                 || (item.key<uintptr_t>() & 1))
            w << " Δ";
        else
            w << " =" << item.write_value<T>();
    }
    w << "}";
}

#endif /* !STO_NO_STM */

// logN (instead of 2logN) insertion for STAMP
template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::stamp_insert(const K& key, const T& value) {
    rbwrapper<rbpair<K, T>> node( rbpair<K, T>(key, value) );
    auto results = this->find_or_insert(node);
    wrapper_type* x = std::get<0>(results);
    Version ver = std::get<1>(results);
    bool found = std::get<2>(results);
    boundaries_type& boundaries = std::get<3>(results);
    node_info_type& pinfo = std::get<4>(results);
    wrapper_type* p = std::get<0>(pinfo);
    // p_ver is always nodeversion
    Version p_ver = std::get<1>(pinfo);

    // INSERT: kvp did not exist
    // @ver is *nodeversion*
    if (!found) {
        wrapper_type* lhs = std::get<0>(boundaries.first);
        wrapper_type* rhs = std::get<0>(boundaries.second);
        if (p == nullptr) {
            // tree was empty, increment treeversion at COMMIT TIME
            assert(lhs == nullptr && rhs == nullptr);
            Sto::item(this, tree_key_).add_write(0);
        } else {
            // update txn's own read set if inserted under a tracked boundary node
            auto item = Sto::item(this, reinterpret_cast<uintptr_t>(p) | 0x1);
            item.add_write(0);

            // add the newly inserted node to boundary node set if it is adjacent to any tracked
            // boundary node
            if (Sto::item(this, reinterpret_cast<uintptr_t>(lhs) | 0x1).has_read()
                || Sto::item(this, reinterpret_cast<uintptr_t>(rhs) | 0x1).has_read()) {
                Sto::item(this, reinterpret_cast<uintptr_t>(x) | 0x1).observe(ver);
            }
        }
        Sto::item(this, x).add_write(value).add_flags(insert_tag);
        change_size_offset(1);
        return true;
    }
    // UPDATE: kvp is already inserted into the tree
    // @ver is *value version*
    else {
        auto item = Sto::item(this, x);

        // insert-my-delete
        if (has_delete(item)) {
            item.clear_flags(delete_tag);
            // recover from delete-my-insert (engineer's induction all over the place...)
            if (is_inserted(ver)) {
                // okay to directly update value since we are the only txn
                // who can access it
                item.add_flags(insert_tag);
                //x->get_raw_pair().second = value;
            }
            // overwrite value
            item.add_write(value);
            // we have to update the value of the size we will write
            change_size_offset(1);
            return true;
        }
        // operator[] on RHS (THIS IS A READ!)
        // don't need to add a write to size because size isn't changing
        // STO won't add read of items in our write set

        // This is not ideal because a blind write also reads this version
        // It's a compromise since we don't want to acquire any more locks
        // at install time (to avoid deadlocks)
        item.observe(ver);
        return false;
    }

}

template <typename K, typename T, bool GlobalSize>
T RBTree<K, T, GlobalSize>::stamp_find(const K& key) {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));

    // find_or_abort() tracks boundary nodes if key is absent
    // it also observes a value version if key is found
    auto results = find_or_abort(idx_pair);

    wrapper_type* node = std::get<0>(results);
    Version ver = std::get<1>(results);
    bool found = std::get<2>(results);
    if (found) {
        auto item = Sto::item(const_cast<RBTree<K, T, GlobalSize>*>(this), node);
        if (has_delete(item)) {
            // read my deletes
            return T();
        }
        return node->read_value(item, ver);
    } else {
        return T();
    }
}

template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::nontrans_insert(const K& key, const T& value) {
    lock_write(&treelock_);
    wrapper_type idx_pair(rbpair<K, T>(key, value));
    auto results = wrapper_tree_.find_or_parent(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
    bool found = std::get<1>(results);
    if (!found) {
        size_++;
        rbnodeptr<wrapper_type> p = std::get<0>(results);
        wrapper_type* n = (wrapper_type*)malloc(sizeof(wrapper_type));
        new (n) wrapper_type(rbpair<K, T>(key, value));
        erase_inserted(n->version());
        bool side = (p.node() == nullptr) ? false : (wrapper_tree_.r_.node_compare(*n, *p.node()) > 0);
        wrapper_tree_.insert_commit(n, p, side);
    }
    unlock_write(&treelock_);
    return !found;
}

template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::nontrans_contains(const K& key) {
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = verified_lookup(idx_pair);
    return std::get<2>(results);
}

template <typename K, typename T, bool GlobalSize>
T RBTree<K, T, GlobalSize>::nontrans_find(const K& key) {
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = verified_lookup(idx_pair);
    bool found = std::get<2>(results);
    // TODO: this isn't safe if value is nontrivial (doesn't apply to STAMP)
    T ret = found ? std::get<0>(results)->writeable_value() : T();
    return ret;
}

template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::nontrans_find(const K& key, T& val) {
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = verified_lookup(idx_pair);
    bool found = std::get<2>(results);
    if (found) {
        val = std::get<0>(results)->writeable_value();
    }
    return found;
}

template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::nontrans_remove(const K& key) {
    lock_write(&treelock_);
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = wrapper_tree_.find_any(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
    bool found = std::get<2>(results);
    if (found) {
        size_--;
        wrapper_type* n = std::get<0>(results);
        wrapper_tree_.erase(*n);
        free(n);
    }
    unlock_write(&treelock_);
    return found;
}

// same as normal nontrans_remove, but, if the key is successfully removed, oldval
// is set to the value of the key before removal
template <typename K, typename T, bool GlobalSize>
bool RBTree<K, T, GlobalSize>::nontrans_remove(const K& key, T& oldval) {
    lock_write(&treelock_);
    wrapper_type idx_pair(rbpair<K, T>(key, T()));
    auto results = wrapper_tree_.find_any(idx_pair,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
    bool found = std::get<2>(results);
    if (found) {
        size_--;
        wrapper_type* n = std::get<0>(results);
	// set the old value for the caller
	oldval = n->writeable_value();
        wrapper_tree_.erase(*n);
        free(n);
    }
    unlock_write(&treelock_);
    return found;
}


#if DEBUG 
template <typename K, typename T, bool GlobalSize>
inline void RBTree<K, T, GlobalSize>::print_absent_reads() {
    std::cout << "absent inserts: " << stats_.absent_insert << std::endl;
    std::cout << "absent deletes: " << stats_.absent_delete << std::endl;
    std::cout << "absent counts: " << stats_.absent_count << std::endl;
    std::cout << "present inserts: " << stats_.present_insert << std::endl;
    std::cout << "present deletes: " << stats_.present_delete << std::endl;
    std::cout << "present counts: " << stats_.present_count << std::endl;
    std::cout << "size: " << debug_size() << std::endl;
}
#endif
