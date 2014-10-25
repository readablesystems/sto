#pragma once

template <typename Version,
          Version invalid_bit = 1<<(sizeof(Version)*8-2),
          Version lock_bit = ((Version)1)<<(sizeof(Version)*8-1),
          Version version_mask = ~(lock_bit | invalid_bit)>
class VersionFunctions {
public:
  static bool versionCheck(Version v1, Version v2) {
    return ((v1 ^ v2) & version_mask) == 0;
  }
  static void inc_version(Version& v) {
    assert(is_locked(v));
    Version cur = v & version_mask;
    cur = (cur+1) & version_mask;
    // set new version and ensure invalid bit is off
    v = (cur | (v & ~version_mask)) & ~invalid_bit;
  }
  static bool is_locked(Version v) {
    return v & lock_bit;
  }
  static void lock(Version& v) {
    while (1) {
      Version cur = v;
      if (!(cur&lock_bit) && bool_cmpxchg(&v, cur, cur|lock_bit)) {
        break;
      }
      relax_fence();
    }
  }
  static void unlock(Version& v) {
    assert(is_locked(v));
    Version cur = v;
    cur &= ~lock_bit;
    v = cur;
  }
  
};
