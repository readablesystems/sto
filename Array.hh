#include <stdint.h>
#include <mutex>

#include "TransState.hh"
#include "Interface.hh"

#pragma once

template <typename T, unsigned N>
class Array {
public:
  typedef uint32_t Version;
  typedef unsigned Key;
  typedef T Value;

  Array() {
    memset(data_, 0, N*sizeof(internal_elem));
  }

  T transRead(TransState& t, Key i) {
    // assumes atomic read (probably not actually true)
    internal_elem cur = data_[i];
    t.read(ArrayRead(this, i, cur.version));
    return cur.val;
  }

  void transWrite(TransState& t, Key i, Value v) {
    t.write(ArrayWrite(this, i, v));
  }

  class ArrayRead : Reader {
  public:
    bool check() {
      return a_->elem(i_).version == version_;
    }

  private:
    Array *a_;
    Key i_;
    Version version_;

    ArrayRead(Array *a, Key i, Version v) : a_(a), i_(i), version_(v) {}
  };

  class ArrayWrite : Writer {
  public:

    void lock() {
      // TODO: double writes to same place will double (dead) lock
      a_->elem(i_).mutex.lock();
    }
    void unlock() {
      a_->elem(i_).mutex.unlock();
    }

    uint64_t UID() {
      return i_;
    }

    void install() {
      a_->elem(i_).val = val_;
      a_->elem(i_).version++;
      // unlock here or leave it to transaction mechanism?
    }


  private:
    Array *a_;
    Key i_;
    Value val_;

    ArrayWrite(Array *a, Key i, Value v) : a_(a), i_(i), val_(v) {}
  };

private:

  struct internal_elem {
    std::mutex mutex;
    Version version;
    T val;
  };

  internal_elem data_[N];

  internal_elem& elem(Key i) {
    return data_[i];
  }

};
