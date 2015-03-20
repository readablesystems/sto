#pragma once

#include "Transaction.hh"
#include "Interface.hh"
#include "VersionFunctions.hh"
#include "versioned_value.hh"

template <typename T, int Default_size = 100,
          typename Structure = versioned_value_struct<T>>
class Array_Sized : public Shared {
public:
  typedef uint32_t Version;
  typedef VersionFunctions<Version> Versioning;

  Array_Sized(int size=Default_size) : arr_(new Structure[size]), capacity_(size) {}

  inline void atomicRead(int i, Version& v, T& val) {
    v = arr_[i].version();
    fence();
    val = arr_[i].read_value();
  }

  const T& transRead(Transaction& t, int i) {
    auto& item = t_item(t, i);
    if (item.has_write())
      return item.template write_value<T>();
    else {
      //      Version v;
      //T val;
      //atomicRead(i, v, val);
      Version v = arr_[i].version();
      fence();
      if (!item.has_read())
        t.add_read(item, v);
      // TODO: nothing close to opacity here, but probably faster
      return arr_[i].read_value();
    }
  }

  T transRead_safe(Transaction& t, int i) {
    // TODO: this still isn't really that safe, 
    // since our read of the value could be non-atomic
    return transRead(t, i);
  }

  void transWrite(Transaction& t, int i, const T& val) {
    auto& item = t_item(t, i);
    t.add_write(item, val);
  }

  void lock(TransItem& item) {
    int i = item.template key<int>();
    Versioning::lock(arr_[i].version());
  }

  void unlock(TransItem& item) {
    int i = item.template key<int>();
    Versioning::unlock(arr_[i].version());
  }

  bool check(const TransItem& item, const Transaction& t) {
    int i = item.template key<int>();
    auto& elem = arr_[i];
    return Versioning::versionCheck(elem.version(), item.template read_value<Version>())
        && (!Versioning::is_locked(elem.version()) || item.has_lock(t));
  }

  void install(TransItem& item, Transaction::tid_type) {
    int i = item.template key<int>();
    arr_[i].set_value(item.template write_value<T>());
    Versioning::inc_version(arr_[i].version());
  }

  int capacity() {
    return capacity_;
  }

  // non-transactional
  T read(int i) {
    return arr_[i].read_value();
  }

  void write(int i, const T& val) {
    arr_[i].set_value(val);
  }

  T& writeable(int i) {
    return arr_[i].writeable_value();
  }

  T& operator[](int i) {
    return writeable(i);
  }
  
private:
  TransProxy t_item(Transaction& t, int key) {
    // to disable read-my-writes: return t.fresh_item(this, key);
    return t.item(this, key);
  }

  Structure *arr_;
  int capacity_;
};
