#pragma once
#include "string_base.hh"

template <typename Stuff>
class stuffed_str {
public:
  typedef Stuff stuff_type;

  struct StandardMalloc {
    void *operator()(size_t s) {
      return malloc(s);
    }
  };

  template <typename Malloc = StandardMalloc>
  static stuffed_str* make(const char *str, int len, int capacity, const Stuff& val, Malloc m = Malloc()) {
    // TODO: it might be better if we just take the max of size_for() and capacity
    assert(size_for(len) <= capacity);
    //    printf("%d from %lu\n", alloc_size, len + sizeof(stuffed_str));
    auto vs = (stuffed_str*)m(capacity);
    new (vs) stuffed_str(val, len, capacity - sizeof(stuffed_str), str);
    return vs;
  }

  template <typename Malloc = StandardMalloc>
  static stuffed_str* make(const std::string& s, const Stuff& val, Malloc m = Malloc()) {
    return make(s.data(), s.length(), size_for(s.length()), val, m);
  }

  template <typename Str, typename Malloc = StandardMalloc>
  static stuffed_str* make(const lcdf::String_base<Str>& s, const Stuff& val, Malloc m = Malloc()) {
    return make(s.data(), s.length(), size_for(s.length()), val, m);
  }

  static unsigned pad(unsigned v)
  {
    if (likely(v <= 512)) {
      return (v + 15) & ~15;
    }
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if UINT_MAX == UINT64_MAX
    v |= v >> 32;
#endif
    v++;
    return v;
  }

  static inline int size_for(int len) {
    return pad(len + sizeof(stuffed_str));
  }

  bool needs_resize(int len) {
    return len > (int)capacity_;
  }

  template <typename Malloc = StandardMalloc>
  stuffed_str* reserve(int len, Malloc m = Malloc()) {
    if (likely(!needs_resize(len))) {
      return this;
    }
    return stuffed_str::make(buf_, size_, len, stuff_, m);
  }

  // returns NULL if replacement could happen without a new malloc, otherwise returns new stuffed_str*
  // malloc should be a functor that takes a size and returns a buffer of that size
  template <typename Malloc = StandardMalloc>
  stuffed_str* replace(const char *str, int len, Malloc m = Malloc()) {
    if (likely(!needs_resize(len))) {
      size_ = len;
      memcpy(buf_, str, len);
      return this;
    }
    return stuffed_str::make(str, len, size_for(len), stuff_, m);
  }

  const char *data() {
    return buf_;
  }
  
  int length() {
    return size_;
  }
  
  int capacity() {
    return capacity_;
  }

  Stuff& stuff() {
    return stuff_;
  }

  Stuff stuff() const {
    return stuff_;
  }

private:
  stuffed_str(const Stuff& stuff, uint32_t size, uint32_t capacity, const char *buf) :
    stuff_(stuff), size_(size), capacity_(capacity) {
    memcpy(buf_, buf, size);
  }

  Stuff stuff_;
  uint32_t size_;
  uint32_t capacity_;
  char buf_[0];
};
