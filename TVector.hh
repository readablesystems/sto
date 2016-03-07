#pragma once
#include "TWrapped.hh"
#include "TArrayProxy.hh"
#include "TIntPredicate.hh"

template <typename T, template <typename> typename W = TOpaqueWrapped>
class TVector : public TObject {
public:
    typedef unsigned size_type;
    typedef int difference_type;
private:
    static constexpr size_type default_capacity = 128;
    typedef TIntRange<unsigned> pred_type;
    typedef int key_type;
    static constexpr key_type size_key = 0;
    /* All information about the TVector's size is stored under size_key.
       If size_key exists (which it almost always does), then:
       * Its predicate_value is a pred_type recording constraints on the size.
       * Its xwrite_value is a pred_type. The `first` component is the initial
         size of the vector. The `second` component is the final size of the
         vector.
       Thus, if xwrite_value.first < xwrite_value.second, the vector grew.
       If xwrite_value.first > xwrite_value.second, the vector shrank.
       Note that the xwrite_value exists even if !has_write(). */

    TransProxy index_item(size_type idx) const {
        TransProxy sitem = size_item();
        pred_type& pval = sitem.predicate_value<pred_type>();
        pred_type& wval = sitem.xwrite_value<pred_type>();
        if (idx >= wval.second)
            Sto::abort();
        key_type key = idx >= wval.first ? -key_type(idx - wval.first) - 1 : idx + 1;
        TransProxy item = Sto::item(this, key);
        if (key < 0 || (item.has_write() && !item.has_flag(indexed_bit)))
            pval.observe(wval.first);
        else
            pval.observe_gt(idx);
        return item.add_flags(indexed_bit);
    }

    static constexpr TransItem::flags_type indexed_bit = TransItem::user0_bit;
public:
    class iterator;
    class const_iterator;
    using size_proxy = TIntRangeSizeProxy<size_type>;
    using difference_proxy = TIntRangeDifferenceProxy<size_type>;
    typedef T value_type;
    typedef typename W<T>::read_type get_type;
    typedef typename W<T>::version_type version_type;
    typedef TConstArrayProxy<TVector<T, W> > const_proxy_type;
    typedef TArrayProxy<TVector<T, W> > proxy_type;

    TVector()
        : size_(0), max_size_(0), capacity_(default_capacity) {
        data_ = reinterpret_cast<elem*>(new char[sizeof(elem) * capacity_]);
    }
    ~TVector() {
        using WT = W<T>;
        for (size_type i = 0; i < max_size_; ++i)
            data_[i].v.~WT();
        delete[] reinterpret_cast<char*>(data_);
    }

    inline size_proxy size() const;

    const_proxy_type operator[](size_type i) const {
        return const_proxy_type(this, i);
    }
    proxy_type operator[](size_type i) {
        return proxy_type(this, i);
    }

    inline iterator begin();
    inline iterator end();
    inline const_iterator cbegin() const;
    inline const_iterator cend() const;
    inline const_iterator begin() const;
    inline const_iterator end() const;

    void push_back(T x) {
        auto sitem = size_item();
        pred_type& wval = sitem.template xwrite_value<pred_type>();
        size_type new_size = wval.second + 1;
        sitem.add_write(pred_type{wval.first, new_size});
        key_type key = new_size <= wval.first ? new_size : -(new_size - wval.first);
        Sto::item(this, key).add_write(std::move(x));
    }
    void pop_back() {
        auto sitem = size_item();
        pred_type& wval = sitem.template xwrite_value<pred_type>();
        size_type new_size = wval.second - 1;
        sitem.add_write(pred_type{wval.first, new_size});
        if (new_size < wval.first)
            size_predicate(sitem).observe_ge(wval.first - new_size);
    }

    void clear();
    iterator erase(iterator pos);
    iterator insert(iterator pos, T x);
    void resize(size_type size, T x = T());

    void nontrans_reserve(size_type size);
    void nontrans_push_back(T x) {
        size_type& sz = size_.access();
        assert(sz < capacity_);
        if (sz == max_size_) {
            new(reinterpret_cast<void*>(&data_[sz].v)) W<T>(std::move(x));
            ++max_size_;
        } else
            data_[sz].v.write(std::move(x));
        ++sz;
    }

    // transGet and friends
    get_type transGet(size_type i) const {
        auto item = index_item(i);
        if (item.has_write())
            return item.template write_value<T>();
        else
            return data_[i].v.read(item, data_[i].vers);
    }
    void transPut(size_type i, T x) {
        auto item = index_item(i);
        item.add_write(std::move(x));
    }

    get_type nontrans_get(size_type i) const {
        assert(i < size_.access());
        return data_[i].v.access();
    }
    void nontrans_put(size_type i, const T& x) {
        assert(i < size_.access());
        data_[i].v.access() = x;
    }
    void nontrans_put(size_type i, T&& x) {
        assert(i < size_.access());
        data_[i].v.access() = std::move(x);
    }

    // size helpers
    TransProxy size_item() const {
        auto item = Sto::item(this, size_key);
        if (!item.has_predicate()) {
            item.set_predicate(pred_type::unconstrained());
            size_type sz = size_.snapshot(item, size_vers_);
            item.template xwrite_value<pred_type>() = pred_type{sz, sz};
        }
        return item;
    }
    static pred_type& size_predicate(TransProxy sitem) {
        return sitem.template predicate_value<pred_type>();
    }
    pred_type& size_predicate() const {
        return size_predicate(size_item());
    }
    static size_type current_size(TransProxy sitem) {
        return sitem.template xwrite_value<pred_type>().second;
    }
    static size_type original_size(TransProxy sitem) {
        return sitem.template xwrite_value<pred_type>().first;
    }
    size_type nontrans_size() const {
        return size_.access();
    }

    // transactional methods
    bool check_predicate(TransItem& item, Transaction& txn, bool committing) {
        TransProxy p(txn, item);
        pred_type pred = item.template predicate_value<pred_type>();
        size_type value = committing ? size_.read(p, size_vers_) : size_.snapshot(p, size_vers_);
        return pred.verify(value);
    }
    bool lock(TransItem& item, Transaction& txn) {
        auto key = item.template key<key_type>();
        if (key == size_key)
            return txn.try_lock(size_vers_);
        else if (key > 0)
            return txn.try_lock(data_[key - 1].vers);
        else {
            assert(size_vers_.is_locked_here());
            return true;
        }
    }
    bool check(const TransItem& item, const Transaction&) {
        auto key = item.template key<key_type>();
        if (key == size_key)
            return item.check_version(size_vers_);
        else
            return item.check_version(data_[key - 1].vers);
    }
    void install(TransItem& item, const Transaction& txn) {
        auto key = item.template key<key_type>();
        if (key == size_key) {
            original_size_ = size_.access();
            pred_type& wval = item.template xwrite_value<pred_type>();
            size_.write(original_size_ + wval.second - wval.first);
            size_vers_.set_version(txn.commit_tid());
            return;
        }
        size_type idx = key - 1;
        if (size_vers_.is_locked_here()) {
            // maybe we have popped past this point
            if (key < 0)
                idx = original_size_ - key - 1;
            else if (!item.has_flag(indexed_bit)) {
                TransProxy sitem = const_cast<Transaction&>(txn).item(this, size_key);
                idx = original_size_ - (original_size(sitem) - idx);
            }
            if (idx >= size_.access()) {
                item.clear_needs_unlock();
                return;
            }
        }
        assert(idx <= max_size_ && idx < capacity_);
        if (idx == max_size_) {
            new(reinterpret_cast<void*>(&data_[idx].v)) W<T>(std::move(item.write_value<T>()));
            ++max_size_;
        } else
            data_[idx].v.write(std::move(item.write_value<T>()));
        data_[idx].vers = txn.commit_tid(); // set_version_unlock
        item.clear_needs_unlock();
    }
    void unlock(TransItem& item) {
        auto key = item.template key<key_type>();
        if (key == size_key)
            size_vers_.unlock();
        else if (key > 0)
            data_[key - 1].vers.unlock();
    }
    void print(std::ostream& w, const TransItem& item) const {
        w << "{TVector<";
        const char* pf = strstr(__PRETTY_FUNCTION__, "with T = ");
        if (pf) {
            pf += 9;
            const char* semi = strchr(pf, ';');
            w.write(pf, semi - pf);
        }
        w << "> " << (void*) this;
        key_type key = item.key<key_type>();
        if (key == size_key) {
            w << ".size " << item.predicate_value<pred_type>()
              << '@' << item.xwrite_value<pred_type>().first;
            if (item.has_write())
                w << " =" << item.xwrite_value<pred_type>().second;
        } else {
            w << "[";
            if (key > 0)
                w << (key - 1);
            else
                w << "push" << key;
            w << "]";
            if (item.has_read())
                w << " ?" << item.read_value<version_type>();
            if (item.has_write())
                w << " =" << item.write_value<T>();
        }
        w << "}";
    }
    void print(std::ostream& w) const {
        w << "TVector<";
        const char* pf = strstr(__PRETTY_FUNCTION__, "with T = ");
        if (pf) {
            pf += 9;
            const char* semi = strchr(pf, ';');
            w.write(pf, semi - pf);
        }
        w << ">{" << (void*) this
          << "size=" << size_.access() << '@' << size_vers_ << " [";
        for (size_type i = 0; i < size_.access(); ++i) {
            if (i)
                w << ", ";
            w << data_[i].v.access() << '@' << data_[i].vers;
        }
        w << "]}";
    }
    friend std::ostream& operator<<(std::ostream& w, const TVector<T, W>& v) {
        v.print(w);
        return w;
    }

private:
    struct elem {
        version_type vers;
        W<T> v;
    };
    elem* data_;
    W<size_type> size_;
    version_type size_vers_;
    size_type original_size_; // protected by size_vers_ lock
    size_type max_size_; // protected by size_vers_ lock
    size_type capacity_;

    friend class iterator;
    friend class const_iterator;
};


template <typename T, template <typename> typename W>
class TVector<T, W>::const_iterator : public std::iterator<std::random_access_iterator_tag, T> {
public:
    typedef TVector<T, W> vector_type;
    typedef typename vector_type::size_type size_type;
    typedef typename vector_type::difference_type difference_type;
    typedef typename vector_type::pred_type pred_type;
    typedef typename vector_type::difference_proxy difference_proxy;

    const_iterator()
        : a_() {
    }
    const_iterator(const TVector<T, W>* a, size_type i, size_type cend1)
        : a_(const_cast<vector_type*>(a)), i_(i), cend1_(cend1) {
    }

    typename vector_type::const_proxy_type operator*() const {
        return vector_type::const_proxy_type(a_, i_);
    }

    bool operator==(const const_iterator& x) const {
        if (a_ != x.a_)
            return false;
        if (different_end(x)) {
            difference_type d = cend1_ ? x.i_ - i_ : i_ - x.i_;
            a_->size_predicate().observe_eq(d + cend1_ + x.cend1_ - 1, i_ == x.i_);
        }
        return i_ == x.i_;
    }
    bool operator!=(const const_iterator& x) const {
        return !(*this == x);
    }
    bool operator<(const const_iterator& x) const {
        assert(a_ == x.a_);
        if (cend1_ && !x.cend1_)
            a_->size_predicate().observe_lt(x.i_ - i_ + cend1_ - 1, i_ < x.i_);
        else if (x.cend1_ && !cend1_)
            a_->size_predicate().observe_gt(i_ - x.i_ + x.cend1_ - 1, i_ < x.i_);
        return i_ < x.i_;
    }
    bool operator<=(const const_iterator& x) const {
        assert(a_ == x.a_);
        if (cend1_ && !x.cend1_)
            a_->size_predicate().observe_le(x.i_ - i_ + cend1_ - 1, i_ <= x.i_);
        else if (x.cend1_ && !cend1_)
            a_->size_predicate().observe_ge(i_ - x.i_ + x.cend1_ - 1, i_ <= x.i_);
        return i_ <= x.i_;
    }
    bool operator>(const const_iterator& x) const {
        return x < *this;
    }
    bool operator>=(const const_iterator& x) const {
        return x <= *this;
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
        return const_iterator(a_, i_ + delta, cend1_);
    }
    const_iterator operator-(difference_type delta) const {
        return const_iterator(a_, i_ - delta, cend1_);
    }
    const_iterator& operator++() {
        ++i_;
        return *this;
    }
    const_iterator operator++(int) {
        ++i_;
        return const_iterator(a_, i_ - 1, cend1_);
    }
    const_iterator& operator--() {
        --i_;
        return *this;
    }
    const_iterator operator--(int) {
        --i_;
        return const_iterator(a_, i_ + 1, cend1_);
    }

    inline difference_proxy operator-(const const_iterator& x) const;

protected:
    vector_type* a_;
    size_type i_;
    size_type cend1_;

    bool different_end(const const_iterator& x) const {
        return (cend1_ ^ x.cend1_) && !(cend1_ & x.cend1_);
    }
    friend class TVector<T, W>;
};

template <typename T, template <typename> typename W>
class TVector<T, W>::iterator : public const_iterator {
public:
    typedef TVector<T, W> vector_type;
    typedef typename vector_type::size_type size_type;
    typedef typename vector_type::difference_type difference_type;

    iterator() {
    }
    iterator(const TVector<T, W>* a, size_type i, size_type cend1)
        : const_iterator(a, i, cend1) {
    }

    typename vector_type::proxy_type operator*() const {
        return vector_type::proxy_type(this->a_, this->i_);
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
        return iterator(this->a_, this->i_ + delta, this->cend1_);
    }
    iterator operator-(difference_type delta) const {
        return iterator(this->a_, this->i_ - delta, this->cend1_);
    }
    iterator& operator++() {
        ++this->i_;
        return *this;
    }
    iterator operator++(int) {
        ++this->i_;
        return iterator(this->a_, this->i_ - 1, this->cend1_);
    }
    iterator& operator--() {
        --this->i_;
        return *this;
    }
    iterator operator--(int) {
        --this->i_;
        return iterator(this->a_, this->i_ + 1, this->cend1_);
    }

    difference_proxy operator-(const const_iterator& x) const {
        return const_iterator::operator-(x);
    }

private:
    friend class TVector<T, W>;
};

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::begin() -> iterator {
    return iterator(this, 0, 0);
}

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::end() -> iterator {
    TransProxy sitem = size_item();
    return iterator(this, current_size(sitem), original_size(sitem) + 1);
}

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::cbegin() const -> const_iterator {
    return const_iterator(this, 0, 0);
}

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::cend() const -> const_iterator {
    TransProxy sitem = size_item();
    return iterator(this, current_size(sitem), original_size(sitem) + 1);
}

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::begin() const -> const_iterator {
    return const_iterator(this, 0, 0);
}

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::end() const -> const_iterator {
    TransProxy sitem = size_item();
    return iterator(this, current_size(sitem), original_size(sitem) + 1);
}


template <typename T, template <typename> typename W>
inline auto TVector<T, W>::size() const -> size_proxy {
    auto sitem = size_item();
    return size_proxy(&size_predicate(sitem), original_size(sitem), current_size(sitem) - original_size(sitem));
}

template <typename T, template <typename> typename W>
inline auto TVector<T, W>::const_iterator::operator-(const const_iterator& x) const -> difference_proxy {
    assert(a_ == x.a_);
    if (different_end(x)) {
        size_type cend1 = cend1_ + x.cend1_ - 1;
        return difference_proxy(&a_->size_predicate(), cend1, i_ - x.i_ - cend1);
    } else
        return difference_proxy(nullptr, 0, i_ - x.i_);
}


template <typename T, template <typename> typename W>
void TVector<T, W>::clear() {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    sitem.add_write(pred_type{wval.first, 0});
}

template <typename T, template <typename> typename W>
auto TVector<T, W>::erase(iterator pos) -> iterator {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    if (pos.i_ >= wval.second)
        Sto::abort();
    size_predicate(sitem).observe(wval.first);
    for (auto idx = pos.i_; idx != wval.second - 1; ++idx)
        transPut(idx, transGet(idx + 1));
    sitem.add_write(pred_type{wval.first, wval.second - 1});
    return pos;
}

template <typename T, template <typename> typename W>
auto TVector<T, W>::insert(iterator pos, T value) -> iterator {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    if (pos.i_ >= wval.second)
        Sto::abort();
    sitem.add_write(pred_type{wval.first, wval.second + 1});
    size_predicate(sitem).observe(wval.first);
    for (auto idx = wval.second - 1; idx != pos.i_; --idx)
        transPut(idx, transGet(idx - 1));
    transPut(pos.i_, std::move(value));
    return pos;
}

template <typename T, template <typename> typename W>
void TVector<T, W>::resize(size_type size, T value) {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    size_type old_size = wval.second;
    sitem.add_write(pred_type{wval.first, size});
    if (old_size < size) {
        size_predicate(sitem).observe(wval.first);
        do {
            // inlined portion of push_back (don't double-change size)
            ++old_size;
            key_type key = old_size <= wval.first ? old_size : -(old_size - wval.first);
            Sto::item(this, key).add_write(value);
        } while (old_size < size);
    }
}

template <typename T, template <typename> typename W>
void TVector<T, W>::nontrans_reserve(size_type size) {
    size_type new_capacity = capacity_;
    while (size > new_capacity)
        new_capacity <<= 1;
    if (new_capacity > capacity_) {
        elem* new_data = reinterpret_cast<elem*>(new char[sizeof(elem) * new_capacity]);
        memcpy(new_data, data_, sizeof(elem) * max_size_);
        Transaction::rcu_delete_array(reinterpret_cast<char*>(data_));
        data_ = new_data;
        capacity_ = new_capacity;
    }
}
