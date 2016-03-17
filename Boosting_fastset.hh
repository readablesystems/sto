#pragma once

#include "local_vector.hh"
#include <string.h>

// should be a multiple of 64
#define BOOSTING_BLOOMFILTER_SIZE 0

// size 0 means no hashtable
#define BOOSTING_HASHTABLE_SIZE 1024

template <typename T, unsigned Size = 512>
class FastSet {
public:
#if BOOSTING_HASHTABLE_SIZE
  static constexpr uint16_t max_base = 1<<15;
#endif
  FastSet() : lockVec_()
#if BOOSTING_HASHTABLE_SIZE
            , hash_base_(0), hashtable_()
#endif
#if BOOSTING_BLOOMFILTER_SIZE
            , bloom_()
#endif
  {}
  void push(T& obj) {
    lockVec_.push_back(obj);
#if BOOSTING_BLOOMFILTER_SIZE
    uint64_t hash = bloom_hash(obj);
    uint64_t idx = hash / 64;
    hash = hash % 64;
    bloom_[idx] |= 1 << hash;
#endif
#if BOOSTING_HASHTABLE_SIZE
    auto& h = hashtable_[hash(obj)];
    if (h <= hash_base_)
      h = hash_base_ + lockVec_.size();
#endif
  }
  typename local_vector<T, Size>::iterator find (T& obj) {
    auto it = lockVec_.begin();
    for (auto end = lockVec_.end(); it != end; ++it) {
      if (*it == obj) {
        return it;
      }
    }
    return it;
  }
  typename local_vector<T, Size>::iterator begin () {
    return lockVec_.begin();
  }
  typename local_vector<T, Size>::iterator end () {
    return lockVec_.end();
  }
  bool exists(T& obj) {
#if BOOSTING_BLOOMFILTER_SIZE
    if (definitely_absent(obj))
      return false;
#endif
#if BOOSTING_HASHTABLE_SIZE
    auto idx = hashtable_[hash(obj)];
    if (idx <= hash_base_)
      return false;
    else if (lockVec_[idx - hash_base_ - 1] == obj)
        return true;
#endif
    return find(obj) != lockVec_.end();
  }
  void clear() {
#if BOOSTING_HASHTABLE_SIZE
    hash_base_ += lockVec_.size();
    if (hash_base_ >= max_base) {
      memset(hashtable_, 0, sizeof(hashtable_));
      hash_base_ = 0;
    }
#endif
    lockVec_.clear();
#if BOOSTING_BLOOMFILTER_SIZE
    memset(bloom_, 0, sizeof(bloom_));
#endif
  }
  void unsafe_clear() {
#if BOOSTING_HASHTABLE_SIZE
    hash_base_ += lockVec_.size();
    if (hash_base_ >= max_base) {
      memset(hashtable_, 0, sizeof(hashtable_));
      hash_base_ = 0;
    }
#endif
    lockVec_.unsafe_clear();
#if BOOSTING_BLOOMFILTER_SIZE
    memset(bloom_, 0, sizeof(bloom_));
#endif
  }
  bool insert(T& obj) {
    if (exists(obj))
      return false;
    lockVec_.push_back(obj);
    return true;
  }
  bool erase(T& obj) {
    auto it = find(obj);
    if (it != lockVec_.end()) {
      lockVec_.erase(it);
      return true;
    }
    return false;
  }
private:
  local_vector<T, Size> lockVec_;
#if BOOSTING_HASHTABLE_SIZE
  uint16_t hash_base_;
  uint16_t hashtable_[BOOSTING_HASHTABLE_SIZE];
  static inline int hash(T& obj) {
    uintptr_t n = (uintptr_t)obj;
    //    return (n >> 4) % BOOSTING_HASHTABLE_SIZE;
    return (n + (n>>16)*9) % BOOSTING_HASHTABLE_SIZE;
    return ((n >> 4) ^ (n >> 20)) % BOOSTING_HASHTABLE_SIZE;
  }
#endif
#if BOOSTING_BLOOMFILTER_SIZE
  uint64_t bloom_[BOOSTING_BLOOMFILTER_SIZE / 64];
  static inline uint64_t bloom_hash(T& obj) {
    // assuming T is a pointer
    return (((intptr_t)obj >> 2) ^ ((intptr_t)obj >> 5)) % BOOSTING_BLOOMFILTER_SIZE;
  }
public:
  inline bool definitely_absent(T& obj) {
    uint64_t hash = bloom_hash(obj);
    int idx = hash / 64;
    hash = hash % 64;
    return !((1<<hash) & bloom_[idx]);
  }
#endif
};
