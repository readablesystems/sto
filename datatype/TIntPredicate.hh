#pragma once
#include <utility>
#include <stdio.h>
#include "TWrapped.hh"
#include "TIntRange.hh"

template <typename T, typename W = TNonopaqueWrapped<T> >
class TIntPredicate : public TObject {
public:
    typedef typename W::version_type version_type;
    typedef TIntRange<T> pred_type;

    TIntPredicate() {
    }
    TIntPredicate(T x)
        : v_(x) {
    }

    operator T() const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>();
        else {
            T result = v_.snapshot(item, vers_);
            get(item).observe(result);
            return result;
        }
    }

    TIntPredicate<T, W>& operator=(T x) {
        Sto::item(this, 0).add_write(x);
        return *this;
    }
    TIntPredicate<T, W>& operator=(const TIntPredicate<T, W>& x) {
        return *this = x.operator T();
    }
    template <typename TT, typename WW>
    TIntPredicate<T, W>& operator=(const TIntPredicate<TT, WW>& x) {
        return *this = x.operator TT();
    }

    T nontrans_read() const {
        return v_.access();
    }
    void nontrans_write(T x) {
        v_.access() = x;
    }

    bool operator==(T x) const {
        return observe_eq(Sto::item(this, 0), x);
    }
    bool operator!=(T x) const {
        return !observe_eq(Sto::item(this, 0), x);
    }
    bool operator<(T x) const {
        return observe_lt(Sto::item(this, 0), x);
    }
    bool operator<=(T x) const {
        return observe_le(Sto::item(this, 0), x);
    }
    bool operator>=(T x) const {
        return !observe_lt(Sto::item(this, 0), x);
    }
    bool operator>(T x) const {
        return !observe_le(Sto::item(this, 0), x);
    }


    // transactional methods
    bool lock(TransItem& item, Transaction& txn) override {
        return txn.try_lock(item, vers_);
    }
    bool check_predicate(TransItem& item, Transaction& txn, bool committing) override {
        TransProxy p(txn, item);
        pred_type pred = item.template predicate_value<pred_type>();
        T value = v_.wait_snapshot(p, vers_, committing);
        return pred.verify(value);
    }
    bool check(TransItem& item, Transaction& txn) override {
        return vers_.cp_check_version(txn, item);
    }
    void install(TransItem& item, Transaction& txn) override {
        v_.write(item.template write_value<T>());
        txn.set_version_unlock(vers_, item);
    }
    void unlock(TransItem& item) override {
        vers_.cp_unlock(item);
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{IntProxy " << (void*) this << "=" << v_.access() << ".v" << vers_.value();
        if (item.has_read())
            w << " R" << item.template read_value<version_type>();
        if (item.has_write())
            w << " =" << item.template write_value<T>();
        if (item.has_predicate() && !item.has_read()) {
            auto& p = item.predicate_value<pred_type>();
            w << " P[" << p.first << "," << p.second << "]";
        }
        w << "}";
    }

private:
    version_type vers_;
    W v_;

    static pred_type& get(TransProxy& item) {
        return item.predicate_value<pred_type>(pred_type::unconstrained());
    }
    T snapshot(TransProxy item) const {
        if (item.has_write())
            return item.template write_value<T>();
        else
            return v_.snapshot(item, vers_);
    }
    bool observe_eq(TransProxy item, T value) const {
        T s = snapshot(item);
        if (!item.has_write())
            get(item).observe_test_eq(s, value);
        return s == value;
    }
    bool observe_lt(TransProxy item, T value) const {
        bool result = snapshot(item) < value;
        if (!item.has_write())
            get(item).observe_lt(value, result);
        return result;
    }
    bool observe_le(TransProxy item, T value) const {
        bool result = snapshot(item) <= value;
        if (!item.has_write())
            get(item).observe_le(value, result);
        return result;
    }
};


template <typename T, typename W>
bool operator==(T a, const TIntPredicate<T, W>& b) {
    return b == a;
}
template <typename T, typename W>
bool operator!=(T a, const TIntPredicate<T, W>& b) {
    return b != a;
}
template <typename T, typename W>
bool operator<(T a, const TIntPredicate<T, W>& b) {
    return b > a;
}
template <typename T, typename W>
bool operator<=(T a, const TIntPredicate<T, W>& b) {
    return b >= a;
}
template <typename T, typename W>
bool operator>=(T a, const TIntPredicate<T, W>& b) {
    return b <= a;
}
template <typename T, typename W>
bool operator>(T a, const TIntPredicate<T, W>& b) {
    return b < a;
}

template <typename T>
std::ostream& operator<<(std::ostream& w, const TIntRange<T>& range) {
    return w << '[' << range.first << ',' << range.second << ']';
}
