#pragma once
#include <limits>
#include <utility>
#include <stdio.h>
#include "TWrapped.hh"

template <typename T>
class TIntProxy {
public:

};

template <typename T, typename W = TWrapped<T> >
class TIntPredicate : public TObject {
public:
    typedef typename W::version_type version_type;
    struct pred_type {
        T first;
        T second;
    };

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
            T result = v_.snapshot(vers_);
            observe(get(item), result);
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
    bool lock(TransItem&, Transaction& txn) {
        return txn.try_lock(vers_);
    }
    virtual bool check_predicate(TransItem& item, Transaction& txn) {
        TransProxy p(txn, item);
        pred_type pred = item.template predicate_value<pred_type>();
        return discharge(pred, v_.read(p, vers_));
    }
    virtual bool check(const TransItem& item, const Transaction&) {
        return item.check_version(vers_);
    }
    virtual void install(TransItem& item, const Transaction& txn) {
        v_.write(item.template write_value<T>());
        vers_.set_version_unlock(txn.commit_tid());
        item.clear_needs_unlock();
    }
    virtual void unlock(TransItem&) {
        vers_.unlock();
    }
    virtual void print(std::ostream& w, const TransItem& item) const {
        w << "<IntProxy " << (void*) this << "=" << v_.access() << ".v" << vers_.value();
        if (item.has_read())
            w << " ?" << item.template read_value<version_type>();
        if (item.has_write())
            w << " =" << item.template write_value<T>();
        if (item.has_predicate()) {
            auto& p = item.predicate_value<pred_type>();
            w << " P[" << p.first << "," << p.second << "]";
        }
        w << ">";
    }


    // static methods for managing predicates
    static void observe(pred_type& p, T value) {
        if (p.first <= value && value <= p.second)
            p.first = p.second = value;
        else
            Sto::abort();
    }
    static void observe_eq(pred_type& p, T value, bool was_eq) {
        if (was_eq) {
            p.first = std::max(p.first, value);
            p.second = std::min(p.second, value);
        } else if (value < p.first || p.second < value)
            /* do nothing */;
        else if (value - p.first <= p.second - value)
            p.first = value + 1;
        else
            p.second = value - 1;
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_lt(pred_type& p, T value, bool was_lt) {
        if (was_lt)
            p.second = std::min(p.second, value - 1);
        else
            p.first = std::max(p.first, value);
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_le(pred_type& p, T value, bool was_le) {
        if (was_le)
            p.second = std::min(p.second, value);
        else
            p.first = std::max(p.first, value + 1);
        if (p.first > p.second)
            Sto::abort();
    }
    static bool discharge(pred_type p, T value) {
        return p.first <= value && value <= p.second;
    }

private:
    version_type vers_;
    W v_;

    static pred_type& get(TransProxy& item) {
        return item.predicate_value<pred_type>(pred_type{std::numeric_limits<T>::min(), std::numeric_limits<T>::max()});
    }
    T snapshot(TransProxy item) const {
        return item.has_write() ? item.template write_value<T>() : v_.snapshot(vers_);
    }
    bool observe_eq(TransProxy item, T value) const {
        bool result = snapshot(item) == value;
        if (!item.has_write())
            observe_eq(get(item), value, result);
        return result;
    }
    bool observe_lt(TransProxy item, T value) const {
        bool result = snapshot(item) < value;
        if (!item.has_write())
            observe_lt(get(item), value, result);
        return result;
    }
    bool observe_le(TransProxy item, T value) const {
        bool result = snapshot(item) <= value;
        if (!item.has_write())
            observe_le(get(item), value, result);
        return result;
    }
};
