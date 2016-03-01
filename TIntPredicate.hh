#pragma once
#include <limits>
#include <utility>
#include <stdio.h>
#include "TWrapped.hh"

template <typename T, typename W = TWrapped<T> >
class TIntPredicate : public TObject {
public:
    typedef typename W::version_type version_type;
    struct pair_type {
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
            observe_eq(item, result);
            return result;
        }
    }
    TIntPredicate<T, W>& operator=(T x) {
        auto item = Sto::item(this, 0);
        if (item.has_predicate()) {
            discharge_abort(item, v_.read(item, vers_));
            item.clear_predicate();
        }
        item.add_write(x);
        return *this;
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
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>() == x;
        else {
            T result = v_.snapshot(vers_);
            observe_eq(item, x, result == x);
            return result == x;
        }
    }
    bool operator!=(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>() != x;
        else {
            T result = v_.snapshot(vers_);
            observe_eq(item, x, result == x);
            return result != x;
        }
    }
    bool operator<(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>() < x;
        else {
            T result = v_.snapshot(vers_);
            observe_lt(item, x, result < x);
            return result < x;
        }
    }
    bool operator<=(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>() <= x;
        else {
            T result = v_.snapshot(vers_);
            observe_le(item, x, result <= x);
            return result <= x;
        }
    }
    bool operator>=(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>() >= x;
        else {
            T result = v_.snapshot(vers_);
            observe_lt(item, x, result < x);
            return result >= x;
        }
    }
    bool operator>(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_write())
            return item.template write_value<T>() > x;
        else {
            T result = v_.snapshot(vers_);
            observe_le(item, x, result <= x);
            return result > x;
        }
    }


    // transactional methods
    bool lock(TransItem&, Transaction& txn) {
        return txn.try_lock(vers_);
    }
    virtual bool check_predicate(TransItem& item, Transaction& txn) {
        TransProxy p(txn, item);
        pair_type pred = item.template predicate_value<pair_type>();
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
            auto& p = item.predicate_value<pair_type>();
            w << " P[" << p.first << "," << p.second << "]";
        }
        w << ">";
    }


    // static methods for managing predicates
    static pair_type& get(TransProxy& item) {
        return item.predicate_value<pair_type>(pair_type{std::numeric_limits<T>::min(), std::numeric_limits<T>::max()});
    }
    static void observe_eq(TransProxy& item, T value) {
        auto& p = get(item);
        if (p.first <= value && value <= p.second)
            p.first = p.second = value;
        else
            Sto::abort();
    }
    static void observe_lt(TransProxy& item, T value) {
        auto& p = get(item);
        p.second = std::min(p.second, value - 1);
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_le(TransProxy& item, T value) {
        auto& p = get(item);
        p.second = std::min(p.second, value);
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_gt(TransProxy& item, T value) {
        auto& p = get(item);
        p.first = std::max(p.first, value + 1);
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_ge(TransProxy& item, T value) {
        auto& p = get(item);
        p.first = std::max(p.first, value);
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_ne(TransProxy& item, T value) {
        auto& p = get(item);
        if (value < p.first || p.second < value)
            /* skip */;
        else if (value - p.first <= p.second - value)
            p.first = value + 1;
        else
            p.second = value - 1;
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_eq(TransProxy& item, T value, bool was_eq) {
        was_eq ? observe_eq(item, value) : observe_ne(item, value);
    }
    static void observe_lt(TransProxy& item, T value, bool was_lt) {
        auto& p = get(item);
        if (was_lt)
            p.second = std::min(p.second, value - 1);
        else
            p.first = std::max(p.first, value);
        if (p.first > p.second)
            Sto::abort();
    }
    static void observe_le(TransProxy& item, T value, bool was_le) {
        auto& p = get(item);
        if (was_le)
            p.second = std::min(p.second, value);
        else
            p.first = std::max(p.first, value + 1);
        if (p.first > p.second)
            Sto::abort();
    }
    static bool discharge(pair_type pred, T value) {
        return pred.first <= value && value <= pred.second;
    }
    static void discharge_abort(TransProxy& item, T value) {
        if (!discharge(get(item), value))
            Sto::abort();
    }

private:
    version_type vers_;
    W v_;
};
