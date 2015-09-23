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

    explicit rbpair(K &key, T &value) {
        versioned_value *val = versioned_value::make(value, TransactionTid::increment_value + insert_bit);
        versioned_pair_.first = key;
        versioned_pair_.second = val;
    };
    explicit rbpair(std::pair<K, T> &kvp) {
        rbpair(kvp.first, kvp.second);
    }
    inline const K& key() const {
        return versioned_pair_.first;
    }
    inline versioned_value *vervalue() {
        return versioned_pair_.second;
    }
    inline T& writable_value() {
        return versioned_pair_.second->writable_value();
    }

    inline bool operator<(const rbpair& rhs) const {
        return (key() < rhs.key());
    }
    inline bool operator>(const rbpair& rhs) const {
        return (key() > rhs.key());
    }
private:
    std::pair<K, versioned_value*> versioned_pair_;
};

template <typename K, typename T>
class RBTree {
    public:
        // lookup
        inline size_t count(const K& key) const;

        // element access
        inline T& operator[](const K& key);
        
        // modifiers
        inline int erase(K& key);
        inline void insert(std::pair<K, T>& n);
    private:
        rbtree<rbwrapper<rbpair<K, T>>> wrapper_tree_;
};

template <typename K, typename T>
inline size_t RBTree<K, T>::count(const K& key) const {
    return wrapper_tree_.count(key);
}

template <typename K, typename T>
inline T& RBTree<K, T>::operator[](const K& key) {
    auto x = wrapper_tree_.find_any(key);
    if (x) 
        return x->value().writable_value();
    // create a new key-value pair with empty value
    // return reference to value
    auto n = *new rbwrapper<rbpair<K, T>>(rbpair<K, T>(key, T()));
    wrapper_tree_.insert(n);
    return n.value().writable_value();
}

template <typename K, typename T>
inline void RBTree<K, T>::insert(std::pair<K, T>& kvp) {
    auto x = *new rbwrapper<rbpair<K, T>>(kvp);
    wrapper_tree_.insert(x);
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
