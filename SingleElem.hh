
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

	T read(){
		return s_.read_value();
	}

	void write(T v){
		lock();
		s_.set_value(v);
		unlock();
	}

	inline void atomicRead(Version& v, T& val){
		Version v2;
		do{
			v = s_.version();
			fence();
			val = s_.read_value();
			fence();
			v2 = s_.version();
		}while(v!=v2);
	}

  T transRead(Transaction& t) {
    auto& item = t.item(this, this);
    if (item.has_write())
      return item.template write_value<T>();
		else{
			Version v;
			T val;
			atomicRead(v, val);
			if (!item.has_read())
				t.add_read(item, v);
			return val;
		}
  }

  void transWrite(Transaction& t, const T& v) {
    auto& item = t.item(this, this);
    t.add_write(item, v);
  }

	void lock(){
		Versioning::lock(s_.version());
	}

	void unlock(){
		Versioning::unlock(s_.version());
	}

  void lock(TransItem&) {
		lock();
  }

  void unlock(TransItem&) {
		unlock();
  }

  bool check(TransItem& item, Transaction&) {
    return Versioning::versionCheck(s_.version(), item.template read_value<Version>()) &&
      (!Versioning::is_locked(s_.version()) || item.has_write());
  }

  void install(TransItem& item) {
    s_.set_value(item.template write_value<T>());
		Versioning::inc_version(s_.version());
  }

  void cleanup(TransItem& item) {
    if (item.has_write())
      free_packed<T>(item.data.wdata);
  }

private:
  // we store the versioned_value inlined (no added boxing)
  Structure s_;
};
