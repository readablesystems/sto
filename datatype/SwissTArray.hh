#pragma once
#include "TWrapped.hh"
#include "TArrayProxy.hh"
#include "Transaction.hh"
#include <climits>
#include <pthread.h>
#include <bitset>
#include <fstream>
#include <sstream>
#include <cstdlib>

template <typename T, unsigned N, template <typename> class W = TSwissNonopaqueWrapped>
class SwissTArray : public TObject {
public:
    class iterator;
    class const_iterator;
    typedef T value_type;
    typedef typename W<T>::read_type get_type;
    typedef typename W<T>::version_type version_type;
    typedef unsigned size_type;
    typedef int difference_type;
    typedef TConstArrayProxy<SwissTArray<T, N, W> > const_proxy_type;
    typedef TArrayProxy<SwissTArray<T, N, W> > proxy_type;

    size_type size() const {
        return N;
    }

    const_proxy_type operator[](size_type i) const {
        assert(i < N);
        return const_proxy_type(this, i);
    }
    proxy_type operator[](size_type i) {
        assert(i < N);
        return proxy_type(this, i);
    }

    inline iterator begin();
    inline iterator end();
    inline const_iterator cbegin() const;
    inline const_iterator cend() const;
    inline const_iterator begin() const;
    inline const_iterator end() const;

    // transGet and friends
    bool transGet(size_type i, T& ret) const {
        assert(i < N);
        auto item = Sto::item(this, i);
        if (item.has_write()) {
            ret = item.template write_value<T>();
            return true;
        } else {
            bool ok;
            std::tie(ok, ret) = data_[i].v.read(item, data_[i].version);
            return ok;
        }
    }
    bool transPut(size_type i, T x) {
        assert(i < N);
        return Sto::item(this, i).acquire_write(data_[i].version, x);
    }
    // the throw versions
    get_type transGet_throws(size_type i) const {
        get_type ret;
        bool ok = transGet(i, ret);
        if (!ok)
            Sto::abort();
        return ret;
    }
    void transPut_throws(size_type i, T x) {
        bool ok = transPut(i, x);
        if (!ok)
            Sto::abort();
    }
    get_type nontrans_get(size_type i) const {
        assert(i < N);
        return data_[i].v.access();
    }
    void nontrans_put(size_type i, const T& x) {
        assert(i < N);
        data_[i].v.access() = x;
    }
    void nontrans_put(size_type i, T&& x) {
        assert(i < N);
        data_[i].v.access() = std::move(x);
    }

    // transactional methods
    bool lock(TransItem& item, Transaction& txn) override {
        // read lock is always set successfully if we ever make it to commit
        auto ok = txn.try_lock(item, data_[item.key<size_type>()].version);
        assert(ok);
        return true;
    }
    bool check(TransItem& item, Transaction& txn) override {
        return data_[item.key<size_type>()].version.cp_check_version(txn, item);
        //return item.check_version(data_[item.key<size_type>()].version);
    }
    void install(TransItem& item, Transaction& txn) override {
        size_type i = item.key<size_type>();
        data_[i].v.write(item.write_value<T>());
        txn.set_version_unlock(data_[i].version, item);
    }
    void unlock(TransItem& item) override {
        data_[item.key<size_type>()].version.cp_unlock(item);
    }

private:
    struct elem {
        mutable version_type version;
        W<T> v;
    };
    elem data_[N];

    friend class iterator;
    friend class const_iterator;
};


template <typename T, unsigned N, template <typename> class W>
class SwissTArray<T, N, W>::const_iterator : public std::iterator<std::random_access_iterator_tag, T> {
public:
    typedef SwissTArray<T, N, W> array_type;
    typedef typename array_type::size_type size_type;
    typedef typename array_type::difference_type difference_type;

    const_iterator(const SwissTArray<T, N, W>* a, size_type i)
        : a_(const_cast<array_type*>(a)), i_(i) {
    }

    typename array_type::const_proxy_type operator*() const {
        return array_type::const_proxy_type(a_, i_);
    }

    bool operator==(const const_iterator& x) const {
        return a_ == x.a_ && i_ == x.i_;
    }
    bool operator!=(const const_iterator& x) const {
        return !(*this == x);
    }
    bool operator<(const const_iterator& x) const {
        assert(a_ == x.a_);
        return i_ < x.i_;
    }
    bool operator<=(const const_iterator& x) const {
        assert(a_ == x.a_);
        return i_ <= x.i_;
    }
    bool operator>(const const_iterator& x) const {
        assert(a_ == x.a_);
        return i_ > x.i_;
    }
    bool operator>=(const const_iterator& x) const {
        assert(a_ == x.a_);
        return i_ >= x.i_;
    }

    const_iterator& operator+=(difference_type delta) {
        i_ += delta;
        return *this;
    }
    const_iterator& operator-=(difference_type delta) {
        i_ += delta;
        return *this;
    }
    const_iterator operator+(difference_type delta) const {
        return const_iterator(a_, i_ + delta);
    }
    const_iterator operator-(difference_type delta) const {
        return const_iterator(a_, i_ - delta);
    }
    const_iterator& operator++() {
        ++i_;
        return *this;
    }
    const_iterator operator++(int) {
        ++i_;
        return const_iterator(a_, i_ - 1);
    }
    const_iterator& operator--() {
        --i_;
        return *this;
    }
    const_iterator operator--(int) {
        --i_;
        return const_iterator(a_, i_ + 1);
    }

    difference_type operator-(const const_iterator& x) const {
        assert(a_ == x.a_);
        return i_ - x.i_;
    }

protected:
    array_type* a_;
    size_type i_;
};

template <typename T, unsigned N, template <typename> class W>
class SwissTArray<T, N, W>::iterator : public const_iterator {
public:
    typedef SwissTArray<T, N, W> array_type;
    typedef typename array_type::size_type size_type;
    typedef typename array_type::difference_type difference_type;

    iterator(const SwissTArray<T, N, W>* a, size_type i)
        : const_iterator(a, i) {
    }

    typename array_type::proxy_type operator*() const {
        return array_type::proxy_type(this->a_, this->i_);
    }

    iterator& operator+=(difference_type delta) {
        this->i_ += delta;
        return *this;
    }
    iterator& operator-=(difference_type delta) {
        this->i_ += delta;
        return *this;
    }
    iterator operator+(difference_type delta) const {
        return iterator(this->a_, this->i_ + delta);
    }
    iterator operator-(difference_type delta) const {
        return iterator(this->a_, this->i_ - delta);
    }
    iterator& operator++() {
        ++this->i_;
        return *this;
    }
    iterator operator++(int) {
        ++this->i_;
        return iterator(this->a_, this->i_ - 1);
    }
    iterator& operator--() {
        --this->i_;
        return *this;
    }
    iterator operator--(int) {
        --this->i_;
        return iterator(this->a_, this->i_ + 1);
    }
};

template <typename T, unsigned N, template <typename> class W>
inline auto SwissTArray<T, N, W>::begin() -> iterator {
    return iterator(this, 0);
}

template <typename T, unsigned N, template <typename> class W>
inline auto SwissTArray<T, N, W>::end() -> iterator {
    return iterator(this, N);
}

template <typename T, unsigned N, template <typename> class W>
inline auto SwissTArray<T, N, W>::cbegin() const -> const_iterator {
    return const_iterator(this, 0);
}

template <typename T, unsigned N, template <typename> class W>
inline auto SwissTArray<T, N, W>::cend() const -> const_iterator {
    return const_iterator(this, N);
}

template <typename T, unsigned N, template <typename> class W>
inline auto SwissTArray<T, N, W>::begin() const -> const_iterator {
    return const_iterator(this, 0);
}

template <typename T, unsigned N, template <typename> class W>
inline auto SwissTArray<T, N, W>::end() const -> const_iterator {
    return const_iterator(this, N);
}
