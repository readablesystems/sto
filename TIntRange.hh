#pragma once
#include <limits>
#include "compiler.hh"
#include "Transaction.hh"

template <typename T>
struct TIntRange {
    T first;
    T second;

    static TIntRange<T> unconstrained() {
        return TIntRange<T>{std::numeric_limits<T>::min(), std::numeric_limits<T>::max()};
    }
    void observe(T value) {
        if (first <= value && value <= second)
            first = second = value;
        else
            Sto::abort();
    }
    void observe_test_eq(T value, T comparison) {
        if (value == comparison) {
            first = std::max(first, value);
            second = std::min(second, value);
        } else if (value < comparison)
            second = std::min(second, comparison - 1);
        else
            first = std::max(first, comparison + 1);
        if (first > second)
            Sto::abort();
    }
    void observe_lt(T value, bool was_lt = true) {
        if (was_lt)
            second = std::min(second, value - 1);
        else
            first = std::max(first, value);
        if (first > second)
            Sto::abort();
    }
    void observe_le(T value, bool was_le = true) {
        if (was_le)
            second = std::min(second, value);
        else
            first = std::max(first, value + 1);
        if (first > second)
            Sto::abort();
    }
    void observe_gt(T value, bool was_gt = true) {
        observe_le(value, !was_gt);
    }
    void observe_ge(T value, bool was_ge = true) {
        observe_lt(value, !was_ge);
    }
    bool verify(T value) const {
        return first <= value && value <= second;
    }

    friend std::ostream& operator<<(std::ostream& w, const TIntRange<T>& r) {
        return w << '[' << r.first << ',' << r.second << ']';
    }
};

template <typename T, bool Signed = std::is_signed<T>::value>
class TIntRangeProxy;

template <typename T>
class TIntRangeProxy<T, false> {
public:
    typedef T value_type;
    typedef typename mass::make_signed<T>::type difference_type;

    TIntRangeProxy(TIntRange<T>* r, value_type original, difference_type delta)
        : r_(r), original_(original), delta_(delta) {
    }

    operator value_type() const {
        // XXX this assumes that TransItem doesn't change location
        if (r_)
            r_->observe(original_);
        return original_ + delta_;
    }

    bool operator==(value_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_test_eq(original_, x);
        return original_ == x;
    }
    bool operator!=(value_type x) const {
        return !(*this == x);
    }
    bool operator<(value_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_lt(x, original_ < x);
        return original_ < x;
    }
    bool operator<=(value_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_le(x, original_ <= x);
        return original_ <= x;
    }
    bool operator>=(value_type x) const {
        return !(*this < x);
    }
    bool operator>(value_type x) const {
        return !(*this <= x);
    }

    // difference_type versions just for friendliness
    bool operator==(difference_type x) const {
        return *this == value_type(x);
    }
    bool operator!=(difference_type x) const {
        return *this != value_type(x);
    }
    bool operator<(difference_type x) const {
        return *this < value_type(x);
    }
    bool operator<=(difference_type x) const {
        return *this <= value_type(x);
    }
    bool operator>=(difference_type x) const {
        return *this >= value_type(x);
    }
    bool operator>(difference_type x) const {
        return *this > value_type(x);
    }

private:
    TIntRange<T>* r_;
    value_type original_;
    difference_type delta_;
};

template <typename T>
class TIntRangeProxy<T, true> {
public:
    typedef T value_type;

    TIntRangeProxy(TIntRange<T>* r, value_type original, value_type delta)
        : r_(r), original_(original), delta_(delta) {
    }

    operator value_type() const {
        // XXX this assumes that TransItem doesn't change location
        if (r_)
            r_->observe(original_);
        return original_ + delta_;
    }

    bool operator==(value_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_test_eq(original_, x);
        return original_ == x;
    }
    bool operator!=(value_type x) const {
        return !(*this == x);
    }
    bool operator<(value_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_lt(x, original_ < x);
        return original_ < x;
    }
    bool operator<=(value_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_le(x, original_ <= x);
        return original_ <= x;
    }
    bool operator>=(value_type x) const {
        return !(*this < x);
    }
    bool operator>(value_type x) const {
        return !(*this <= x);
    }

private:
    TIntRange<T>* r_;
    value_type original_;
    value_type delta_;
};

template <typename T>
class TIntRangeDifferenceProxy {
public:
    typedef T size_type;
    typedef typename mass::make_signed<T>::type difference_type;

    TIntRangeDifferenceProxy(TIntRange<T>* r, size_type original, difference_type delta)
        : r_(r), original_(original), delta_(delta) {
    }

    operator difference_type() const {
        // XXX this assumes that TransItem doesn't change location
        if (r_)
            r_->observe(original_);
        return original_ + delta_;
    }

    bool operator==(difference_type x) const {
        x -= delta_;
        if (r_ && x >= 0)
            r_->observe_test_eq(original_, x);
        return x >= 0 && original_ == size_type(x);
    }
    bool operator!=(difference_type x) const {
        return !(*this == x);
    }
    bool operator<(difference_type x) const {
        x -= delta_;
        if (r_ && x >= 0)
            r_->observe_lt(x, original_ < size_type(x));
        return x >= 0 && original_ < size_type(x);
    }
    bool operator<=(difference_type x) const {
        x -= delta_;
        if (r_ && x >= 0)
            r_->observe_le(x, original_ <= size_type(x));
        return x >= 0 && original_ <= size_type(x);
    }
    bool operator>=(difference_type x) const {
        return !(*this < x);
    }
    bool operator>(difference_type x) const {
        return !(*this <= x);
    }

private:
    TIntRange<T>* r_;
    size_type original_;
    difference_type delta_;
};

template <typename T, bool Signed>
bool operator==(T a, const TIntRangeProxy<T, Signed>& b) {
    return b == a;
}
template <typename T, bool Signed>
bool operator!=(T a, const TIntRangeProxy<T, Signed>& b) {
    return b != a;
}
template <typename T, bool Signed>
bool operator<(T a, const TIntRangeProxy<T, Signed>& b) {
    return b > a;
}
template <typename T, bool Signed>
bool operator<=(T a, const TIntRangeProxy<T, Signed>& b) {
    return b >= a;
}
template <typename T, bool Signed>
bool operator>=(T a, const TIntRangeProxy<T, Signed>& b) {
    return b <= a;
}
template <typename T, bool Signed>
bool operator>(T a, const TIntRangeProxy<T, Signed>& b) {
    return b < a;
}

template <typename T>
bool operator==(typename TIntRangeDifferenceProxy<T>::difference_type a,
                const TIntRangeDifferenceProxy<T>& b) {
    return b == a;
}
template <typename T>
bool operator!=(typename TIntRangeDifferenceProxy<T>::difference_type a,
                const TIntRangeDifferenceProxy<T>& b) {
    return b != a;
}
template <typename T>
bool operator<(typename TIntRangeDifferenceProxy<T>::difference_type a,
               const TIntRangeDifferenceProxy<T>& b) {
    return b > a;
}
template <typename T>
bool operator<=(typename TIntRangeDifferenceProxy<T>::difference_type a,
                const TIntRangeDifferenceProxy<T>& b) {
    return b >= a;
}
template <typename T>
bool operator>=(typename TIntRangeDifferenceProxy<T>::difference_type a,
                const TIntRangeDifferenceProxy<T>& b) {
    return b <= a;
}
template <typename T>
bool operator>(typename TIntRangeDifferenceProxy<T>::difference_type a,
               const TIntRangeDifferenceProxy<T>& b) {
    return b < a;
}
