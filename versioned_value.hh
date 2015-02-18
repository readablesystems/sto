#pragma once
#include <iostream>


template <typename T, typename=void>
struct versioned_value_struct /*: public threadinfo::rcu_callback*/ {
  typedef T value_type;
  typedef uint32_t version_type;

  versioned_value_struct() : version_(), value_() {}
  
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
  
  inline const value_type& read_value() {
    return value_;
  }
  
  inline version_type& version() {
    return version_;
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
  versioned_value_struct(const value_type& val, version_type v) : version_(v), value_(val) {}
  
  version_type version_;
  value_type value_;
};

// double box for non trivially copyable types!
template<typename T>
struct versioned_value_struct<T, typename std::enable_if<!__has_trivial_copy(T)>::type> {
public:
  typedef T value_type;
  typedef uint32_t version_type;

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

  const value_type& read_value() {
    return *valueptr_;
  }

  version_type& version() {
    return version_;
  }
  
private:
  versioned_value_struct(const value_type& val, version_type version) : version_(version), valueptr_(new value_type(std::move(val))) {}

  version_type version_;
  value_type* valueptr_;
};
