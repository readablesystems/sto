#pragma once
#include "Transaction.hh"
#include "TWrapped.hh"
#include "ConcurrencyControl.hh"
#include "ContentionManager.hh"
#include "Transaction.hh"
#include "SwissTArray.hh"

#include <sys/resource.h>
#include <assert.h>

//typedef TSwissVersion<false> WriteLock;

template <template <typename> class W = TSwissWrapped>
class SwissTBasicGeneric : public TObject {
public:
    typedef typename W<int>::version_type version_type;
    static constexpr unsigned table_size = 1 << 22;

    template <typename T>
    bool read(T* word, T& ret) {
        // XXX this code doesn't work right if `word` is a union member;
        // we assume that every value at location `word` has the same size
        static_assert(sizeof(T) <= sizeof(void*), "T larger than void*");
        static_assert(mass::is_trivially_copyable<T>::value, "T nontrivial");
        auto item = Sto::item(this, word);
        if (item.has_write()) {
            //assert(it.shifted_user_flags() == sizeof(T));
            ret = item.template write_value<T>();
            return true;
        } else {
            bool success;
            std::tie(success, ret) = W<T>::read(word, item, version(word));
            return success;
       }
    }
    template <typename T>
    void read_throws(T* word, T& ret) {
        if (!read(word, ret)) {
            Sto::abort();
        }
    }

    template <typename T, typename U>
    bool write(T* word, U value) {
        static_assert(sizeof(T) <= sizeof(void*), "T larger than void*");
        static_assert(mass::is_trivially_copyable<T>::value, "T nontrivial");
        auto item = Sto::item(this, word);
        auto val = item.acquire_write(version(word), T(value)); //.assign_flags(sizeof(T) << TransItem::userf_shift
        return val;
    }
    template <typename T, typename U>
    void write_throws(T* word, U value) {
        if (!write(word, value)) {
            Sto::abort();
        }
    }


    bool lock(TransItem& item, Transaction& txn) override {
        (void) txn;
        version_type& vers = version(item.template key<void*>());
        return vers.is_locked_here() || vers.cp_try_lock(item, -1);  // ID not used
    }
    bool check(TransItem& item, Transaction& txn) override {
        return version(item.template key<void*>()).cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        void* word = item.template key<void*>();
        void* data = item.template write_value<void*>();
        memcpy(word, &data, 4);
        //memcpy(word, &data, item.shifted_user_flags());
        txn.set_version(version(word));
    }
    inline void unlock(TransItem& item) override {
        version_type& vers = version(item.template key<void*>());
        //WriteLock& wl = wlock(item.template key<void*>());
        if (vers.is_locked_here())
            vers.cp_unlock(item);
        //if (wl.is_locked())
        //    wl.cp_unlock(item);
    }

    void print(std::ostream& w, const TransItem& item) const override {
        w << "{SwissTGeneric @" << item.key<void*>();
        w << "}";
    }
    void init_table_counts() {
    }
    void print_table_counts() {
    }

private:
    version_type table_[table_size];
    //WriteLock wlock_table_[table_size];
    //int table_count_[table_size];
    //int wlock_table_count_[table_size];

    inline version_type& version(void* k) {
#ifdef __x86_64__
        int index = (reinterpret_cast<uintptr_t>(k) >> 5) % table_size;
        //table_count_[index] += 1;
        return table_[index];
#else /* __i386__ */
        int index = (reinterpret_cast<uintptr_t>(k) >> 4) % table_size;
        //table_count_[index] += 1;
	return table_[(index];
#endif
    }

#if 0
     inline WriteLock& wlock(void* k, int index) {
        assert(false);
#ifdef __x86_64__
        return wlock_table_[(reinterpret_cast<uintptr_t>(k) >> 5) % table_size];
#else /* __i386__ */
	return wlock_table_[(reinterpret_cast<uintptr_t>(k) >> 4) % table_size];
#endif
    }

      inline WriteLock& wlock(void* k) {
#ifdef __x86_64__
        int index = (reinterpret_cast<uintptr_t>(k) >> 5) % table_size;
        //wlock_table_count_[index] += 1;
        return wlock_table_[index];
#else /* __i386__ */
        int index = (reinterpret_cast<uintptr_t>(k) >> 4) % table_size;
        //wlock_table_count_[index] += 1;
	return wlock_table_[index];
#endif
    }
#endif

  
};

typedef SwissTBasicGeneric<TSwissOpaqueWrapped> SwissTGeneric;
typedef SwissTBasicGeneric<TSwissNonopaqueWrapped> SwissTNonopaqueGeneric;


