#pragma once

#include <functional>

#include "../State.hh"

namespace storia {

template <
    typename V, typename B=V, bool U=std::is_same_v<void, B>>
    class Predicate;

template <typename V, typename B>
class Predicate<V, B, false> {
public:
    typedef V value_type;
    typedef B base_type;
    static constexpr bool Unary = false;

    typedef std::function<bool(const value_type&, const base_type&)> comp_type;

    Predicate(const comp_type& comparator, const base_type& base)
        : comp_(comparator), base_(base) {}

    bool eval(const value_type& value) const {
        return comp_(value, base_);
    }

private:
    const comp_type comp_;
    const base_type base_;
};

template <typename V>
class Predicate<V, void, true> {
public:
    typedef V value_type;
    typedef void base_type;
    static constexpr bool Unary = true;

    typedef std::function<bool(const value_type&)> comp_type;

    Predicate(const comp_type& comparator) : comp_(comparator) {}

    bool eval(const value_type& value) const {
        return comp_(value);
    }

private:
    const comp_type comp_;
};

class PredicateUtil {
public:
    template <typename V, typename B, typename P=Predicate<V, B, false>>
    static P Make(
            const typename P::comp_type& comparator, const B& base_value) {
        return P(comparator, base_value);
    }

    template <typename V, typename B=void, typename P=Predicate<V, void, true>>
    static P Make(
            const typename P::comp_type& comparator) {
        return P(comparator);
    }
};

};  // namespace storia
