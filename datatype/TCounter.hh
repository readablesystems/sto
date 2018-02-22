#pragma once
#include "TIntPredicate.hh"

template <typename T, typename W = TWrapped<T> >
class TCounter : public TObject {
    typedef TIntPredicate<T, W> ip_type;
    typedef typename ip_type::pred_type pred_type;
public:
    typedef typename W::version_type version_type;
    static constexpr TransItem::flags_type delta_bit = TransItem::user0_bit;
    static constexpr TransItem::flags_type assigned_bit = TransItem::user0_bit << 1;
    struct pair_type {
        T first;
        T second;
    };

    TCounter() {
    }
    explicit TCounter(T x)
        : v_(x) {
    }

    operator T() const {
        auto item = Sto::item(this, 0);
        T result = T();
        if (!item.has_flag(assigned_bit)) {
            result += v_.snapshot(item, vers_);
            get(item).observe(result);
        }
        if (item.has_write())
            result += item.template write_value<T>();
        return result;
    }

    TCounter<T, W>& operator=(T x) {
        Sto::item(this, 0).add_write(x).assign_flags(assigned_bit);
        return *this;
    }
    TCounter<T, W>& operator=(const TCounter<T, W>& x) {
        return *this = x.operator T();
    }
    template <typename TT, typename WW>
    TCounter<T, W>& operator=(const TCounter<TT, WW>& x) {
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

    TCounter<T, W>& operator+=(T delta) {
        auto item = Sto::item(this, 0);
        item.add_write(item.template write_value<T>(T()) + delta);
        if (!item.has_flag(assigned_bit))
            item.add_flags(delta_bit);
        return *this;
    }
    TCounter<T, W>& operator-=(T delta) {
        auto item = Sto::item(this, 0);
        item.add_write(item.template write_value<T>(T()) - delta);
        if (!item.has_flag(assigned_bit))
            item.add_flags(delta_bit);
        return *this;
    }
    TCounter<T, W>& operator++() {
        return *this += 1;
    }
    void operator++(int) {
        *this += 1;
    }
    TCounter<T, W>& operator--() {
        return *this -= 1;
    }
    void operator--(int) {
        *this -= 1;
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
        T result = item.template write_value<T>();
        if (item.has_flag(delta_bit))
            result += v_.access();
        v_.write(result);
        txn.set_version_unlock(vers_, item);
    }
    void unlock(TransItem& item) override {
        vers_.cp_unlock(item);
    }
    void print(std::ostream& w, const TransItem& item) const override {
        w << "{Counter " << (void*) this << "=" << v_.access() << ".v" << vers_.value();
        if (item.has_read())
            w << " R" << item.read_value<version_type>();
        if (item.has_write() && item.has_flag(delta_bit))
            w << " Δ" << item.template write_value<T>();
        else if (item.has_write())
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
        if (item.has_flag(assigned_bit))
            return item.template write_value<T>();
        else
            return v_.snapshot(item, vers_);
    }
    static T delta(TransProxy item) {
        return item.has_flag(delta_bit) ? item.template write_value<T>() : T();
    }
    bool observe_eq(TransProxy item, T value) const {
        value -= delta(item);
        T s = snapshot(item);
        if (!item.has_flag(assigned_bit))
            get(item).observe_test_eq(s, value);
        return s == value;
    }
    bool observe_lt(TransProxy item, T value) const {
        value -= delta(item);
        bool result = snapshot(item) < value;
        if (!item.has_flag(assigned_bit))
            get(item).observe_lt(value, result);
        return result;
    }
    bool observe_le(TransProxy item, T value) const {
        value -= delta(item);
        bool result = snapshot(item) <= value;
        if (!item.has_flag(assigned_bit))
            get(item).observe_le(value, result);
        return result;
    }
};


template <typename T, typename W>
bool operator==(T a, const TCounter<T, W>& b) {
    return b == a;
}
template <typename T, typename W>
bool operator!=(T a, const TCounter<T, W>& b) {
    return b != a;
}
template <typename T, typename W>
bool operator<(T a, const TCounter<T, W>& b) {
    return b > a;
}
template <typename T, typename W>
bool operator<=(T a, const TCounter<T, W>& b) {
    return b >= a;
}
template <typename T, typename W>
bool operator>=(T a, const TCounter<T, W>& b) {
    return b <= a;
}
template <typename T, typename W>
bool operator>(T a, const TCounter<T, W>& b) {
    return b < a;
}
