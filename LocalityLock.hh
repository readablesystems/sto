#pragma once
#include "config.h"
#include "compiler.hh"

class LocalityLock {
public:
  typedef unsigned word_type;

  static constexpr word_type in_use = word_type(1) << 15;//(sizeof(word_type)*8 - 1);
  static constexpr word_type increment_value = word_type(1) << 16;
  static constexpr word_type thread_mask = increment_value-1;

  LocalityLock() : owner_(), mode_() {}
  LocalityLock(int initialowner) : owner_(initialowner), mode_() {}

  word_type owner() {
    return (owner_ & thread_mask) & ~in_use;
  }

  word_type version() {
    return owner_ & ~thread_mask;
  }
  word_type inc_version() {
    owner_ = owner_ + increment_value;
  }

  inline __attribute__((always_inline)) void acquire(unsigned threadid) {
    threadid = threadid+1;
    fence();
    if ((owner_ & thread_mask) != threadid) {
      while (1) {
        auto cur = mode_;
        fence();
        if (cur == 0) {
          // register our intent
          if (bool_cmpxchg(&mode_, cur, threadid)) {
            fence();
            // we have dibs, now wait for current owner
            while (1) {
              auto curowner = owner_;
              if (!(curowner & in_use)) break;
              relax_fence();
            }
            break;
          }
        }
        relax_fence();
      }
    }
    fence();
    owner_ = (owner_ & ~thread_mask) | threadid | in_use;
    fence();
    if (mode_ == threadid) {
      mode_ = 0;
    } else
    // someone else got dibs
    if (mode_ != 0) {
      // release and retry
      owner_ = owner_ & ~thread_mask;
      acquire(threadid);
    }
  }
  inline __attribute__((always_inline)) void release(unsigned threadid) {
    threadid = threadid+1;
    fence();
    owner_ = owner_ & ~in_use;
  }
  word_type owner_;
  word_type mode_;
};
