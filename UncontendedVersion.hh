

class UncontendedVersion {
public:
  typedef uint64_t type;
  static constexpr type lock_bit = type(1);
  static constexpr type increment_value = type(2);

  UncontendedVersion() : vers_(0) {}
  void lock() {
    // this really means "dirty" rather than lock. We assume there are no other
    // concurrent writers so we just have to do an atomic write.
    vers_ = vers_ | lock_bit;
  }
  void unlock() {
    vers_ = vers_ & ~lock_bit;
  }
  bool is_locked() const {
    return vers_ & lock_bit;
  }
  void inc_version() {
    vers_ = vers_ + increment_value;
  }
  void inc_and_unlock() {
    type new_v = (vers_ + increment_value) & ~lock_bit;
    fence();
    vers_ = new_v;
  }
  bool operator==(UncontendedVersion v) const {
    return vers_ == v.vers_;
  }
  bool operator!=(UncontendedVersion v) const {
    return vers_ != v.vers_;
  }
private:
  type vers_;
};
