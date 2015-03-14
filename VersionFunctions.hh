#pragma once

template <typename T>
class VersionFunctions {
public:
    typedef T version_type;
    static constexpr T lock_bit = version_type(1) << (sizeof(version_type) * 8 - 1);
    static constexpr T spacer_bit = version_type(1) << (sizeof(version_type) * 8 - 2);
    static constexpr T version_mask = ~(lock_bit | spacer_bit);

  static bool versionCheck(version_type v1, version_type v2) {
    return ((v1 ^ v2) & version_mask) == 0;
  }
  static void inc_version(version_type& v) {
    assert(is_locked(v));
    version_type cur = (v + 1) & ~spacer_bit;
    release_fence();
    v = cur;
  }
  static void set_version(version_type& v, version_type cur) {
    assert(is_locked(v));
    assert((cur & version_mask) == cur);
    release_fence();
    v = (cur | (v & lock_bit));
  }
  
  static version_type get_tid(version_type v) {
    return (v & version_mask);
  }
  
 
  static bool is_locked(version_type v) {
    return v & lock_bit;
  }
  static void lock(version_type& v) {
    while (1) {
      version_type cur = v;
      if (!(cur&lock_bit) && bool_cmpxchg(&v, cur, cur|lock_bit)) {
        break;
      }
      relax_fence();
    }
    acquire_fence();
  }
  static void unlock(version_type& v) {
    assert(is_locked(v));
    version_type cur = v & ~lock_bit;
    release_fence();
    v = cur;
  }

};
