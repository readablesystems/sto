#include <stdint.h>
#include <mutex>
#include <iostream>

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

  T read(Key i) {
    return data_[i].val;
  }

  void write(Key i, Value v) {
    mutex(i).lock();
    data_[i].val = v;
    mutex(i).unlock();
  }

  T transRead(TransState& t, Key i) {
    // assumes atomic read (probably not actually true)
    internal_elem cur = data_[i];
    // TODO: memory leaks
    t.read(new ArrayRead(this, i, cur.version));
    return cur.val;
  }

  void transWrite(TransState& t, Key i, Value v) {
    t.write(new ArrayWrite(this, i, v));
  }

  class ArrayRead : public Reader {
  public:
    virtual bool check() {
      return a_->elem(i_).version == version_;
    }

  private:
    Array *a_;
    Key i_;
    Version version_;

    friend class Array;

    ArrayRead(Array *a, Key i, Version v) : a_(a), i_(i), version_(v) {}
  };

  class ArrayWrite : public Writer {
  public:

    virtual void lock() {
      // TODO: double writes to same place will double (dead) lock
      a_->mutex(i_).lock();
    }
    virtual void unlock() {
      a_->mutex(i_).unlock();
    }

    virtual uint64_t UID() {
      return i_;
    }

    virtual void install() {
      a_->elem(i_).val = val_;
      a_->elem(i_).version++;
      // unlock here or leave it to transaction mechanism?
    }


  private:
    Array *a_;
    Key i_;
    Value val_;

    friend class Array;

    ArrayWrite(Array *a, Key i, Value v) : a_(a), i_(i), val_(v) {}
  };

private:

  struct internal_elem {
    Version version;
    T val;
  };

  std::mutex locks_[N];
  internal_elem data_[N];

  internal_elem& elem(Key i) {
    return data_[i];
  }

 std::mutex& mutex(Key i) {
    return locks_[i];
  }

};
