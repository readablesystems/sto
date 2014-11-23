#pragma once

#ifndef sto_new_util_h
#define sto_new_util_h

#include "compiler.hh"
#include <iostream>
#define THREAD_BITS 9
#define MAX_THREADS_ (1 << THREAD_BITS)

#define THREAD_MASK ((MAX_THREADS_) - 1)

#define NUMID_BITS 24

#define NUMID_SHIFT THREAD_BITS
#define NUMID_MASK (((((uint64_t) 1) << NUMID_BITS) - 1) << (NUMID_SHIFT))

#define EPOCH_SHIFT (THREAD_BITS + NUMID_BITS)
#define EPOCH_MASK (((uint64_t)-1) << (EPOCH_SHIFT))

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
  std::cout <<"Thread id " << threadId<<std::endl;
  std::cout <<" Num id "<<numId<< std::endl;
  std::cout <<"Epoch id "<<epochId<<std::endl;
  std::cout<<"tid " <<((threadId) | (numId << NUMID_SHIFT) | (epochId << EPOCH_SHIFT)) << std::endl;
  return (threadId) | (numId << NUMID_SHIFT) | (epochId << EPOCH_SHIFT);

}


#endif
