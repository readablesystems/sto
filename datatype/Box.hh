#pragma once
#include "Interface.hh"
#include "versioned_value.hh"

template <typename T,  bool GenericSTM = false, typename Structure = versioned_value_struct<T>>
// if we're inheriting from TObject then a SingleElem adds both a version word and a vtable word
// (not much else we can do though)
class Box {
public:
    static constexpr TransItem::flags_type valid_only_bit = TransItem::user0_bit;
    
    Box() : s_() , local_lock(false) {}
    
    T unsafe_read() const {
        return s_.read_value();
    }
    
    void write(T v) {
        bool locked = try_lock();
        // Can be locked by a thread that is about to abort or locked by a thread that is about to release the locks.
        if (!locked) lock_local();
        s_.set_value(v);
        TransactionTid::type newv = (s_.version() + TransactionTid::increment_value) | TransactionTid::nonopaque_bit;
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
    T transRead(TransProxy item) {
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
    
    void transWrite(TransProxy item, const T& v) {
        item.add_write(v);
        if (!item.has_read()) {
            item.add_read(v).add_flags(valid_only_bit);
        }
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

    bool is_locked_elsewhere() const {
        return TransactionTid::is_locked_elsewhere(s_.version());
    }
    bool check_version(version_type v) const {
        return TransactionTid::check_version(s_.version(), v);
    }
    void install(TransItem& item, const Transaction& t) {
        s_.set_value(item.template write_value<T>());
        if (GenericSTM) {
            TransactionTid::set_version(s_.version(), t.commit_tid());
        } else {
            TransactionTid::inc_nonopaque_version(s_.version());
        }
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
    bool local_lock;
};
