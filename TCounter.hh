#pragma once
#include "TIntPredicate.hh"

template <typename T, typename W = TWrapped<T> >
class TCounter : public TObject {
    typedef TIntPredicate<T, W> ip_type;
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
    TCounter(T x)
        : v_(x) {
    }

    operator T() const {
        auto item = Sto::item(this, 0);
        T result = T();
        if (item.has_write())
            result += item.template write_value<T>();
        if (!item.has_flag(assigned_bit)) {
            result += v_.snapshot(vers_);
            ip_type::observe_eq(item, result);
        }
        if (item.has_flag(delta_bit))
            item.add_write(result).assign_flags(assigned_bit);
        return result;
    }
    TCounter<T, W>& operator=(T x) {
        auto item = Sto::item(this, 0);
        if (item.has_predicate()) {
            ip_type::discharge_abort(item, v_.read(item, vers_));
            item.clear_predicate();
        }
        item.add_write(x).assign_flags(assigned_bit);
        return *this;
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
        auto item = Sto::item(this, 0);
        if (item.has_flag(delta_bit))
            x -= item.template write_value<T>();
        else if (item.has_write())
            return item.template write_value<T>() == x;
        T result = v_.snapshot(vers_);
        ip_type::observe_eq(item, x, result == x);
        return result == x;
    }
    bool operator!=(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_flag(delta_bit))
            x -= item.template write_value<T>();
        else if (item.has_write())
            return item.template write_value<T>() != x;
        T result = v_.snapshot(vers_);
        ip_type::observe_eq(item, x, result == x);
        return result != x;
    }
    bool operator<(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_flag(delta_bit))
            x -= item.template write_value<T>();
        else if (item.has_write())
            return item.template write_value<T>() < x;
        T result = v_.snapshot(vers_);
        ip_type::observe_lt(item, x, result < x);
        return result < x;
    }
    bool operator<=(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_flag(delta_bit))
            x -= item.template write_value<T>();
        else if (item.has_write())
            return item.template write_value<T>() <= x;
        T result = v_.snapshot(vers_);
        ip_type::observe_le(item, x, result <= x);
        return result <= x;
    }
    bool operator>=(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_flag(delta_bit))
            x -= item.template write_value<T>();
        else if (item.has_write())
            return item.template write_value<T>() >= x;
        T result = v_.snapshot(vers_);
        ip_type::observe_lt(item, x, result < x);
        return result >= x;
    }
    bool operator>(T x) const {
        auto item = Sto::item(this, 0);
        if (item.has_flag(delta_bit))
            x -= item.template write_value<T>();
        else if (item.has_write())
            return item.template write_value<T>() > x;
        T result = v_.snapshot(vers_);
        ip_type::observe_le(item, x, result <= x);
        return result > x;
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
    bool lock(TransItem&, Transaction& txn) {
        return txn.try_lock(vers_);
    }
    virtual bool check_predicate(TransItem& item, Transaction& txn) {
        TransProxy p(txn, item);
        typename ip_type::pair_type pred = item.template predicate_value<typename ip_type::pair_type>();
        return ip_type::discharge(pred, v_.read(p, vers_));
    }
    virtual bool check(const TransItem& item, const Transaction&) {
        return item.check_version(vers_);
    }
    virtual void install(TransItem& item, const Transaction& txn) {
        T result = item.template write_value<T>();
        if (item.has_flag(delta_bit))
            result += v_.access();
        v_.write(result);
        vers_.set_version_unlock(txn.commit_tid());
        item.clear_needs_unlock();
    }
    virtual void unlock(TransItem&) {
        vers_.unlock();
    }
    virtual void print(std::ostream& w, const TransItem& item) const {
        w << "<Counter " << (void*) this << "=" << v_.access() << ".v" << vers_.value();
        if (item.has_read())
            w << " ?" << item.read_value<version_type>();
        if (item.has_write() && item.has_flag(delta_bit))
            w << " Δ" << item.template write_value<T>();
        else if (item.has_write())
            w << " =" << item.template write_value<T>();
        if (item.has_predicate()) {
            auto& p = item.predicate_value<pair_type>();
            w << " P[" << p.first << "," << p.second << "]";
        }
        w << ">";
    }

private:
    version_type vers_;
    W v_;
};
