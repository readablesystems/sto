#pragma once
// Adopted from stuffed_str
class simple_str {
public:
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
    int res = pad(len + sizeof(simple_str));
    return res;

  }

  bool needs_resize(int len) {
    return len > (int)capacity_;
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
    int len = v.length();
    size_ = len;
    if (unlikely(needs_resize(len))) {
      capacity_ = size_for(len) - sizeof(simple_str);
      buf_ = (char *) malloc(capacity_);
    }
    memcpy(buf_, v.data(), len);
    return *this;    
  }
  
  operator std::string() {
    return std::string(this->data(), this->length());  
  }

  simple_str(const std::string& v) {
    int len = v.length();
    size_ = len;
    capacity_ = size_for(len) - sizeof(simple_str);
    buf_ = (char *) malloc(capacity_);
    memcpy(buf_, v.data(), len);
  }
private:
  uint32_t size_;
  uint32_t capacity_;
  char *buf_;
};

