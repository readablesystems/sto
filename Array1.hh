#pragma once

#include "config.h"
#include "compiler.hh"
#include <iostream>
#include "Transaction.hh"
#include "SingleElem.hh"
#include "VersionFunctions.hh"

template<typename T, unsigned N, typename Elem = SingleElem<T>> class Array1Iter;

template <typename T, unsigned N, typename Elem = SingleElem<T>>
class Array1 {
    friend class Array1Iter<T, N, Elem>;
    typedef Array1Iter<T, N, Elem> iterator;
    
    typedef uint32_t Version;
    typedef VersionFunctions<Version> Versioning;
  public:
    typedef unsigned key_type;
    typedef unsigned size_type;
    typedef T value_type;

    T unsafe_get(size_type i) const {
        return data_[i].unsafe_read();
    }
    void unsafe_put(size_type i, value_type v) {
        data_[i].unsafe_write(v);
    }

    value_type transGet(size_type idx) const {
        return data_[idx].transRead();
    }
    void transPut(size_type idx, value_type v) {
        data_[idx].transWrite(std::move(v));
    }

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, N); }
    
  private:
    Elem data_[N];
};

template<typename T, unsigned N, typename Elem>
class Array1Iter : public std::iterator<std::forward_iterator_tag, T> {
public:
    typedef T value_type;
    Array1Iter(Array1<T, N, Elem> * arr, int ptr) : myArr(arr), myPtr(ptr) { }
    Array1Iter(const Array1Iter& itr) : myArr(itr.myArr), myPtr(itr.myPtr) {}
    
    Array1Iter& operator= (const Array1Iter& v) {
        myArr = v.myArr;
        myPtr = v.myPtr;
        return *this;
    }
    
    bool operator==(Array1Iter<T, N, Elem> other) const {
        return (myArr == other.myArr) && (myPtr == other.myPtr);
    }
    
    bool operator!=(Array1Iter<T, N, Elem> other) const {
        return !(operator==(other));
    }
    
    Elem& operator*() {
        return myArr->data_[myPtr];
    }
    
    /* This is the prefix case */
    Array1Iter<T, N, Elem>& operator++() { ++myPtr; return *this; }
    
    /* This is the postfix case */
    Array1Iter<T, N, Elem> operator++(int) {
        Array1Iter<T, N, Elem> clone(*this);
        ++myPtr;
        return clone;
    }
    
private:
    Array1<T, N, Elem> * myArr;
    int myPtr;
};
