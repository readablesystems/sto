#pragma once
#include <iostream>

#include "masstree-beta/kvthread.hh"

//versioned_value_struct suitable for logging
template <typename T, typename S, typename=void>
struct versioned_value_struct_logging /*: public threadinfo::rcu_callback*/ {
  typedef T value_type;
  typedef uint64_t version_type;
  typedef S key_type;
  
  versioned_value_struct_logging() : version_(), valueptr_(), keyptr_() {}
  
  static versioned_value_struct_logging* make(const key_type& key, const value_type& val, version_type version) {
    return new versioned_value_struct_logging<T, S>(val, version, key);
  }
  
  bool needsResize(const value_type&) {
    return false;
  }
  
  versioned_value_struct_logging* resizeIfNeeded(const value_type&) {
    return NULL;
  }
  
  inline void set_value(const value_type& v) {
    valueptr_ = new value_type(std::move(v));
  }
  
  inline const value_type& read_value() {
    return *valueptr_;
  }
  
  inline void set_key(const key_type& k) {
    keyptr_ = new key_type(std::move(k));
  }
  
  inline const key_type& read_key() {
    return *keyptr_;
  }
  
  inline version_type& version() {
    return version_;
  }
  
  inline void deallocate_rcu(threadinfo& ti) {
    ti.deallocate_rcu(this, sizeof(versioned_value_struct_logging), memtag_value);
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
  versioned_value_struct_logging(const value_type& val, version_type v, const key_type& k) : version_(v), valueptr_(new value_type(std::move(val))), keyptr_(new key_type(std::move(k))) {}
  
  version_type version_;
  value_type* valueptr_;
  key_type* keyptr_;
};


template <typename T, typename=void>
struct versioned_value_struct /*: public threadinfo::rcu_callback*/ {
  typedef T value_type;
  typedef uint64_t version_type;
  typedef std::string key_type;

  versioned_value_struct() : version_(), value_() {}
  
  static versioned_value_struct* make(const key_type& key, const value_type& val, version_type version) {
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
  
  inline void set_key(const key_type& k) {
    return;
  }
  
  inline const key_type& read_key() {
    return "";
  }

  
  inline value_type& writeable_value() {
    return value_;
  }
  
  inline version_type& version() {
    return version_;
  }

  inline void deallocate_rcu(threadinfo& ti) {
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
  versioned_value_struct(const value_type& val, version_type v, const key_type& k) : version_(v), value_(val) {}
  
  version_type version_;
  value_type value_;
};

// double box for non trivially copyable types!
template<typename T>
struct versioned_value_struct<T, typename std::enable_if<!__has_trivial_copy(T)>::type> {
public:
  typedef T value_type;
  typedef uint64_t version_type;
  typedef std::string key_type;

  static versioned_value_struct* make(const key_type& k, const value_type& val, version_type version) {
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
  
  inline void set_key(const key_type& k) {
    return;
  }
  
  inline const key_type& read_key() {
    return "";
  }
  
  version_type& version() {
    return version_;
  }

  inline void deallocate_rcu(threadinfo& ti) {
    // XXX: really this one needs to be a rcu_callback so we can call destructor
    ti.deallocate_rcu(this, sizeof(versioned_value_struct), memtag_value);
  }
  
private:
  versioned_value_struct(const value_type& val, version_type version, const key_type& k) : version_(version), valueptr_(new value_type(std::move(val))) {}

  version_type version_;
  value_type* valueptr_;
};
