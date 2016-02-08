#pragma once

#include "config.h"
#include "compiler.hh"
#include <iostream>
#include "Transaction.hh"
#include "SingleElem.hh"
#include "VersionFunctions.hh"

template<typename T, unsigned N, typename Elem = SingleElem<T>> class Array1Iter;

template <typename T, unsigned N, typename Elem = SingleElem<T>>
class Array1 : public Shared {
    friend class Array1Iter<T, N, Elem>;
    typedef Array1Iter<T, N, Elem> iterator;
    
    typedef uint32_t Version;
    typedef VersionFunctions<Version> Versioning;
  public:
    typedef unsigned key_type;
    typedef T value_type;

    T read(key_type i) {
        return data_[i].read();
    }

    void write(key_type i, value_type v) {
        data_[i].write(std::move(v));
    }

    value_type transRead(const key_type& i){
        return data_[i].transRead();
    }

    void transWrite(const key_type& i, value_type v) {
        data_[i].transWrite(std::move(v));
    }

    void lock(key_type i) {
        data_[i].lock();
    }

    void unlock(key_type i) {
        data_[i].unlock();
    }

    bool check(const TransItem& item, const Transaction& trans){
        key_type i = item.key<key_type>();
        return data_[i].check(item, trans);
    }

    void lock(TransItem& item){
        lock(item.key<key_type>());
    }
    void install(TransItem& item, const Transaction& t) {
        //install value
        data_[item.key<key_type>()].install(item, t);
    }
    void cleanup(TransItem& item, bool) {
        if (item.needs_unlock())
            unlock(item.key<key_type>());
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

