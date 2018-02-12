#pragma once

template <typename C>
class TConstArrayProxy {
public:
    typedef typename C::value_type value_type;
    typedef typename C::get_type get_type;
    typedef typename C::size_type size_type;

    TConstArrayProxy(const C* c, size_type idx)
        : c_(c), idx_(idx) {
    }

    operator value_type() const {
        return c_->transGet_throws(idx_);
    }

    TConstArrayProxy<C>& operator=(const TConstArrayProxy<C>&) = delete;

protected:
    const C* c_;
    size_type idx_;
};

template <typename C>
class TArrayProxy : public TConstArrayProxy<C> {
public:
    typedef TConstArrayProxy<C> base_type;
    typedef typename TConstArrayProxy<C>::value_type value_type;
    typedef typename TConstArrayProxy<C>::get_type get_type;
    typedef typename TConstArrayProxy<C>::size_type size_type;

    TArrayProxy(C* c, size_type idx)
        : base_type(c, idx) {
    }

    TArrayProxy<C>& operator=(const value_type& x) {
        const_cast<C*>(this->c_)->transPut_throws(this->idx_, x);
        return *this;
    }
    TArrayProxy<C>& operator=(value_type&& x) {
        const_cast<C*>(this->c_)->transPut_throws(this->idx_, std::move(x));
        return *this;
    }
    TArrayProxy<C>& operator=(const TArrayProxy<C>& x) {
        const_cast<C*>(this->c_)->transPut_throws(this->idx_, x.operator get_type());
        return *this;
    }
};
