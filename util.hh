#pragma once

#ifndef sto_new_util_h
#define sto_new_util_h

#include "compiler.hh"
#include <iostream>
#include <assert.h>
#include <vector>
#include <sys/time.h>
#include "macros.hh"

/// Some macros related to epochs
#define THREAD_BITS 9
#define MAX_THREADS_ (1 << THREAD_BITS)

#define THREAD_MASK ((MAX_THREADS_) - 1)

#define NUMID_BITS 24

#define NUMID_SHIFT THREAD_BITS
#define NUMID_MASK (((((uint64_t) 1) << NUMID_BITS) - 1) << (NUMID_SHIFT))

#define EPOCH_SHIFT (THREAD_BITS + NUMID_BITS)
#define EPOCH_MASK (((uint64_t)-1) << (EPOCH_SHIFT))

/*
static inline uint64_t epochId(uint64_t tid) {
  return (tid & EPOCH_MASK) >> EPOCH_SHIFT;
}

static inline uint64_t numId(uint64_t tid) {
  return (tid & NUMID_MASK) >> NUMID_SHIFT;
}

static inline uint64_t threadId(uint64_t tid) {
  return tid & THREAD_MASK;
}

static inline uint64_t makeTID(uint64_t threadId, uint64_t numId, uint64_t epochId) {
  static_assert((THREAD_MASK | NUMID_MASK | EPOCH_MASK) == ((uint64_t)-1), "dsew");
  static_assert((THREAD_MASK & NUMID_MASK) == 0, "weq");
  static_assert((NUMID_MASK & EPOCH_MASK) == 0, "xx");
  //std::cout <<"Thread id " << threadId<<std::endl;
  //std::cout <<" Num id "<<numId<< std::endl;
  //std::cout <<"Epoch id "<<epochId<<std::endl;
  //std::cout<<"tid " <<((threadId) | (numId << NUMID_SHIFT) | (epochId << EPOCH_SHIFT)) << std::endl;
  
  return (threadId) | (numId << NUMID_SHIFT) | (epochId << EPOCH_SHIFT);
}*/

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
}


#endif
