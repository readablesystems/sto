#ifndef sto_new_spinlock_h
#define sto_new_spinlock_h

#include "compiler.hh"


class spinlock {
private:
  volatile uint32_t value;
public:
  spinlock() : value(0) {}
  
  spinlock(const spinlock &) = delete;
  spinlock(spinlock &&) = delete;
  spinlock &operator =(const spinlock &) = delete;
  
  inline void lock() {
    uint32_t v = value;
    while (v || !__sync_bool_compare_and_swap(&value, 0, 1)) {
      __asm volatile("pause" : :);
      v = value;

    }
    fence(); // Do we need a fence here?
  }
  
  inline bool try_lock() {
    return __sync_bool_compare_and_swap(&value, 0, 1);
  }
  
  inline void unlock() {
    assert(value);
    value = 0;
    fence();
  }
  
  inline bool is_locked() const {
    return value;
  }
};


#endif
