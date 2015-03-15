#pragma once

// Tagged pointer which uses lower 3 bits of pointer for tag bits
// (so, requires pointer is one returned by malloc, i.e. 8-byte aligned)
// does not require extra space on 32-bit architectures
template <typename T>
class TaggedLow {
public:

  TaggedLow(T* ptr, uint8_t flgs) : ptr_(ptr) {
    set_flags(flgs);
  }

  TaggedLow(T* ptr) : ptr_(ptr) {}

  operator T*() {
    return (T*)ptr();
  }

  operator const T*() const {
    return ptr();
  }

  TaggedLow& operator=(T* p) {
    uint8_t f = flags();
    ptr_ = p;
    set_flags(f);
    return *this;
  }

  const T* ptr() const {
    return (const T*)((uintptr_t)ptr_ & ~7);
  }

  uint8_t flags() {
    return (uintptr_t)ptr_ & 7;
  }

  bool has_flags(uint8_t flgs) {
    return flags() & flgs;
  }

  void set_flags(uint8_t flags) {
    assert((flags & ~7) == 0);
    auto p = (uintptr_t)ptr_;
    p &= ~7;
    p |= flags;
    fence();
    ptr_ = (T*)p;
  }

  void or_flags(uint8_t flags) {
    assert((flags & ~7) == 0);
    ptr_ = (T*)((uintptr_t)ptr_ | flags);
  }

  void atomic_add_flags(uint8_t flags) {
    assert((flags & ~7) == 0);
    while (1) {
      auto cur = (uintptr_t)ptr_;
      fence();
      if ((cur & flags) == 0 && bool_cmpxchg(&ptr_, (T*)cur, (T*)(cur | flags))) {
        break;
      }
      relax_fence();
    }
  }

private:
  T* ptr_;
};
