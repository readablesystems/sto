#pragma once

#include <assert.h>
#include <vector>
#include <sys/time.h>
#include <atomic>

#define CACHELINE_SIZE 64

// some helpers for cacheline alignment
#define CACHE_ALIGNED __attribute__((aligned(CACHELINE_SIZE)))

#define __XCONCAT2(a, b) a ## b
#define __XCONCAT(a, b) __XCONCAT2(a, b)
#define CACHE_PADOUT  \
char __XCONCAT(__padout, __COUNTER__)[0] __attribute__((aligned(CACHELINE_SIZE)))

#define PACKED __attribute__((packed))

#define nop_pause __asm volatile("pause" : :)



template <typename T>
static std::vector<T> MakeRange(T start, T end) {
  std::vector<T> ret;
  for (T i = start; i < end; i++) {
    ret.push_back(i);
  }
  return ret;
}

template <typename T>
static inline uint8_t * write(uint8_t* buf, const T &obj) {
  T *p = (T *) buf;
  *p = obj;
  return (uint8_t *)(p + 1);
}

static inline uint64_t _char_to_uint64(char *str)
{
  uint64_t ret = 0;
  char *ptr = str;
  while (*ptr) {
    ret *= 10;
    ret += (uint64_t) (*ptr) - 48;
    ptr++;
  }
  
  return ret;
}

namespace util {
  
  //Padded aligned primitives
  template <typename T, bool Pedantic = true>
  class aligned_padded_elem {
  public:
    
    template <class... Args>
    aligned_padded_elem(Args &&... args) : elem(std::forward<Args>(args)...) {
      if (Pedantic) {
        assert(((uintptr_t)this % CACHELINE_SIZE) == 0);
      }
    }
    
    T elem;
    CACHE_PADOUT;
    
    inline T & operator*() {return elem;}
    inline const T & operator*() const {return elem; }
    inline T * operator->() {return &elem; }
    inline const T * operator->() const { return &elem;}
    
  } CACHE_ALIGNED;

  template <typename T>
  static inline T
  non_atomic_fetch_add_(std::atomic<T> &data, T arg)
  {
    const T ret = data.load(std::memory_order_acquire);
    data.store(ret + arg, std::memory_order_release);
    return ret;
  }

  static inline uint64_t
  cur_time_us()
  {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((uint64_t)tv.tv_sec) * 1000000 + tv.tv_usec;
  }
}


