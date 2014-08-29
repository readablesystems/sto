
#include "Interface.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

template <typename T, typename Structure = versioned_value_struct<T>>
// if we're inheriting from Shared then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class SingleElem : public Shared {
public:
  typedef uint32_t Version;
  typedef VersionFunctions<Version, 0> Versioning;

  T transRead(Transaction& t) {
    // TODO: depending on what we're really going for some of this we could just make
    // the Structure do
    Version v = s_.version();
    fence();
    // TODO: do we actually need to do so called "atomic" read?? (check v# after value read too)
    auto val = s_.read_value();
    auto& item = t.item(this, this);
    if (item.has_write())
      return item.template write_value<T>();
    if (!item.has_read())
      t.add_read(item, v);
    return val;
  }

  void transWrite(Transaction& t, const T& v) {
    auto& item = t.item(this, this);
    t.add_write(item, v);
  }

  void lock(TransItem&) {
    Versioning::lock(s_.version());
  }
  void unlock(TransItem&) {
    Versioning::unlock(s_.version());
  }
  bool check(TransItem& item, Transaction&) {
    return Versioning::versionCheck(s_.version(), item.template read_value<Version>()) &&
      (!Versioning::is_locked(s_.version()) || !item.has_write());
  }
  void install(TransItem& item) {
    s_.set_value(item.template write_value<T>());
  }
  void cleanup(TransItem& item) {
    if (item.has_write())
      free_packed<T>(item.data.wdata);
  }

private:
  // we store the versioned_value inlined (no added boxing)
  Structure s_;
};
