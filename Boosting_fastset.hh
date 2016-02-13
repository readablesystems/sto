#pragma once

#include "local_vector.hh"

// should be a multiple of 64
#define BOOSTING_BLOOMFILTER_SIZE 0

// size 0 means no hashtable
#define BOOSTING_HASHTABLE_SIZE 0
#define BOOSTING_HASHTABLE_THRESHOLD 8

template <typename T, unsigned Size = 512>
class FastSet {
public:
  FastSet() : lockVec_()
#if BOOSTING_HASHTABLE_SIZE
            , hashtable_(), nhashed_()
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
    if (lockVec_.size() > BOOSTING_HASHTABLE_THRESHOLD) {
      if (nhashed_ < lockVec_.size()) {
        update_hash();
      }
      uint64_t idx = hashtable_[hash(obj)];
      if (!idx) {
        return false;
      } else if (lockVec_[idx-1] == obj) {
        return true;
      }
    }
#endif
    return find(obj) != lockVec_.end();
  }
  void clear() {
    lockVec_.clear();
#if BOOSTING_BLOOMFILTER_SIZE
    memset(bloom_, 0, sizeof(bloom_));
#endif
#if BOOSTING_HASHTABLE_SIZE
    nhashed_ = 0;
#endif
  }
  void unsafe_clear() {
    lockVec_.unsafe_clear();
#if BOOSTING_BLOOMFILTER_SIZE
    memset(bloom_, 0, sizeof(bloom_));
#endif
#if BOOSTING_HASHTABLE_SIZE
    nhashed_ = 0;
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
  uint64_t hashtable_[BOOSTING_HASHTABLE_SIZE];
  int nhashed_;
  static inline int hash(T& obj) {
    uintptr_t n = (uintptr_t)obj;
    return ((n >> 4) ^ (n >> 20)) % BOOSTING_HASHTABLE_SIZE;
  }
  inline void update_hash() {
    if (nhashed_ == 0)
      memset(hashtable_, 0, sizeof(hashtable_));
    for (auto it = lockVec_.begin() + nhashed_; it != lockVec_.end(); ++it, ++nhashed_) {
      int h = hash(*it);
      hashtable_[h] = nhashed_ + 1;
    }
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
