#pragma once
#include "Interface.hh"
#include "versioned_value.hh"

template <typename T, bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from TObject then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class SingleElem : public TObject {
public:
    T unsafe_read() const {
        return s_.read_value();
    }
    void unsafe_write(T v) {
        s_.set_value(v);
    }
    
    void write(T v) {
        TransactionTid::lock(s_.version());
        s_.set_value(v);
        TransactionTid::set_version_unlock(s_.version(), TransactionTid::next_nonopaque_version(s_.version()));
    }

private:
    typedef TransactionTid::type version_type;

    inline void atomicRead(version_type& v, T& val) const {
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
    T transRead() const {
        auto item = Sto::item(this, this);
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
        if (Sto::in_progress())
            return transRead();
        else
            return unsafe_read();
    }

    void transWrite(const T& v) {
        Sto::item(this, this).add_write(v);
    }
  
    /* Overloads = operator with transWrite */
    SingleElem& operator= (const T& v) {
        if (Sto::in_progress())
            transWrite(v);
        else
            unsafe_write(v);
        return *this;
    }

    void lock() {
        TransactionTid::lock(s_.version());
    }

    void unlock() {
        TransactionTid::unlock(s_.version());
    }

    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, s_.version());
    }

    bool check(TransItem& item, Transaction&) override {
        return TransactionTid::check_version(s_.version(), item.template read_value<version_type>());
    }

    void install(TransItem& item, Transaction& t) override {
        s_.set_value(item.template write_value<T>());
        if (GenericSTM) {
            TransactionTid::set_version(s_.version(), t.commit_tid());
        } else {
            TransactionTid::inc_nonopaque_version(s_.version());
        }
    }

    void unlock(TransItem&) override {
        unlock();
    }

  protected:
    // we Store the versioned_value inlined (no added boxing)
    Structure s_;
};
