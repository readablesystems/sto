#pragma once
#include <iostream>
#include "Interface.hh"
#include "TWrapped.hh"

// TODO(nate): ugh. really we should have a MassTrans subclass of this with the
// deallocate_rcu functions so we 1) don't have to include Masstree headers in
// nearly everything STO-related and 2) don't accidentally call a Masstree
// function (deallocate_rcu) in some other context.
#include "kvthread.hh"

template <typename T>
struct versioned_value_struct /*: public threadinfo::rcu_callback*/ {
  typedef T value_type;
  typedef TWrapped<T> wv_type;
  typedef typename wv_type::read_type read_type;
  typedef TicTocVersion version_type;

  versioned_value_struct() : version_(), wrapped_value_() {}
  // XXX Yihe: I made it public; is there any reason why it should be private?
  versioned_value_struct(const value_type& val, version_type v) : version_(v), wrapped_value_(val) {}
  
  static versioned_value_struct* make(const value_type& val, version_type version) {
    return new versioned_value_struct<T>(val, version);
  }
  
  bool needsResize(const value_type&) {
    return false;
  }
  
  versioned_value_struct* resizeIfNeeded(const value_type&) {
    return NULL;
  }
  
  void set_value(const value_type& v) {
    wrapped_value_.write(v);
  }
  
  const version_type& version() const {
    return version_;
  }
  version_type& version() {
    return version_;
  }

  read_type atomic_read(TransProxy item) const {
    return wrapped_value_.read(item, version_);
  }

  read_type read_value() const {
    return wrapped_value_.access();
  }

  void deallocate_rcu(threadinfo& ti) {
    ti.deallocate_rcu(this, sizeof(versioned_value_struct), memtag_value);
  }

#if 0
  // rcu_callback method to self-destruct ourself
  void operator()(threadinfo& ti) {
    // this will call value's destructor
    this->versioned_value_struct::~versioned_value_struct();
    // and free our memory too
    ti.deallocate(this, sizeof(versioned_value_struct), memtag_value);
  }
#endif
  
private: 
  version_type version_;
  TWrapped<value_type> wrapped_value_;
};

#if 0
// double box for non trivially copyable types!
template<typename T>
struct versioned_value_struct<T, typename std::enable_if<!__has_trivial_copy(T)>::type> {
public:
  typedef T value_type;
  typedef TicTocVersion version_type;

  static versioned_value_struct* make(const value_type& val, version_type version) {
    return new versioned_value_struct(val, version);
  }

  versioned_value_struct() : version_(), valueptr_() {}

  bool needsResize(const value_type&) {
    return false;
  }
  versioned_value_struct* resizeIfNeeded(const value_type&) {
    return this;
  }

  void set_value(const value_type& v) {
    //auto *old = valueptr_;
    valueptr_ = new value_type(std::move(v));
    // rcu free old (HOW without threadinfo access??)
  }

  const value_type& read_value() const {
    return *valueptr_;
  }

  inline const version_type& version() const {
    return version_;
  }
  version_type& version() {
    return version_;
  }

  inline void deallocate_rcu(threadinfo& ti) {
    // XXX: really this one needs to be a rcu_callback so we can call destructor
    ti.deallocate_rcu(this, sizeof(versioned_value_struct), memtag_value);
  }
  
private:
  versioned_value_struct(const value_type& val, version_type version) : version_(version), valueptr_(new value_type(std::move(val))) {}

  version_type version_;
  value_type* valueptr_;
};
#endif
