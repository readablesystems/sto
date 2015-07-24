#pragma once
#include "Interface.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

template <typename T,  bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from Shared then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class SingleElem : public Shared {
public:
    T read() {
        return s_.read_value();
    }

    void write(T v) {
        TransactionTid::lock(s_.version());
        s_.set_value(v);
        TransactionTid::type newv = (s_.version() + TransactionTid::increment_value) & ~(TransactionTid::lock_bit | TransactionTid::valid_bit);
        release_fence();
        s_.version() = newv;
    }

private:
    typedef TransactionTid::type version_type;

    inline void atomicRead(version_type& v, T& val) {
        version_type v2;
        do {
            v2 = s_.version();
            if (TransactionTid::is_locked(v2))
              STO::abort();
            fence();
            val = s_.read_value();
            fence();
            v = s_.version();
        } while (v != v2);
    }

public:
    T transRead() {
        auto item = STO::item(this, this);
        if (item.has_write())
            return item.template write_value<T>();
        else {
            version_type v;
            T val;
            atomicRead(v, val);
            if (GenericSTM)
              STO::check_opacity(v);
            item.add_read(v);
            return val;
        }
    }

    void transWrite(const T& v) {
      STO::item(this, this).add_write(v);
    }

    void lock() {
        TransactionTid::lock(s_.version());
    }

    void unlock() {
        TransactionTid::unlock(s_.version());
    }

    void lock(TransItem&) {
        lock();
    }

    void unlock(TransItem&) {
        unlock();
    }

    bool check(const TransItem& item, const Transaction&) {
        return TransactionTid::same_version(s_.version(), item.template read_value<version_type>())
            && (!TransactionTid::is_locked(s_.version()) || item.has_write());
    }

    void install(TransItem& item, const Transaction& t) {
        s_.set_value(item.template write_value<T>());
        if (GenericSTM) {
            TransactionTid::set_version(s_.version(), t.commit_tid());
        } else {
            TransactionTid::inc_invalid_version(s_.version());
        }
    }

  protected:
    // we store the versioned_value inlined (no added boxing)
    Structure s_;
};
