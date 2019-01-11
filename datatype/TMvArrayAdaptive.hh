#pragma once

#include "Sto.hh"
#include "TArrayProxy.hh"
#include "MVCCAccess.hh"
#include "MVCCStructs.hh"

template <typename T, unsigned N>
class TMvArrayAdaptive : public TObject {
public:
    class iterator;
    class const_iterator;
    typedef T value_type;
    typedef T get_type;
    typedef unsigned size_type;
    typedef int difference_type;
    typedef TConstArrayProxy<TMvArrayAdaptive<T, N> > const_proxy_type;
    typedef TArrayProxy<TMvArrayAdaptive<T, N> > proxy_type;

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
    bool transGet(size_type i, value_type& ret) const {
        assert(i < N);
        auto item = Sto::item(this, i);
        if (item.has_write()) {
            ret = item.template write_value<T>();
            return true;
        }
        else {
            history_type *h = data_[i].v.find(Sto::read_tid());
            TMvAccess::template read<T>(item, h);
            ret = h->v();
            return true;
        }
    }
    value_type transGet_throws(size_type i) const {
        value_type ret;
        bool ok = transGet(i, ret);
        if (!ok)
            throw Transaction::Abort();
        return ret;
    }
    bool transPut(size_type i, T x) {
        assert(i < N);
        Sto::item(this, i).add_write(x);
        return true;
    }
    void transPut_throws(size_type i, T x) {
        bool ok = transPut(i, x);
        if (!ok)
            throw Transaction::Abort();
    }

    get_type nontrans_get(size_type i) const {
        assert(i < N);
        return data_[i].v.nontrans_access();
    }
    void nontrans_put(size_type i, const T& x) {
        assert(i < N);
        data_[i].v.nontrans_access() = x;
    }
    void nontrans_put(size_type i, T&& x) {
        assert(i < N);
        data_[i].v.nontrans_access() = std::move(x);
    }

    // transactional methods
    bool lock(TransItem& item, Transaction& txn) override {
        object_type &v = data_[item.key<size_type>()].v;
        history_type *hprev = nullptr;
        if (item.has_read()) {
            hprev = item.read_value<history_type*>();
        } else {
            hprev = v.find(Sto::read_tid(), false);
        }
        if (Sto::commit_tid() < hprev->rtid()) {
            return false;
        }
        history_type *h = new history_type(
            Sto::commit_tid(), &v, item.write_value<T>(), hprev);
        return v.cp_lock(Sto::commit_tid(), h);
    }
    bool check(TransItem& item, Transaction& txn) override {
        assert(item.has_read());
        fence();
        history_type *hprev = item.read_value<history_type*>();
        return data_[item.key<size_type>()].v.cp_check(Sto::commit_tid(), hprev);
    }
    void install(TransItem& item, Transaction& txn) override {
        data_[item.key<size_type>()].v.cp_install();
    }
    void unlock(TransItem& item) override {
        // no-op
    }

private:
    typedef MvObject<T> object_type;
    typedef typename object_type::history_type history_type;

    struct elem {
        object_type v;
    };
    elem data_[N];

    friend class iterator;
    friend class const_iterator;
};


template <typename T, unsigned N>
class TMvArrayAdaptive<T, N>::const_iterator : public std::iterator<std::random_access_iterator_tag, T> {
public:
    typedef TMvArrayAdaptive<T, N> array_type;
    typedef typename array_type::size_type size_type;
    typedef typename array_type::difference_type difference_type;

    const_iterator(const TMvArrayAdaptive<T, N>* a, size_type i)
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

template <typename T, unsigned N>
class TMvArrayAdaptive<T, N>::iterator : public const_iterator {
public:
    typedef TMvArrayAdaptive<T, N> array_type;
    typedef typename array_type::size_type size_type;
    typedef typename array_type::difference_type difference_type;

    iterator(const TMvArrayAdaptive<T, N>* a, size_type i)
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

template <typename T, unsigned N>
inline auto TMvArrayAdaptive<T, N>::begin() -> iterator {
    return iterator(this, 0);
}

template <typename T, unsigned N>
inline auto TMvArrayAdaptive<T, N>::end() -> iterator {
    return iterator(this, N);
}

template <typename T, unsigned N>
inline auto TMvArrayAdaptive<T, N>::cbegin() const -> const_iterator {
    return const_iterator(this, 0);
}

template <typename T, unsigned N>
inline auto TMvArrayAdaptive<T, N>::cend() const -> const_iterator {
    return const_iterator(this, N);
}

template <typename T, unsigned N>
inline auto TMvArrayAdaptive<T, N>::begin() const -> const_iterator {
    return const_iterator(this, 0);
}

template <typename T, unsigned N>
inline auto TMvArrayAdaptive<T, N>::end() const -> const_iterator {
    return const_iterator(this, N);
}
