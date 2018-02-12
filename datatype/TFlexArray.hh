#pragma once

#include "Sto.hh"

template <typename T, unsigned N, template <typename> class W>
class TFlexArray : public TObject {
public:
    typedef T value_type;
    typedef typename W<T>::read_type get_type;
    typedef typename W<T>::version_type version_type;
    typedef unsigned size_type;

    TFlexArray() {
        for (unsigned i = 0; i < N; ++i)
            new (data_ + i) elem();
    }

    size_type size() const {
        return N;
    }

    bool transGet(size_type i, value_type &ret) const {
        assert(i < N);
        auto item = Sto::item(this, i);
        if (item.has_write()) {
            ret = item.template write_value<T>();
            return true;
        } else {
            auto result = data_[i].v.read(item, data_[i].vers);
            ret = result.second;
            return result.first;
        }
    }

    bool transPut(size_type i, T x) const {
        assert(i < N);
        return Sto::item(this, i).acquire_write(data_[i].vers, x);
    }

    get_type nontrans_get(size_type i) const {
        assert(i < N);
        return data_[i].v.access();
    }

    void nontrans_put(size_type i, const T &x) {
        assert(i < N);
        data_[i].v.access() = x;
    }

    void nontrans_put(size_type i, T &&x) {
        assert(i < N);
        data_[i].v.access() = std::move(x);
    }

    // TObject interface
    bool lock(TransItem &item, Transaction &txn) override {
        return txn.try_lock(item, data_[item.key<size_type>()].vers);
    }

    bool check(TransItem &item, Transaction &txn) override {
        return data_[item.key<size_type>()].vers.cp_check_version(txn, item);
    }

    void install(TransItem &item, Transaction &txn) override {
        size_type i = item.key<size_type>();
        data_[i].v.write(item.write_value<T>());
        txn.set_version_unlock(data_[i].vers, item);
    }

    void unlock(TransItem &item) override {
        data_[item.key<size_type>()].vers.cp_unlock(item);
    }

private:
    struct elem {
        elem() = default;
        mutable version_type vers;
        W<T> v;
    };
    elem data_[N];
};

template <typename T, unsigned N>
using TOCCArray = TFlexArray<T, N, TNonopaqueWrapped>;

template <typename T, unsigned N>
using TAdaptiveArray = TFlexArray<T, N, TAdaptiveNonopaqueWrapped>;

template <typename T, unsigned N>
using TSwissArray = TFlexArray<T, N, TSwissNonopaqueWrapped>;

template <typename T, unsigned N>
using TicTocArray = TFlexArray<T, N, TicTocNonopaqueWrapped>;
