#pragma once

#include <stdint.h>

#if INTPTR_MAX == INT64_MAX
#define IS64BIT
#elif INTPTR_MAX == INT32_MAX
#define IS32BIT
#else "Only support 32 and 64 bit systems"
#error
#endif

// 64 bits including a pointer and 16 bits of flags
// (technically we have 19 flag bits on a 64 bit machine and 35 on a 32 bit machine but who cares)
template <typename T>
class Tagged64 {
#ifdef IS64BIT
  static constexpr uintptr_t shift = (sizeof(void*)*8 - 16);
  static constexpr uintptr_t flagmask = ((uintptr_t)0xffff) << shift;
  static constexpr uintptr_t ptrmask = ~(flagmask);
#endif

public:

  Tagged64(T *p) : p_(p)
#ifdef IS32BIT
                 , flags_(0)
#endif
  { assert(ptr() == p); }

  T* ptr() const {
#ifdef IS32BIT
    return p_;
#else
    return (T*)((uintptr_t)p_ & ptrmask);
#endif
  }

  uint16_t flags() const {
#ifdef IS32BIT
    return flags_;
#else
    return ((uintptr_t)p_ & flagmask) >> shift;
#endif
  }

  void set_flags(uint16_t flags) {
#ifdef IS32BIT
    flags_ = flags;
#else
    p_ = (T*)(((uintptr_t)p_ & ~flagmask) | shifted(flags));
#endif
  }

  void or_flags(uint16_t flags) {
#ifdef IS32BIT
    flags_ |= flags;
#else
    p_ = (T*)((uintptr_t)p_ | shifted(flags));
#endif
  }
  
  void rm_flags(uint16_t flags) {
#ifdef IS32BIT
    flags_ &= ~flags;
#else
    p_ = (T*)((uintptr_t)p_ & ~shifted(flags));
#endif
  }

  bool has_flags(uint16_t flags) const {
#ifdef IS32BIT
    return (flags_ & flags) == flags;
#else
    return (this->flags() & flags) == flags;
#endif
  }

  bool operator<(const Tagged64<T> other) const {
    return ptr() < other.ptr() || (ptr() == other.ptr() && 
                                   flags() < other.flags());
  }

private:
  inline uintptr_t shifted(uint16_t flags) const {
    return (uintptr_t)flags << shift;
  }

  T *p_;
#ifdef IS32BIT
  uint16_t unused;
  uint16_t flags_;
#endif

};
