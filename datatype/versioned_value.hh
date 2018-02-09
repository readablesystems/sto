#pragma once
#include <iostream>
#include "Interface.hh"
#include "masstree_print.hh"

// TODO(nate): ugh. really we should have a MassTrans subclass of this with the
// deallocate_rcu functions so we 1) don't have to include Masstree headers in
// nearly everything STO-related and 2) don't accidentally call a Masstree
// function (deallocate_rcu) in some other context.
#include "kvthread.hh"

template <typename T, typename=void>
struct versioned_value_struct /*: public threadinfo::rcu_callback*/ {
  typedef T value_type;
  typedef TransactionTid::type version_type;

  versioned_value_struct() : version_(), value_() {}
  // XXX Yihe: I made it public; is there any reason why it should be private?
  versioned_value_struct(const value_type& val, version_type v) : version_(v), value_(val) {}
  
  static versioned_value_struct* make(const value_type& val, version_type version) {
    return new versioned_value_struct<T>(val, version);
  }
  
  bool needsResize(const value_type&) {
    return false;
  }
  
  versioned_value_struct* resizeIfNeeded(const value_type&) {
    return NULL;
  }
  
  inline void set_value(const value_type& v) {
    value_ = v;
  }
  
  inline const value_type& read_value() const {
    return value_;
  }

  inline value_type& writeable_value() {
    return value_;
  }
  
  inline const version_type& version() const {
    return version_;
  }
  inline version_type& version() {
    return version_;
  }

  inline void deallocate_rcu(threadinfo& ti) {
    ti.deallocate_rcu(this, sizeof(versioned_value_struct), memtag_value);
  }

  // Masstree debug printer
  void print(FILE *f, const char *prefix, int indent, lcdf::Str key,
    kvtimestamp_t, char *suffix) {
    std::stringstream vss;
    vss << value_;
    fprintf(f, "%s%*s%.*s = %s%s\n", prefix, indent, "",
        key.len, key.s, vss.str().c_str(), suffix);
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
  value_type value_;
};

// double box for non trivially copyable types!
template<typename T>
struct versioned_value_struct<T, typename std::enable_if<!__has_trivial_copy(T)>::type> {
public:
  typedef T value_type;
  typedef TransactionTid::type version_type;

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
  
  // Masstree debug printer
  void print(FILE *f, const char *prefix, int indent, lcdf::Str key,
    kvtimestamp_t, char *suffix) {
    std::stringstream vss;
    vss << *valueptr_;
    fprintf(f, "%s%*s%.*s = %s%s\n", prefix, indent, "",
        key.len, key.s, vss.str().c_str(), suffix);
  }

private:
  versioned_value_struct(const value_type& val, version_type version) : version_(version), valueptr_(new value_type(std::move(val))) {}

  version_type version_;
  value_type* valueptr_;
};
