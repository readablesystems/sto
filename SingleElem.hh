#pragma once
#include "Interface.hh"
#include "versioned_value.hh"

#ifdef UBENCHMARK
#  define OPACITY_NONE 0
#  define OPACITY_TL2  1
#  define OPACITY_SLOW 2
extern int opacity;
#endif

template <typename T, bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from Shared then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class SingleElem : public Shared {
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
#ifdef UBENCHMARK
            if (opacity == OPACITY_TL2) {
#else
            if (GenericSTM) {
#endif
                Sto::check_opacity(v);
#ifdef UBENCHMARK
            } else if (opacity == OPACITY_SLOW) {
                Sto::check_opacity(TransactionTid::nonopaque_bit);
#endif
            }
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

    bool lock(TransItem& item, Transaction& txn) {
        return txn.try_lock(item, s_.version());
    }

    bool check(const TransItem& item, const Transaction&) {
        return TransactionTid::check_version(s_.version(), item.template read_value<version_type>());
    }

    void install(TransItem& item, const Transaction& t) {
        s_.set_value(item.template write_value<T>());
#ifdef UBENCHMARK
        if (opacity == OPACITY_TL2) {
#else
        if (GenericSTM) {
#endif
            TransactionTid::set_version(s_.version(), Sto::commit_tid());
        } else {
            TransactionTid::inc_nonopaque_version(s_.version());
        }
    }

    void unlock(TransItem&) {
        unlock();
    }

  protected:
    // we Store the versioned_value inlined (no added boxing)
    Structure s_;
};
