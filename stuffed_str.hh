
template <typename Stuff>
class stuffed_str {
public:

  template <typename Malloc>
  static stuffed_str *make(const char *str, int len, const Stuff& val, Malloc m) {
    int alloc_size = size_for(len);
    //    printf("%d from %lu\n", alloc_size, len + sizeof(stuffed_str));
    auto vs = (stuffed_str*)m(alloc_size);
    new (vs) stuffed_str(val, len, alloc_size - sizeof(stuffed_str), str);
    return vs;
  }

  template <typename StringType, typename Malloc>
  static stuffed_str *make(const StringType& s, const Stuff& val, Malloc m) {
    return make(s.data(), s.length(), val, m);
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

  int needs_resize(int len) {
    return len > (int)capacity_;
  }

  // returns NULL if replacement could happen without a new malloc, otherwise returns new stuffed_str*
  // malloc should be a functor that takes a size and returns a buffer of that size
  template <typename Malloc>
  stuffed_str* replace(const char *str, int len, Malloc m) {
    if (likely(!needs_resize(len))) {
      size_ = len;
      memcpy(buf_, str, len);
      return NULL;
    }
    return stuffed_str::make(str, len, stuff_, m);
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
