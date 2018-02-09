#pragma once

// a no-op RCU class for MassTrans (aka, completely unimportant code)

#if !RCU
class debug_threadinfo {
public:
#if 0
  debug_threadinfo()
    : ts_(0) { // XXX?
  }
#endif

  class rcu_callback {
  public:
    virtual void operator()(debug_threadinfo& ti) = 0;
  };

private:
  static inline void rcu_callback_function(void* p) {
    debug_threadinfo ti;
    static_cast<rcu_callback*>(p)->operator()(ti);
  }


public:
  // XXX Correct node timstamps are needed for recovery, but for no other
  // reason.
  kvtimestamp_t operation_timestamp() const {
    return 0;
  }
  kvtimestamp_t update_timestamp() const {
    return ts_;
  }
  kvtimestamp_t update_timestamp(kvtimestamp_t x) const {
    if (circular_int<kvtimestamp_t>::less_equal(ts_, x))
      // x might be a marker timestamp; ensure result is not
      ts_ = (x | 1) + 1;
    return ts_;
  }
  kvtimestamp_t update_timestamp(kvtimestamp_t x, kvtimestamp_t y) const {
    if (circular_int<kvtimestamp_t>::less(x, y))
      x = y;
    if (circular_int<kvtimestamp_t>::less_equal(ts_, x))
      // x might be a marker timestamp; ensure result is not
      ts_ = (x | 1) + 1;
    return ts_;
  }
  void increment_timestamp() {
    ts_ += 2;
  }
  void advance_timestamp(kvtimestamp_t x) {
    if (circular_int<kvtimestamp_t>::less(ts_, x))
      ts_ = x;
  }

  // event counters
  void mark(threadcounter) {
  }
  void mark(threadcounter, int64_t) {
  }
  bool has_counter(threadcounter) const {
    return false;
  }
  uint64_t counter(threadcounter) const {
    return 0;
  }

  /** @brief Return a function object that calls mark(ci); relax_fence().
   *
   * This function object can be used to count the number of relax_fence()s
   * executed. */
  relax_fence_function accounting_relax_fence(threadcounter) {
    return relax_fence_function();
  }

  class accounting_relax_fence_function {
  public:
    template <typename V>
    void operator()(V) {
      relax_fence();
    }
  };
  /** @brief Return a function object that calls mark(ci); relax_fence().
   *
   * This function object can be used to count the number of relax_fence()s
   * executed. */
  accounting_relax_fence_function stable_fence() {
    return accounting_relax_fence_function();
  }

  relax_fence_function lock_fence(threadcounter) {
    return relax_fence_function();
  }

  void* allocate(size_t sz, memtag) {
    return malloc(sz);
  }
  void deallocate(void* , size_t , memtag) {
    // in C++ allocators, 'p' must be nonnull
    //free(p);
  }
  void deallocate_rcu(void *, size_t , memtag) {
  }

  void* pool_allocate(size_t sz, memtag) {
    int nl = (sz + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
    return malloc(nl * CACHE_LINE_SIZE);
  }
  void pool_deallocate(void* , size_t , memtag) {
  }
  void pool_deallocate_rcu(void* , size_t , memtag) {
  }

  // RCU
  void rcu_register(rcu_callback *) {
    //    scoped_rcu_base<false> guard;
    //rcu::s_instance.free_with_fn(cb, rcu_callback_function);
  }

private:
  mutable kvtimestamp_t ts_;
};

#endif
