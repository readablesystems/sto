#pragma once
#include "TWrapped.hh"
#include "TArrayProxy.hh"
#include "TIntPredicate.hh"

template <typename T, template <typename> typename W = TOpaqueWrapped>
class TVector_nopred : public TObject {
public:
    using size_type = int;
    using difference_type = int;
private:
    static constexpr size_type default_capacity = 128;
    static constexpr TransactionTid::type dead_bit = TransactionTid::user_bit;
    static constexpr TransItem::flags_type size_flag = TransItem::user0_bit;
    using pred_type = TIntRange<size_type>;
    using key_type = int;
    static constexpr key_type size_key = -1;
    /* All information about the TVector's size is stored under size_key.
       If size_key exists (which it almost always does), then:
       * Its xwrite_value is a pred_type. The `first` component is the initial
         size of the vector. The `second` component is the final size of the
         vector.
       Thus, if xwrite_value.first < xwrite_value.second, the vector grew.
       If xwrite_value.first > xwrite_value.second, the vector shrank.
       Note that the xwrite_value exists even if !has_write(). */

    static constexpr TransItem::flags_type indexed_bit = TransItem::user0_bit;
public:
    class iterator;
    class const_iterator;
    typedef T value_type;
    typedef typename W<T>::read_type get_type;
    typedef typename W<T>::version_type version_type;
    typedef TConstArrayProxy<TVector_nopred<T, W> > const_proxy_type;
    typedef TArrayProxy<TVector_nopred<T, W> > proxy_type;

    TVector_nopred()
        : size_(0), max_size_(0), capacity_(default_capacity) {
        data_ = reinterpret_cast<elem*>(new char[sizeof(elem) * capacity_]);
        for (size_type i = 0; i < capacity_; ++i)
            data_[i].vers = dead_bit;
    }
    ~TVector_nopred() {
        using WT = W<T>;
        for (size_type i = 0; i < max_size_; ++i)
            data_[i].v.~WT();
        delete[] reinterpret_cast<char*>(data_);
    }

    size_type size() const {
        return size_info().second;
    }
    bool empty() const {
        return size() != 0;
    }

    const_proxy_type operator[](size_type i) const {
        return const_proxy_type(this, i);
    }
    proxy_type operator[](size_type i) {
        return proxy_type(this, i);
    }
    const_proxy_type front() const {
        return const_proxy_type(this, 0);
    }
    proxy_type front() {
        return proxy_type(this, 0);
    }
    const_proxy_type back() const {
        auto& sinfo = size_info();
        if (!sinfo.second)
            version_type::opaque_throw(std::out_of_range("TVector::back"));
        return const_proxy_type(this, sinfo.second - 1);
    }
    proxy_type back() {
        auto& sinfo = size_info();
        if (!sinfo.second)
            version_type::opaque_throw(std::out_of_range("TVector::back"));
        return proxy_type(this, sinfo.second - 1);
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
        ++wval.second;
        sitem.add_write();
        Sto::item(this, wval.second - 1).add_write(std::move(x));
    }
    void pop_back() {
        auto sitem = size_item();
        pred_type& wval = sitem.template xwrite_value<pred_type>();
        if (!wval.second)
            throw std::out_of_range("TVector_nopred::pop_back");
        --wval.second;
        sitem.add_write();
        Sto::item(this, wval.second).add_write();
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
        auto item = Sto::item(this, i);
        if (item.has_write())
            return item.template write_value<T>();
        else {
            get_type value = data_[i].v.read(item, data_[i].vers);
            if (item.template read_value<version_type>().value() & dead_bit)
                throw std::out_of_range("TVector_nopred::transGet");
            return value;
        }
    }
    void transPut(size_type i, T x) {
        // XXX should throw out_of_range
        Sto::item(this, i).add_write(std::move(x));
    }

    size_type nontrans_size() const {
        return size_.access();
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

    // transactional methods
    bool lock(TransItem& item, Transaction& txn) {
        auto key = item.template key<key_type>();
        if (key == size_key)
            return txn.try_lock(item, size_vers_);
        else if (!txn.try_lock(item, data_[key].vers))
            return false;
        else if (!(data_[key].vers.value() & dead_bit))
            return true;
        else {
            OptionalTransProxy sitem = txn.check_item(this, size_key);
            // ok to access dead item if we are adding it
            if (sitem && key >= sitem->template xwrite_value<pred_type>().first)
                return true;
            else {
                data_[key].vers.unlock();
                return false;
            }
        }
    }
    bool check(const TransItem& item, const Transaction&) {
        auto key = item.template key<key_type>();
        if (key == size_key)
            return item.check_version(size_vers_);
        else
            return item.check_version(data_[key].vers);
    }
    void install(TransItem& item, const Transaction& txn) {
        auto key = item.template key<key_type>();
        if (key == size_key) {
            pred_type& wval = item.template xwrite_value<pred_type>();
            size_.write(wval.second);
            txn.set_version(size_vers_);
        } else {
            bool exists = true;
            {
                OptionalTransProxy sitem = txn.check_item(this, size_key);
                if (sitem)
                    exists = key < sitem->template xwrite_value<pred_type>().second;
            }
            if (exists && key == max_size_) {
                new(reinterpret_cast<void*>(&data_[key].v)) W<T>(std::move(item.write_value<T>()));
                ++max_size_;
            } else if (exists)
                data_[key].v.write(std::move(item.write_value<T>()));
            txn.set_version_unlock(data_[key].vers, item, exists ? 0 : dead_bit);
        }
    }
    void unlock(TransItem& item) {
        auto key = item.template key<key_type>();
        if (key == size_key)
            size_vers_.unlock();
        else
            data_[key].vers.unlock();
    }
    void print(std::ostream& w, const TransItem& item) const {
        w << "{TVector_nopred<" << typeid(T).name() << "> " << (void*) this;
        key_type key = item.key<key_type>();
        if (key == size_key) {
            w << ".size @" << item.xwrite_value<pred_type>().first;
            if (item.has_read())
                w << " R" << item.read_value<version_type>();
            if (item.has_write())
                w << " =" << item.xwrite_value<pred_type>().second;
        } else {
            w << "[" << key << "]";
            if (item.has_read())
                w << " R" << item.read_value<version_type>();
            if (item.has_write())
                w << " =" << item.write_value<T>();
        }
        w << "}";
    }
    void print(std::ostream& w) const {
        w << "TVector_nopred<" << typeid(T).name() << ">{" << (void*) this
          << "size=" << size_.access() << '@' << size_vers_ << " [";
        for (size_type i = 0; i < size_.access(); ++i) {
            if (i)
                w << ", ";
            if (i >= 10)
                w << '[' << i << ']';
            w << data_[i].v.access() << '@' << data_[i].vers;
        }
        w << "]}";
    }
    bool check_not_locked_here(int here) const {
        if (size_vers_.is_locked_here(here))
            return false;
        size_type max_size = max_size_;
        for (size_type i = 0; i != max_size; ++i)
            if (data_[i].vers.is_locked_here(here))
                return false;
        return true;
    }
    friend std::ostream& operator<<(std::ostream& w, const TVector_nopred<T, W>& v) {
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
    size_type max_size_; // protected by size_vers_ lock
    size_type capacity_;

    // size helpers
    TransProxy size_item() const {
        auto item = Sto::item(this, size_key);
        if (!item.has_read()) {
            size_type sz = size_.read(item, size_vers_);
            item.template xwrite_value<pred_type>() = pred_type{sz, sz};
        }
        return item;
    }
    pred_type& size_info() const {
        return size_item().template xwrite_value<pred_type>();
    }

    friend class iterator;
    friend class const_iterator;
};


template <typename T, template <typename> typename W>
class TVector_nopred<T, W>::const_iterator : public std::iterator<std::random_access_iterator_tag, T> {
public:
    typedef TVector_nopred<T, W> vector_type;
    typedef typename vector_type::size_type size_type;
    typedef typename vector_type::difference_type difference_type;

    const_iterator()
        : a_() {
    }
    const_iterator(const TVector_nopred<T, W>* a, size_type i)
        : a_(const_cast<vector_type*>(a)), i_(i) {
    }

    typename vector_type::const_proxy_type operator*() const {
        return vector_type::const_proxy_type(a_, i_);
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

    inline difference_type operator-(const const_iterator& x) const;

protected:
    vector_type* a_;
    size_type i_;

    friend class TVector_nopred<T, W>;
};

template <typename T, template <typename> typename W>
class TVector_nopred<T, W>::iterator : public const_iterator {
public:
    typedef TVector_nopred<T, W> vector_type;
    typedef typename vector_type::size_type size_type;
    typedef typename vector_type::difference_type difference_type;

    iterator() {
    }
    iterator(const TVector_nopred<T, W>* a, size_type i)
        : const_iterator(a, i) {
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

    difference_type operator-(const const_iterator& x) const {
        return const_iterator::operator-(x);
    }

private:
    friend class TVector_nopred<T, W>;
};


template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::begin() -> iterator {
    return iterator(this, 0);
}

template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::end() -> iterator {
    return iterator(this, size_info().second);
}

template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::cbegin() const -> const_iterator {
    return const_iterator(this, 0);
}

template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::cend() const -> const_iterator {
    return iterator(this, size_info().second);
}

template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::begin() const -> const_iterator {
    return const_iterator(this, 0);
}

template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::end() const -> const_iterator {
    return iterator(this, size_info().second);
}



template <typename T, template <typename> typename W>
inline auto TVector_nopred<T, W>::const_iterator::operator-(const const_iterator& x) const -> difference_type {
    return i_ - x.i_;
}


template <typename T, template <typename> typename W>
void TVector_nopred<T, W>::clear() {
    resize(0);
}

template <typename T, template <typename> typename W>
auto TVector_nopred<T, W>::erase(iterator pos) -> iterator {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    if (pos.i_ >= wval.second)
        version_type::opaque_throw(std::out_of_range("TVector_nopred::erase"));
    for (auto idx = pos.i_; idx != wval.second - 1; ++idx)
        transPut(idx, transGet(idx + 1));
    sitem.add_write(pred_type{wval.first, wval.second - 1});
    return pos;
}

template <typename T, template <typename> typename W>
auto TVector_nopred<T, W>::insert(iterator pos, T value) -> iterator {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    if (pos.i_ > wval.second)
        Sto::abort();
    sitem.add_write(pred_type{wval.first, wval.second + 1});
    for (auto idx = wval.second - 1; idx != pos.i_; --idx)
        transPut(idx, transGet(idx - 1));
    transPut(pos.i_, std::move(value));
    return pos;
}

template <typename T, template <typename> typename W>
void TVector_nopred<T, W>::resize(size_type size, T value) {
    auto sitem = size_item();
    pred_type& wval = sitem.template xwrite_value<pred_type>();
    size_type old_size = wval.second;
    sitem.add_write();
    for (; old_size < size; ++old_size)
        Sto::item(this, old_size).add_write(value);
    for (; old_size > size; --old_size)
        Sto::item(this, old_size - 1).add_write();
    wval.second = size;
}

template <typename T, template <typename> typename W>
void TVector_nopred<T, W>::nontrans_reserve(size_type size) {
    size_type new_capacity = capacity_;
    while (size > new_capacity)
        new_capacity <<= 1;
    if (new_capacity > capacity_) {
        elem* new_data = reinterpret_cast<elem*>(new char[sizeof(elem) * new_capacity]);
        memcpy(new_data, data_, sizeof(elem) * capacity_);
        for (size_type x = capacity_; x != new_capacity; ++x)
            new_data[x].vers = dead_bit;
        Transaction::rcu_delete_array(reinterpret_cast<char*>(data_));
        data_ = new_data;
        capacity_ = new_capacity;
    }
}
