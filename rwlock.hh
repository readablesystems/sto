#include "compiler.hh"

class rwlock {
private:
  __attribute__((aligned(64))) uint32_t value;
  static constexpr uint32_t write_bit = 1 << 31;
public:
  rwlock() : value(0) {}
    
  inline void read_lock() {
    uint32_t v = value;
    while ( (v & write_bit) || !bool_cmpxchg(&value, v, v+1)) {
      __asm volatile("pause" : :);
      v = value;
    }
    fence();
  }
  
  inline void read_unlock() {
    fetch_and_add(&value, -1);
    fence();
  }
    
  inline void write_lock() {
    uint32_t v = value;
    while ( v || !bool_cmpxchg(&value, v, write_bit)) {
      __asm volatile("pause" : :);
      v = value;
    }
    fence();
  }
    
  inline void write_unlock() {
    value = 0;
    fence();
  }
};

