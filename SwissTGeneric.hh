#pragma once
#include "Transaction.hh"
#include "TWrapped.hh"
#include "WriteLock.hh"
#include "ContentionManager.hh"
#include "Transaction.hh"
#include "SwissTArray.hh"

#include <sys/resource.h>

template <template <typename> class W = TOpaqueWrapped>
class SwissTBasicGeneric : public TObject {
public:
    typedef typename W<int>::version_type version_type;
    static constexpr unsigned table_size = 1 << 15;

    template <typename T>
    T read(T* word, int index) {
        // XXX this code doesn't work right if `word` is a union member;
        // we assume that every value at location `word` has the same size
        static_assert(sizeof(T) <= sizeof(void*), "T larger than void*");
        static_assert(mass::is_trivially_copyable<T>::value, "T nontrivial");
        auto it = Sto::item(this, word);
        if (it.has_write()) {
            assert(it.shifted_user_flags() == sizeof(T));
            return it.template write_value<T>();
        }
        return W<T>::read(word, it, version(word));
    }
    template <typename T, typename U>
    void write(T* word, U value, int index) {
        static_assert(sizeof(T) <= sizeof(void*), "T larger than void*");
        static_assert(mass::is_trivially_copyable<T>::value, "T nontrivial");
	//struct rusage ru1, ru2;
        //getrusage(RUSAGE_THREAD, &ru1);
        Sto::item(this, word).add_swiss_write(T(value), wlock(word), index).assign_flags(sizeof(T) << TransItem::userf_shift);
        //getrusage(RUSAGE_THREAD, &ru2);
        //long local_csws = ru2.ru_nvcsw - ru1.ru_nvcsw;
        //TXP_ACCOUNT(txp_csws, local_csws);
    }


    bool lock(TransItem& item, Transaction& txn) override {
        version_type& vers = version(item.template key<void*>());
        //return vers.is_locked_here() || txn.try_lock(item, vers);
        return vers.is_locked_here() || vers.set_lock();
    }
    bool check(TransItem& item, Transaction&) override {
        return item.check_version(version(item.template key<void*>()));
    }
    void install(TransItem& item, Transaction& txn) override {
        void* word = item.template key<void*>();
        void* data = item.template write_value<void*>();
        memcpy(word, &data, item.shifted_user_flags());
        txn.set_version(version(word));
    }
    void unlock(TransItem& item) override {
        version_type& vers = version(item.template key<void*>());
        if (vers.is_locked_here())
            vers.unlock();
	WriteLock& wl = wlock(item.template key<void*>());
	if (wl.is_locked())
	    wl.unlock();
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{SwissTGeneric @" << item.key<void*>();
        //if (item.has_read())
        //    w << " R" << item.read_value<version_type>();
        //if (item.has_write())
        //    w << " =" << item.write_value<void*>() << "/" << item.shifted_user_flags();
        w << "}";
    }

private:
    version_type table_[table_size];
    WriteLock wlock_table_[table_size];

    inline version_type& version(void* k) {
#ifdef __x86_64__
        return table_[(reinterpret_cast<uintptr_t>(k) >> 5) % table_size];
#else /* __i386__ */
	return table_[(reinterpret_cast<uintptr_t>(k) >> 4) % table_size];
#endif
    }

     inline WriteLock& wlock(void* k) {
#ifdef __x86_64__
        return wlock_table_[(reinterpret_cast<uintptr_t>(k) >> 5) % table_size];
#else /* __i386__ */
	return wlock_table_[(reinterpret_cast<uintptr_t>(k) >> 4) % table_size];
#endif
    }

   
};

typedef SwissTBasicGeneric<TOpaqueWrapped> SwissTGeneric;
typedef SwissTBasicGeneric<TNonopaqueWrapped> SwissTNonopaqueGeneric;


