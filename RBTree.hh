#pragma once

#include <cassert>
#include <utility>
#include "TaggedLow.hh"
#include "Transaction.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"
#include "RBTreeInternal.hh"

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
    // Copied over from priority queue; may need to change it later
    typedef versioned_value_struct<T> versioned_value;
    typedef TransactionTid::type Version;

    static constexpr TransItem::flags_type insert_tag = TransItem::user0_bit;
    static constexpr TransItem::flags_type delete_tag = TransItem::user0_bit<<1;
    static constexpr Version insert_bit = TransactionTid::user_bit1;
    static constexpr Version delete_bit = TransactionTid::user_bit1<<1;
    static constexpr Version dirty_bit = TransactionTid::user_bit1<<2;

    explicit rbpair(const K& key, const T& value) {
        init(key, value);
    }
    explicit rbpair(std::pair<K, T>& kvp) {
        init(kvp.first, kvp.second);
    }

    // move ctor
    inline rbpair(rbpair&& cp) noexcept {
        versioned_pair_.first = cp.key();
        versioned_pair_.second = cp.vervalue();
        cp.versioned_pair_.first = K();
        cp.versioned_pair_.second = nullptr;
    }

    ~rbpair(){delete versioned_pair_.second;}

    inline const K& key() const {
        return versioned_pair_.first;
    }
    inline versioned_value *vervalue() {
        return versioned_pair_.second;
    }
    inline T& writable_value() {
        //XXX typo in versioned_value.hh...
        return versioned_pair_.second->writeable_value();
    }

    inline bool operator<(const rbpair& rhs) const {
        return (key() < rhs.key());
    }
    inline bool operator>(const rbpair& rhs) const {
        return (key() > rhs.key());
    }
private:
    void init(const K& key, const T& value) {
        versioned_value *val = versioned_value::make(value,
            TransactionTid::increment_value + insert_bit);
        versioned_pair_.first = key;
        versioned_pair_.second = val;
    }
    std::pair<K, versioned_value*> versioned_pair_;
};

template <typename K, typename T>
class RBTree {
    public:
        // lookup
        typedef rbwrapper<rbpair<K, T>> wrapper_type;
        typedef rbtree<wrapper_type> internal_tree_type;
        inline size_t count(const K& key) const;

        // element access
        inline T& operator[](const K& key);
        
        // modifiers
        inline int erase(K& key);
        inline void insert(std::pair<K, T>& n);
    private:
        internal_tree_type wrapper_tree_;
};

template <typename K, typename T>
inline size_t RBTree<K, T>::count(const K& key) const {
    return wrapper_tree_.count(key);
}

template <typename K, typename T>
inline T& RBTree<K, T>::operator[](const K& key) {
    rbwrapper<rbpair<K, T>> idx_pair(rbpair<K, T>(key, T()));
    auto x = wrapper_tree_.find_any(idx_pair,
        rbpriv::make_compare<wrapper_type, wrapper_type>(wrapper_tree_.r_.get_compare()));
    if (x) 
        return x->mutable_value().writable_value();
    // create a new key-value pair with empty value
    // return reference to value
    auto n = new rbwrapper<rbpair<K, T> >(  rbpair<K, T>(key, T())  );
    wrapper_tree_.insert(*n);
    return n->mutable_value().writable_value();
}

template <typename K, typename T>
inline void RBTree<K, T>::insert(std::pair<K, T>& kvp) {
    rbpair<K, T> rbkvp(kvp);
    wrapper_tree_.insert(*new rbwrapper<rbpair<K, T> >(  std::move(rbkvp)  ));
}

template <typename K, typename T>
inline int RBTree<K, T>::erase(K& key) {
    auto x = wrapper_tree_.find_any(key);
    if (x) {
        wrapper_tree_.erase(x);
        return 1;
    }
    else return 0;
}
