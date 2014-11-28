#pragma once

#include <cstring>
#include <atomic>
#include <vector>
#include <limits>


// Thread safety is ensured for many concurrent enqueuers but only one
// concurrent dequeuer. That is, the head end is thread safe, but the tail end
// can only be manipulated by a single thread.
template <typename Tp, unsigned int Capacity>
class circbuf {
public:
  circbuf() : head_(0), tail_(0) {
    memset(&buf_[0], 0, Capacity * sizeof(buf_[0]));
  }
  
  inline bool empty() const {
    return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire) && !buf_[head_.load(std::memory_order_acquire)].load(std::memory_order_acquire);
  }
  
  inline void enq(Tp *p) {
    assert(p);
    
  retry:
    unsigned icur = head_.load(std::memory_order_acquire);
    assert(icur < Capacity);
    if (buf_[icur].load(std::memory_order_acquire)) {
       __asm volatile("pause" : :);
      goto retry;
    }
    
    // found an empty spot, so we now race for it
    unsigned inext = (icur + 1) % Capacity;
    if (!head_.compare_exchange_strong(icur, inext, std::memory_order_acq_rel)) {
      __asm volatile("pause" : :);
      goto retry;
    }
    
    assert (!buf_[icur].load(std::memory_order_acquire));
    buf_[icur].store(p, std::memory_order_release);
  }
  
  inline Tp* deq() {
    while (!buf_[tail_.load(std::memory_order_acquire)].load(std::memory_order_acquire)) {
      __asm volatile("pause" : :);
    }
    Tp *ret = buf_[tail_.load(std::memory_order_acquire)].load(std::memory_order_acquire);
    buf_[postincr(tail_)].store(nullptr, std::memory_order_release);
    assert(ret);
    return ret;
  }
  
  inline Tp* peek() {
    return buf_[tail_.load(std::memory_order_acquire)].load(std::memory_order_acquire);
  }
  
  // takes a current snapshot of all entries in the queue
  inline void peekall(std::vector<Tp*> &ps, size_t limit = std::numeric_limits<size_t>::max()) {
    ps.clear();
    const unsigned t = tail_.load(std::memory_order_acquire);
    unsigned i = t;
    Tp *p;
    while ((p = buf_[i].load(std::memory_order_acquire)) && ps.size() < limit) {
      ps.push_back(p);
      postincr(i);
      if (i == t) {
        break;
      }
    }
  }
  
private:
  static inline unsigned postincr(unsigned &i) {
    const unsigned ret = i;
    i = (i + 1) % Capacity;
    return ret;
  }
  
  static inline unsigned postincr(std::atomic<unsigned> &i) {
    const unsigned ret = i.load(std::memory_order_acquire);
    i.store((ret + 1) % Capacity, std::memory_order_release);
    return ret;
  }
  
  std::atomic<Tp *> buf_[Capacity];
  std::atomic<unsigned> head_;
  std::atomic<unsigned> tail_;
  
};