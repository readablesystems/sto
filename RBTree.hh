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

template <typename T>
class RBTree {
    public :
        inline RBTree();
        ~RBTree();

/*
        inline T *root();

        // lookup
        inline bool empty() const;
        inline size_t size() const;
        template <typename K>
        inline size_t count(const K& key) const;

        // element access
        template <typename K>
        inline T& operator[](const K& key);
        
        // modifiers
        // XXX should we also dispose (i.e. delete/free) the node?
        inline void erase(T& x);
*/
        inline void insert(T& n);
    private:
        rbtree<rbwrapper<T>> wrapper_tree_;
};

template <typename T>
inline void RBTree<T>::insert(T& n) {
    auto x = *new rbwrapper<T>(n);
    wrapper_tree_.insert(x);
}
