#pragma once
#include "config.h"
#include "compiler.hh"

#include <assert.h>

class Mode {
public:
  typedef uint64_t word_type;

  static constexpr word_type in_use = word_type(1) << (sizeof(word_type)*8 - 1);
  static constexpr word_type coarse_grained = 1;
  static constexpr word_type lock_bit = word_type(1) << (sizeof(word_type)*8 - 1);

  Mode() : owner_(), mode_() {}
  Mode(int initialowner) : owner_(initialowner), mode_() {}

  inline __attribute__((always_inline)) void acquire(int threadid) volatile {
    if (owner_ != threadid) {
      while (1) {
        auto cur = mode_;
        if (cur == coarse_grained && !(cur & lock_bit)) {
          if (bool_cmpxchg(&mode_, cur, cur|lock_bit)) {
            return;
          }
        }
        if (cur == 0) {
          // try to switch to coarse-grained mode
          if (bool_cmpxchg(&mode_, cur, coarse_grained|lock_bit)) {
            // we have lock, now have to wait for a potential current writer
            while (1) {
              auto curowner = owner_;
              if (!(curowner & in_use)) return;
            }
          }
        }
      }
    }
    owner_ = threadid | in_use;
    // we've switched modes
    if (mode_ != 0) {
      owner_ = 0;
      acquire(threadid);
    }
  }
  inline __attribute__((always_inline)) void release(int threadid) volatile {
    //assert(owner_ == (threadid|in_use));
    if (owner_ == (threadid|in_use))
      owner_ = threadid;
    else {
      if (mode_ & coarse_grained) {
        // TODO: probably not safe once more than one mode exists
        mode_ = coarse_grained;
      }
    }
  }
  word_type owner_;
  word_type mode_;
};
