#pragma once
#include "Transaction.hh"
#include "TWrapped.hh"

template <template <typename> typename W = TOpaqueWrapped>
class TBasicGeneric : public TObject {
public:
    typedef typename W<int>::version_type version_type;
    static constexpr unsigned table_size = 1 << 15;

    template <typename T>
    T read(T* word) {
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
    void write(T* word, U value) {
        static_assert(sizeof(T) <= sizeof(void*), "T larger than void*");
        static_assert(mass::is_trivially_copyable<T>::value, "T nontrivial");
        Sto::item(this, word).add_write(T(value)).assign_flags(sizeof(T) << TransItem::userf_shift);
    }


    bool lock(TransItem& item, Transaction& txn) {
        version_type& vers = version(item.template key<void*>());
        return vers.is_locked_here() || txn.try_lock(vers);
    }
    bool check(const TransItem& item, const Transaction&) {
        return item.check_version(version(item.template key<void*>()));
    }
    void install(TransItem& item, const Transaction& t) {
        void* word = item.template key<void*>();
        void* data = item.template write_value<void*>();
        memcpy(word, &data, item.shifted_user_flags());
        version(word).set_version(t.commit_tid());
    }
    void unlock(TransItem& item) {
        version_type& vers = version(item.template key<void*>());
        if (vers.is_locked_here())
            vers.unlock();
    }

private:
    version_type table_[table_size];

    inline version_type& version(void* k) {
        return table_[(reinterpret_cast<uintptr_t>(k) >> 3) % table_size];
    }
};

typedef TBasicGeneric<TOpaqueWrapped> TGeneric;
typedef TBasicGeneric<TNonopaqueWrapped> TNonopaqueGeneric;
