#pragma once
#include "TWrapped.hh"
#include "TArrayProxy.hh"
#include "Transaction.hh"
#include "compiler.hh"
#include "WriteLock.hh"
#include "ContentionManager.hh"
#include <limits.h>
#include <pthread.h>
#include <bitset>
#include <fstream>
#include <sstream>
#include <stdlib.h>


template <typename T, unsigned N, template <typename> class W = TOpaqueWrapped>
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
    get_type transGet(size_type i) const {
        assert(i < N);
        auto item = Sto::item(this, i);
        if (item.has_write())
            return item.template write_value<T>();
        else
            return data_[i].v.read(item, data_[i].vers);
    }
    void transPut(size_type i, T x) const {
        assert(i < N);
        Sto::item(this, i).add_swiss_write(x, data_[i].wlock, i);
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
        return txn.try_lock(item, data_[item.key<size_type>()].vers);
    }
    bool check(TransItem& item, Transaction&) override {
        return item.check_version(data_[item.key<size_type>()].vers);
    }
    void install(TransItem& item, Transaction& txn) override {
        size_type i = item.key<size_type>();
        data_[i].v.write(item.write_value<T>());
        txn.set_version_unlock(data_[i].vers, item);
        txn.set_version_unlock(data_[i].wlock, item);
    }
    void unlock(TransItem& item) override {
        if (data_[item.key<size_type>()].vers.is_locked())
            data_[item.key<size_type>()].vers.unlock();
        if (data_[item.key<size_type>()].wlock.is_locked())
            data_[item.key<size_type>()].wlock.unlock();
    }

private:
    struct elem {
        version_type vers;
        mutable WriteLock wlock;
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

template <typename T>
inline TransProxy& TransProxy::add_swiss_write(const T& wdata, WriteLock& wlock, int index) {
    return add_swiss_write<T, const T&>(wdata, wlock, index);
}

template <typename T>
inline TransProxy& TransProxy::add_swiss_write(T&& wdata, WriteLock& wlock, int index) {
    typedef typename std::decay<T>::type V;
    return add_swiss_write<V, V&&>(std::move(wdata), wlock, index);
}

template <typename T, typename... Args>
inline TransProxy& TransProxy::add_swiss_write(Args&&... args, WriteLock& wlock, int index) {
    if (wlock.is_locked_here()) {
        item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);
        return *this;
    }

    while(true) {
        if (wlock.is_locked()) {
            bool aborted_by_others = false;
            if (ContentionManager::should_abort(t(), wlock, aborted_by_others, index)) {
		TXP_INCREMENT(txp_wwc_aborts);
                if (aborted_by_others) {
                  TXP_INCREMENT(txp_aborted_by_others);
		  t_->mark_abort_because(item_, "w/w conflict aborted by others" );
                } else {
		  t_->mark_abort_because(item_, "w/w conflict");
                }
		//if (uint32_t(rand()) <= uint32_t(0xFFFFFFFF * 0.001)) {
		  //std::ofstream outfile;
		  //outfile.open(std::to_string(t()->threadid()), std::ios_base::app);
		  //outfile << "Transaction [" << (void*)t() << "] on thread [" << t()->threadid() << "] abort for address [" << item().template key<void*>() << "]" << std::endl;
		  //outfile.close();
		//}
                Sto::abort();
            } else {
		relax_fence();
                continue;
            }
        }

        if (wlock.try_lock()){
            //std::stringstream msg;
            //msg << "Thread " << (t()->threadid()) << " acquires lock for index " << index << "\n";
            //std::cout << msg.str();
            break;
        }
    }


    //if (!has_write()) {
        item().__or_flags(TransItem::write_bit | TransItem::lock_bit);
        item().wdata_ = Packer<T>::pack(t()->buf_, std::forward<Args>(args)...);
        t()->any_writes_ = true;
    //} else
        // TODO: this assumes that a given writer data always has the same type.
        // this is certainly true now but we probably shouldn't assume this in general
        // (hopefully we'll have a system that can automatically call destructors and such
        // which will make our lives much easier)
        // item().wdata_ = Packer<T>::repack(t()->buf_, item().wdata_, std::forward<Args>(args)...);

    ContentionManager::on_write(t());
    return *this;
}


