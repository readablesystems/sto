
#pragma once 

#include "config.h"
#include "compiler.hh"
#include <iostream>
#include "Transaction.hh"
#include "SingleElem.hh"

template <typename T, unsigned N, typename Elem = SingleElem<T>>
class Array1 : public Shared {
    typedef uint32_t Version;
    typedef VersionFunctions<Version, 0> Versioning;
  public:
    typedef unsigned key_type;
    typedef T value_type;

    T read(key_type i) {
        return data_[i].read();
    }

    void write(key_type i, value_type v) {
        lock(i);
        data_[i].set_value(std::move(v));
        unlock(i);
    }

    value_type transRead(Transaction& t, const key_type& i){
        return data_[i].transRead(t);
    }

    void transWrite(Transaction& t, const key_type& i, value_type v) {
        data_[i].transWrite(t, std::move(v));
    }

    void lock(key_type i) {
        data_[i].lock();
    }

    void unlock(key_type i) {
        data_[i].unlock();
    }

    bool check(TransItem& item, Transaction& trans){
        key_type i = item.key<key_type>();
        return data_[i].check(item, trans);
    }

    void lock(TransItem& item){
        lock(item.key<key_type>());
    }
    void unlock(TransItem& item){
        unlock(item.key<key_type>());
    }

    void install(TransItem& item, uint32_t tid){
        //install value
        data_[item.key<key_type>()].install(item, tid);
    }

  private:
    Elem data_[N];
};
