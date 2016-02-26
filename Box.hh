#pragma once
#include "Interface.hh"
#include "versioned_value.hh"
#include "VersionFunctions.hh"

template <typename T,  bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from Shared then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class Box : public Shared {
public:
    static constexpr TransItem::flags_type valid_only_bit = TransItem::user0_bit;
    
    Box() : s_() , local_lock(false) {}
    void initialize(Shared* container, int idx) {
        container_ = container;
        idx_ = idx;
    }
    
    T unsafe_read() const {
        return s_.read_value();
    }
    
    void write(T v) {
        bool locked = try_lock();
        // Can be locked by a thread that is about to abort or locked by a thread that is about to release the locks.
        if (!locked) lock_local();
        s_.set_value(v);
        TransactionTid::type newv = (s_.version() + TransactionTid::increment_value)  & ~TransactionTid::valid_bit;
        release_fence();
        s_.version() = newv;
        if (!locked) unlock_local();
        else unlock();
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
            if (item.flags() & valid_only_bit) {
                item.clear_read();
                item.clear_flags(valid_only_bit);
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
        auto item = Sto::item(container_, idx_).add_write(v);
        if (!item.has_read()) {
            item.add_read(v).add_flags(valid_only_bit);
        }
    }
  
    /* Overloads = operator with transWrite */
    Box& operator= (const T& v) {
        if (Sto::in_progress())
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
        lock_local();
        TransactionTid::unlock(s_.version());
        unlock_local();
    }

    bool lock(TransItem&) {
        lock();
        return true;
    }

    bool check(const TransItem& item, const Transaction&) {
        if (item.flags() & valid_only_bit) {
            return !TransactionTid::is_locked_elsewhere(s_.version());
        }
        return TransactionTid::check_version(s_.version(), item.template read_value<version_type>());
    }

    void install(TransItem& item, const Transaction& t) {
        s_.set_value(item.template write_value<T>());
        if (GenericSTM) {
            TransactionTid::set_version(s_.version(), t.commit_tid());
        } else {
            TransactionTid::inc_invalid_version(s_.version());
        }
    }

    void unlock(TransItem&) {
        unlock();
    }
    
    void lock_local() {
        while (1) {
            bool vv = local_lock;
            if (!vv && bool_cmpxchg(&local_lock, vv, true))
                break;
            relax_fence();
        }
        acquire_fence();
    }
    void unlock_local() {
        assert(local_lock == true);
        local_lock = false;
    }


  protected:
    // we Store the versioned_value inlined (no added boxing)
    Structure s_;
    Shared* container_;
    int idx_;
    bool local_lock;
};
