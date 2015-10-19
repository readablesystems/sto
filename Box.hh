#pragma once
#include "Interface.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

template <typename T,  bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from Shared then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class Box : public Shared {
public:
    Box() : s_() {}
    void initialize(Shared* container, int idx) {
        container_ = container;
        idx_ = idx;
    }
    
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
              Sto::abort();
            fence();
            val = s_.read_value();
            fence();
            v = s_.version();
        } while (v != v2);
    }

public:
    T transRead() {
        auto item = Sto::item(container_, idx_);
        if (item.has_write())
            return item.template write_value<T>();
        else {
            version_type v;
            T val;
            atomicRead(v, val);
            if (GenericSTM)
              Sto::check_opacity(v);
            item.add_read(v);
            return val;
        }
    }
    
    /* Overloading cast operation so that we can now directly read values from SingleElem objects */
    operator T() {
        if (Sto::trans_in_progress())
            return transRead();
        else
            return read();
    }

    void transWrite(const T& v) {
        Sto::item(container_, idx_).add_write(v);
    }
  
    /* Overloads = operator with transWrite */
    Box& operator= (const T& v) {
        if (Sto::trans_in_progress())
            transWrite(v);
        else
            write(v);
        return *this;
    }

    void lock() {
        TransactionTid::lock(s_.version());
    }
    
    bool try_lock() {
        return TransactionTid::try_lock(s_.version());
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
    // we Store the versioned_value inlined (no added boxing)
    Structure s_;
    Shared* container_;
    int idx_;
};
