#include "Array.hh"
#include "Transaction.hh"

template <typename T, unsigned N>
void Array<T,N>::atomicRead(Transaction& t, unsigned i, Array<T,N>::Version& v, 
                            T& val) {
  Array<T,N>::Version v2;
    // if version stays the same across these reads then .val should match up with .version
    do {
      v2 = data_[i].version;
      if (v2 & Array<T,N>::lock_bit)
        t.abort();
      fence();
      val = data_[i].val;
      fence();
      // make sure version didn't change after we read the value
      v = data_[i].version;
    } while (v != v2);
  }
template <typename T, unsigned N>
T Array<T,N>::transRead_nocheck(Transaction& t, unsigned i) {
    auto item = t.fresh_item(this, i);
    Array<T,N>::Version v;
    T val;
    atomicRead(t, i, v, val);
    item.add_read(v);
    return val;
  }
template <typename T, unsigned N>
void Array<T,N>::transWrite_nocheck(Transaction& t, unsigned i, T val) {
    auto item = t.fresh_item(this, i);
    item.add_write(val);
  }

template <typename T, unsigned N>
T Array<T,N>::transRead(Transaction& t, unsigned i) {
    auto item = t.item(this, i);
    if (item.has_write()) {
      return item.template write_value<T>();
    } else {
      Array<T,N>::Version v;
      T val;
      atomicRead(t, i, v, val);
      item.add_read(v);
      return val;
    }
  }

template <typename T, unsigned N>
void Array<T,N>::transWrite(Transaction& t, unsigned i, T val) {
    auto item = t.item(this, i);
    // can just do this blindly
    item.add_write(val);
  }

// yikes
template class Array<int, 1000000u>;
