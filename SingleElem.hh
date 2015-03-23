#pragma once
#include "Interface.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

template <typename T,  bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from Shared then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class SingleElem : public Shared {
public:
    typedef uint32_t Version;
    typedef VersionFunctions<Version> Versioning;

    T read() {
        return s_.read_value();
    }

    void write(T v) {
        lock();
        s_.set_value(v);
        unlock();
    }

  inline void atomicRead(Transaction& t, Version& v, T& val) {
        Version v2;
        do {
            v2 = s_.version();
            if (Versioning::is_locked(v2))
              t.abort();
            fence();
            val = s_.read_value();
            fence();
            v = s_.version();
        } while (v != v2);
    }

    T transRead(Transaction& t) {
        auto item = t.item(this, this);
        if (item.has_write())
            return item.template write_value<T>();
        else {
            Version v;
            T val;
            atomicRead(t, v, val);
            if (GenericSTM) {
                Transaction::tid_type r_tid = Versioning::get_tid(v);
                if (!t.try_check_opacity(r_tid) || Versioning::is_locked(v))
                    t.abort();
            }
            item.add_read(v);
            return val;
        }
    }

    void transWrite(Transaction& t, const T& v) {
        t.item(this, this).add_write(v);
    }

    void lock() {
        Versioning::lock(s_.version());
    }

    void unlock() {
        Versioning::unlock(s_.version());
    }

    void lock(TransItem&) {
        lock();
    }

    void unlock(TransItem&) {
        unlock();
    }

    bool check(const TransItem& item, const Transaction&) {
        return Versioning::versionCheck(s_.version(), item.template read_value<Version>()) &&
            (!Versioning::is_locked(s_.version()) || item.has_write());
    }

    void install(TransItem& item, const Transaction& t) {
        s_.set_value(item.template write_value<T>());
        if (GenericSTM) {
            Versioning::set_version(s_.version(), t.commit_tid());
        } else {
            Versioning::inc_version(s_.version());
        }
    }

  protected:
    // we store the versioned_value inlined (no added boxing)
    Structure s_;
};
