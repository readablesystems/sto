#pragma once

#include <cassert>
#include <utility>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"
#include "RBTreeInternal.hh"

template <typename K, typename T>
class RBTree;

template <typename T>
class rbwrapper : public T {
  public:
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
    rblinks<rbwrapper<T> > rblinks_;
};

// Define a custom key-value pair type that contains versions and also
// has proper comparison methods
template <typename K, typename T>
class rbpair {
public:
    typedef TransactionTid::type Version;
    typedef versioned_value_struct<std::pair<K, T>> versioned_pair;
    typedef uint64_t version_type;

    static constexpr Version insert_bit = TransactionTid::user_bit1;

    explicit rbpair(const K& key, const T& value) :
        pair_(std::pair<K, T>(key, value),
              TransactionTid::increment_value + insert_bit) {}
    explicit rbpair(std::pair<K, T>& kvp) :
        pair_(kvp, TransactionTid::increment_value + insert_bit) {}

    inline const K& key() const {
        return pair_.read_value().first;
    }
    inline version_type& version() {
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
    static constexpr Version insert_bit = TransactionTid::user_bit1;
    static constexpr Version delete_bit = TransactionTid::user_bit1<<1;

public:
    RBTree() {
        treelock_ = 0;
    }
    
    // lookup
    typedef rbwrapper<rbpair<K, T>> wrapper_type;
    typedef rbtree<wrapper_type> internal_tree_type;
    inline size_t count(const K& key) const;

    // element access
    inline RBProxy<K, T> operator[](const K& key);
    
    // modifiers
    inline int erase(K& key);
    inline void insert(std::pair<K, T>& n);

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

private:
    // Find and return a pointer to the rbwrapper. Abort if value inserted and not yet committed.
    inline rbwrapper<rbpair<K, T>>* find_or_abort(rbwrapper<rbpair<K, T>>& rbkvp) {
        lock(&treelock_);
        auto x = wrapper_tree_.find_any(rbkvp,
            rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
        unlock(&treelock_);
        if (x) {
            // check if rbpair has version "inserted" (someone inserted w/out commit)
            if (is_inserted(x->version())) {
                auto item = Sto::item(this, x);
                // check if item was inserted by this transaction
                if (has_insert(item)) {
                    return x;
                } else {
                    // some other transaction inserted this node and hasn't committed
                    // XXX should we abort every time even just for a read?
                    Sto::abort();
                    return nullptr;
                }
            }
        }
        // item was committed or DNE, so return pointer
        return x;
    }

    // Insert <@key, @value>, optionally force an update if @key exists
    // return value is a reference to the value of the kvp
    inline T& insert_update(const K& key, const T& value, bool force) {
        auto node = rbwrapper<rbpair<K, T>>( rbpair<K, T>(key, T()) );
        auto x = this->find_or_abort(node);
        lock(&treelock_);
        if (x == nullptr) {
            auto n = new rbwrapper<rbpair<K, T> >(  rbpair<K, T>(key, value)  );
            wrapper_tree_.insert(*n);
            unlock(&treelock_);
            // add write and insert flag of item (value of rbpair) with @value
            Sto::item(this, n).add_write(value).add_flags(insert_tag);
            return n->mutable_value().writeable_value();
        // kvp is already inserted into the tree
        } else {
            // add @value to write set
            // XXX do we need the add_write if we're actually writing the value??
            // insert_tag indicates that we need to remove the insert_bit during install
            Sto::item(this, x).add_write(value).add_flags(insert_tag);
            // we never actually delete nodes -- if deleted nodes are read, they are just updated
            if (is_deleted(x->version())) {
                // previous transaction install marked delete_bit and removed insert_bit
                mark_inserted(&x->version());
                erase_deleted(&x->version());
                if (force) {
                    // XXX do we want to write before commit?
                    x->writeable_value() = value;
                } else {
                   // deleted but we don't want to update with value (T())
                    x->writeable_value() = T();
                }
            }
        }
        unlock(&treelock_);
        return x->mutable_value().writeable_value();
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
    Version treelock_;
    
    friend class RBProxy<K, T>;
};

// STL-ish interface wrapper returned by RBTree::operator[]
template <typename K, typename T>
class RBProxy {
public:
    typedef RBTree<K, T> transtree_t;
    explicit RBProxy(transtree_t& tree, const K& key)
        : tree_(tree), key_(key) {};
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
    auto x = find_or_abort(idx_pair);
    if (x) {
        auto val = x->mutable_value().vervalue();
        auto item = Sto::item(this, val).add_read(val->version());
    }
    return ((x) ? 1 : 0);
}

template <typename K, typename T>
inline RBProxy<K, T> RBTree<K, T>::operator[](const K& key) {
    return RBProxy<K, T>(*this, key);
}

template <typename K, typename T>
inline void RBTree<K, T>::insert(std::pair<K, T>& kvp) {
    rbpair<K, T> rbkvp(kvp);
    lock(&treelock_);
    wrapper_tree_.insert(*new rbwrapper<rbpair<K, T> >(  std::move(rbkvp)  ));
    unlock(&treelock_);
}

template <typename K, typename T>
inline int RBTree<K, T>::erase(K& key) {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    rbpair<K, T>* x = find_or_abort(idx_pair);
    auto val = x->mutable_value().vervalue();
    auto item = Sto::item(this, val).add_read(val->version());
    // when install, set delete bit so another transaction will erase
    item.add_flags(delete_tag);
    return (x) ? 1 : 0;
}

template <typename K, typename T>
inline void RBTree<K, T>::lock(TransItem& item) {
    lock(item.key<versioned_value*>());
}
    
template <typename K, typename T>
inline void RBTree<K, T>::unlock(TransItem& item) {
    unlock(item.key<versioned_value*>());
}
   
template <typename K, typename T>
inline bool RBTree<K, T>::check(const TransItem& item, const Transaction& trans) {
    auto e = item.key<versioned_value*>();
    auto read_version = item.template read_value<Version>();
    // XXX?
    bool same_version = (read_version ^ e->version()) <= TransactionTid::lock_bit;
    bool not_locked = (!is_locked(e->version()) || item.has_lock(trans));
    return same_version && not_locked;
}

// XXX we use versioned value as the "STO key" -- will break if we start deleting nodes because then two
// key-versionedvalue pairs with the same key will have two different items
template <typename K, typename T>
inline void RBTree<K, T>::install(TransItem& item, const Transaction& t) {
    (void) t;
    auto e = item.key<versioned_value*>();
    assert(is_locked(e->version()));
    if (has_delete(item)) {
        mark_deleted(&e->version());
    }
    if (has_insert(item)) {
        erase_inserted(&e->version());
    }
}

template <typename K, typename T>
inline void RBTree<K, T>::cleanup(TransItem& item, bool committed) {
    if (!committed) {
        // if item has been tagged inserted, then we just mark that it should be deleted 
        // (i.e. another item can reuse this node)
        if (has_insert(item)) {
            auto e = item.key<versioned_value*>();
            mark_deleted(&e->version());
            erase_inserted(&e->version());
        }
        // if item has been tagged deleted, then we just ignore it (don't set delete bit)
    }
}
