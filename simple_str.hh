#pragma once
// Adopted from stuffed_str
class simple_str {
public:

  struct StandardMalloc {
    void *operator()(size_t s) {
      return malloc(s);
    }
  };

  template <typename Malloc = StandardMalloc>
  static simple_str *make(const char *str, int len, int capacity, Malloc m = Malloc()) {
    // TODO: it might be better if we just take the max of size_for() and capacity
    assert(size_for(len) <= capacity);
    //    printf("%d from %lu\n", alloc_size, len + sizeof(simple_str));
    auto vs = (simple_str*)m(capacity);
    new (vs) simple_str(len, capacity - sizeof(simple_str), str);
    return vs;
  }

  template <typename StringType, typename Malloc = StandardMalloc>
  static simple_str *make(const StringType& s, Malloc m = Malloc()) {
    return make(s.data(), s.length(), size_for(s.length()), m);
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
    return pad(len + sizeof(simple_str));
  }

  bool needs_resize(int len) {
    return len > (int)capacity_;
  }

  template <typename Malloc = StandardMalloc>
  simple_str* reserve(int len, Malloc m = Malloc()) {
    if (likely(!needs_resize(len))) {
      return this;
    }
    return simple_str::make(buf_, size_, len, m);
  }

  // returns NULL if replacement could happen without a new malloc, otherwise returns new stuffed_str*
  // malloc should be a functor that takes a size and returns a buffer of that size
  template <typename Malloc = StandardMalloc>
  simple_str* replace(const char *str, int len, Malloc m = Malloc()) {
    if (likely(!needs_resize(len))) {
      size_ = len;
      memcpy(buf_, str, len);
      return this;
    }
    return simple_str::make(str, len, size_for(len), m);
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

  inline simple_str& operator= (const std::string& v) {
    this->replace(v.data(), v.length());  
    return *this;
  }
  
  operator std::string() {
    return std::string(this->data(), this->length());  
  }


  simple_str(const std::string& v) {
    this->replace(v.data(), v.length());
  }
private:
  simple_str(uint32_t size, uint32_t capacity, const char *buf) :
    size_(size), capacity_(capacity) {
    memcpy(buf_, buf, size);
  }

  uint32_t size_;
  uint32_t capacity_;
  char buf_[0];
};

