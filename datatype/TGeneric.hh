#pragma once
#include "Transaction.hh"
#include "TWrapped.hh"

template <template <typename> class W = TOpaqueWrapped>
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
        return W<T>::read(word, it, version(word)).second;
    }
    template <typename T, typename U>
    void write(T* word, U value) {
        static_assert(sizeof(T) <= sizeof(void*), "T larger than void*");
        static_assert(mass::is_trivially_copyable<T>::value, "T nontrivial");
        Sto::item(this, word).add_write(T(value)).assign_flags(sizeof(T) << TransItem::userf_shift);
    }


    bool lock(TransItem& item, Transaction& txn) override {
        version_type& vers = version(item.template key<void*>());
        return vers.is_locked_here() || txn.try_lock(item, vers);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return version(item.template key<void*>()).cp_check_version(txn, item);
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
            vers.cp_unlock(item);
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{TGeneric @" << item.key<void*>();
        if (item.has_read())
            w << " R" << item.read_value<version_type>();
        if (item.has_write())
            w << " =" << item.write_value<void*>() << "/" << item.shifted_user_flags();
        w << "}";
    }

private:
    version_type table_[table_size];

    inline version_type& version(void* k) {
        return table_[(reinterpret_cast<uintptr_t>(k) >> 3) % table_size];
    }
};

typedef TBasicGeneric<TOpaqueWrapped> TGeneric;
typedef TBasicGeneric<TNonopaqueWrapped> TNonopaqueGeneric;
