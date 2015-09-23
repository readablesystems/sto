#pragma once

#include <cassert>
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

template <typename K, typename T>
class RBTree {
    public :
        // lookup
        inline size_t count(const K& key) const;

        // element access
        inline T& operator[](const K& key);
        
        // modifiers
        inline int erase(K& key);
        inline void insert(std::pair<K, T>& n);
    private:
        rbtree<rbwrapper<std::pair<K, T>>> wrapper_tree_;
};

template <typename K, typename T>
inline size_t RBTree<K, T>::count(const K& key) const {
    return wrapper_tree_.count(key);
}

template <typename K, typename T>
inline T& RBTree<K, T>::operator[](const K& key) {
    auto x = wrapper_tree_.find_any(key);
    if (x) 
        return x->value().second;
    // create a new key-value pair with empty value
    // return reference to value
    auto n = *new rbwrapper<std::pair<K, T>>(std::pair<K, T>(key, T()));
    wrapper_tree_.insert(n);
    return n.value().second;
}

template <typename K, typename T>
inline void RBTree<K, T>::insert(std::pair<K, T>& kvp) {
    auto x = *new rbwrapper<std::pair<K, T>>(kvp);
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
