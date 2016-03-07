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
    void observe_eq(T value, bool was_eq) {
        if (was_eq) {
            first = std::max(first, value);
            second = std::min(second, value);
        } else if (value < first || second < value)
            /* do nothing */;
        else if (value - first <= second - value)
            first = value + 1;
        else
            second = value - 1;
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
};

template <typename T>
class TIntRangeSizeProxy {
public:
    typedef T size_type;
    typedef typename mass::make_signed<T>::type difference_type;

    TIntRangeSizeProxy(TIntRange<T>* r, size_type original, difference_type delta)
        : r_(r), original_(original), delta_(delta) {
    }

    operator size_type() const {
        // XXX this assumes that TransItem doesn't change location
        if (r_)
            r_->observe(original_);
        return original_ + delta_;
    }

    bool operator==(size_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_eq(x, original_ == x);
        return original_ == x;
    }
    bool operator!=(size_type x) const {
        return !(*this == x);
    }
    bool operator<(size_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_lt(x, original_ < x);
        return original_ < x;
    }
    bool operator<=(size_type x) const {
        x -= delta_;
        if (r_)
            r_->observe_le(x, original_ <= x);
        return original_ <= x;
    }
    bool operator>=(size_type x) const {
        return !(*this < x);
    }
    bool operator>(size_type x) const {
        return !(*this <= x);
    }

    // difference_type versions just for friendliness
    bool operator==(difference_type x) const {
        return *this == size_type(x);
    }
    bool operator!=(difference_type x) const {
        return *this != size_type(x);
    }
    bool operator<(difference_type x) const {
        return *this < size_type(x);
    }
    bool operator<=(difference_type x) const {
        return *this <= size_type(x);
    }
    bool operator>=(difference_type x) const {
        return *this >= size_type(x);
    }
    bool operator>(difference_type x) const {
        return *this > size_type(x);
    }

private:
    TIntRange<T>* r_;
    size_type original_;
    difference_type delta_;
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
            r_->observe_eq(x, original_ == size_type(x));
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

template <typename T>
bool operator==(T a, const TIntRangeSizeProxy<T>& b) {
    return b == a;
}
template <typename T>
bool operator!=(T a, const TIntRangeSizeProxy<T>& b) {
    return b != a;
}
template <typename T>
bool operator<(T a, const TIntRangeSizeProxy<T>& b) {
    return b > a;
}
template <typename T>
bool operator<=(T a, const TIntRangeSizeProxy<T>& b) {
    return b >= a;
}
template <typename T>
bool operator>=(T a, const TIntRangeSizeProxy<T>& b) {
    return b <= a;
}
template <typename T>
bool operator>(T a, const TIntRangeSizeProxy<T>& b) {
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
