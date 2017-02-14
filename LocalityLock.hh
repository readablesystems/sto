#pragma once
#include "config.h"
#include "compiler.hh"

class LocalityLock {
public:
  typedef unsigned word_type;

  static constexpr word_type in_use = word_type(1) << 7;//(sizeof(word_type)*8 - 1);
  static constexpr word_type increment_value = word_type(1) << 8;
  static constexpr word_type thread_mask = increment_value-1;

  LocalityLock() : owner_(), mode_() {}
  LocalityLock(int initialowner) : owner_(initialowner), mode_() {}

  word_type owner() const {
    return (owner_ & thread_mask) & ~in_use;
  }

  bool is_locked() const {
    return owner_ & in_use;
  }

  bool i_own(unsigned threadid) {
    return owner() == threadid+1;
  }

  word_type version() const {
    return owner_ & ~thread_mask;
  }
  void inc_version() {
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
    auto to_set = (owner_ & ~thread_mask) | threadid | in_use;
    fence();
    owner_ = to_set;
    fence();
    if (mode_ == threadid) {
    } else
    // someone else got dibs
    if (mode_ != 0) {
      assert(mode_ != threadid);
      // the other thread thinks they have the lock, but the lock owner
      // could be marked as owned by either of us (we might've done our
      // set after theirs). 
      // If the other thread hasn't released the lock yet when that happens,
      // their release will make things right.
      // But, it's also possible that we're racing with their release()
      // right now (e.g., they just set the lock unowned but then we set it
      // back to owned by us).
      while (owner_ & in_use) {
        if (mode_ == 0 && (owner_ & in_use))
          return;
        //printf("reached %d\n", threadid);
        relax_fence();
      }
      acquire(threadid);
    }

    fence();
  }
  inline __attribute__((always_inline)) void release(int threadid) {
    fence();

    assert(owner_ & in_use);
    threadid=threadid+1;
    fence();
    owner_ = threadid;//owner_ & ~in_use;
    fence();
    // need to make sure other threads can grab the lock
    if (mode_ == threadid)
      mode_ = 0;
    fence();
  }
  word_type owner_;
  word_type mode_;
};
